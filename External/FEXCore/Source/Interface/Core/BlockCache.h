#pragma once
#include "Interface/Context/Context.h"
#include <FEXCore/Utils/LogManager.h>

namespace FEXCore {
class BlockCache {
public:

  struct BlockCacheEntry { 
    uintptr_t HostCode;
    uintptr_t GuestCode;
  };

  BlockCache(FEXCore::Context::Context *CTX);
  ~BlockCache();

  using BlockCacheIter = uintptr_t;
  uintptr_t End() { return 0; }

  uintptr_t FindBlock(uint64_t Address) {
    return FindCodePointerForAddress(Address);
  }

  void Erase(uint64_t Address) {
    // Do L1
    auto &L1Entry = reinterpret_cast<BlockCacheEntry*>(L1Pointer)[Address & L1_ENTRIES_MASK];
    if (L1Entry.GuestCode == Address) {
      L1Entry.GuestCode = L1Entry.HostCode = 0;
    }

    // Do full map
    Address = Address & (VirtualMemSize -1);
    uint64_t PageOffset = Address & (0x0FFF);
    Address >>= 12;

    uintptr_t *Pointers = reinterpret_cast<uintptr_t*>(PagePointer);
    uint64_t LocalPagePointer = Pointers[Address];
    if (!LocalPagePointer) {
      // Page for this code didn't even exist, nothing to do
      return;
    }

    // Page exists, just set the offset to zero
    auto BlockPointers = reinterpret_cast<BlockCacheEntry*>(LocalPagePointer);
    BlockPointers[PageOffset].GuestCode = 0;
    BlockPointers[PageOffset].HostCode = 0;
  }

  uintptr_t AddBlockMapping(uint64_t Address, void *Ptr) { 
    // Do L1
    auto &L1Entry = reinterpret_cast<BlockCacheEntry*>(L1Pointer)[Address & L1_ENTRIES_MASK];
    if (L1Entry.GuestCode == Address) {
      L1Entry.GuestCode = L1Entry.HostCode = 0;
    }

    // Do ful map
    auto FullAddress = Address;
    Address = Address & (VirtualMemSize -1);

    uint64_t PageOffset = Address & (0x0FFF);
    Address >>= 12;
    uintptr_t *Pointers = reinterpret_cast<uintptr_t*>(PagePointer);
    uint64_t LocalPagePointer = Pointers[Address];
    if (!LocalPagePointer) {
      // We don't have a page pointer for this address
      // Allocate one now if we can
      uintptr_t NewPageBacking = AllocateBackingForPage();
      if (!NewPageBacking) {
        // Couldn't allocate, return so the frontend can recover from this
        return 0;
      }
      Pointers[Address] = NewPageBacking;
      LocalPagePointer = NewPageBacking;
    }

    // Add the new pointer to the page block
    auto BlockPointers = reinterpret_cast<BlockCacheEntry*>(LocalPagePointer);
    uintptr_t CastPtr = reinterpret_cast<uintptr_t>(Ptr);

    // This silently replaces existing mappings
    BlockPointers[PageOffset].GuestCode = FullAddress;
    BlockPointers[PageOffset].HostCode = CastPtr;

    return CastPtr;
  }

  void ClearCache();

  void HintUsedRange(uint64_t Address, uint64_t Size);

  uintptr_t GetL1Pointer() { return L1Pointer; }
  uintptr_t GetPagePointer() { return PagePointer; }
  uintptr_t GetVirtualMemorySize() const { return VirtualMemSize; }

  constexpr static size_t L1_ENTRIES = 1 * 1024 * 1024; // Must be a power of 2
  constexpr static size_t L1_ENTRIES_MASK = L1_ENTRIES - 1;

private:
  uintptr_t AllocateBackingForPage() {
    uintptr_t NewBase = AllocateOffset;
    uintptr_t NewEnd = AllocateOffset + SIZE_PER_PAGE;

    if (NewEnd >= CODE_SIZE) {
      // We ran out of block backing space. Need to clear the block cache and tell the JIT cores to clear their caches as well
      // Tell whatever is calling this that it needs to do it.
      return 0;
    }

    AllocateOffset = NewEnd;
    return PageMemory + NewBase;
  }

  uintptr_t FindCodePointerForAddress(uint64_t Address) {
    
    // Do L1
    auto &L1Entry = reinterpret_cast<BlockCacheEntry*>(L1Pointer)[Address & L1_ENTRIES_MASK];
    if (L1Entry.GuestCode == Address) {
      return L1Entry.HostCode;
    }

    auto FullAddress = Address;
    Address = Address & (VirtualMemSize -1);

    uint64_t PageOffset = Address & (0x0FFF);
    Address >>= 12;
    uintptr_t *Pointers = reinterpret_cast<uintptr_t*>(PagePointer);
    uint64_t LocalPagePointer = Pointers[Address];
    if (!LocalPagePointer) {
      // We don't have a page pointer for this address
      return 0;
    }

    // Find there pointer for the address in the blocks
    auto BlockPointers = reinterpret_cast<BlockCacheEntry*>(LocalPagePointer);

    if (BlockPointers[PageOffset].GuestCode == FullAddress)
    {
      L1Entry.GuestCode = FullAddress;
      return L1Entry.HostCode = BlockPointers[PageOffset].HostCode;
    }
    else
      return 0;
  }

  uintptr_t PagePointer;
  uintptr_t PageMemory;
  uintptr_t L1Pointer;

  constexpr static size_t CODE_SIZE = 128 * 1024 * 1024;
  constexpr static size_t SIZE_PER_PAGE = 4096 * sizeof(BlockCacheEntry);
  constexpr static size_t L1_SIZE = L1_ENTRIES * sizeof(BlockCacheEntry);

  size_t AllocateOffset {};

  FEXCore::Context::Context *ctx;
  uint64_t VirtualMemSize{};
};
}
