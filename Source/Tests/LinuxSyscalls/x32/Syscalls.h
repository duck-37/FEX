#pragma once

#include "Tests/LinuxSyscalls/FileManagement.h"
#include <FEXCore/HLE/SyscallHandler.h>
#include "Tests/LinuxSyscalls/x32/Types.h"

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <unordered_map>

namespace FEX::HLE {
class SignalDelegator;
class SyscallHandler;
}

namespace FEXCore::Core {
struct InternalThreadState;
}

namespace FEX::HLE::x32 {
#include "SyscallsEnum.h"

FEX::HLE::SyscallHandler *CreateHandler(FEXCore::Context::Context *ctx, FEX::HLE::SignalDelegator *_SignalDelegation);
void RegisterSyscallInternal(int SyscallNumber,
#ifdef DEBUG_STRACE
  const std::string& TraceFormatString,
#endif
  void* SyscallHandler, int ArgumentCount);

//////
// REGISTER_SYSCALL_IMPL implementation
// Given a syscall name + a lambda, and it will generate an strace string, extract number of arguments
// and register it as a syscall handler
//////

// RegisterSyscall base
// Deduces return, args... from the function passed
// Does not work with lambas, because they are objects with operator (), not functions
template<typename R, typename ...Args>
bool RegisterSyscall(int SyscallNumber, const char *Name, R(*fn)(FEXCore::Core::InternalThreadState *Thread, Args...)) {
#ifdef DEBUG_STRACE
  auto TraceFormatString = std::string(Name) + "(" + CollectArgsFmtString<Args...>() + ") = %ld";
#endif
  FEX::HLE::x32::RegisterSyscallInternal(SyscallNumber,
#ifdef DEBUG_STRACE
    TraceFormatString,
#endif
    reinterpret_cast<void*>(fn), sizeof...(Args));
  return true;
}

//LambdaTraits extracts the function singature of a lambda from operator()
template<typename FPtr>
struct LambdaTraits;

template<typename T, typename C, typename ...Args>
struct LambdaTraits<T (C::*)(Args...) const>
{
    typedef T(*Type)(Args...);
};

// Generic RegisterSyscall for lambdas
// Non-capturing lambdas can be cast to function pointers, but this does not happen on argument matching
// This is some glue logic that will cast a lambda and call the base RegisterSyscall implementation
template<class F>
bool RegisterSyscall(int num, const char *name, F f){
  typedef typename LambdaTraits<decltype(&F::operator())>::Type Signature;
  return RegisterSyscall(num, name, (Signature)f);
}

}

// Helpers to register a syscall implementation
// Creates a syscall forward from a glibc wrapper, and registers it
#define REGISTER_SYSCALL_FORWARD_ERRNO_X32(function) do { RegisterSyscall(x32::SYSCALL_x86_##function, #function, SYSCALL_FORWARD_ERRNO(function)); } while(0)

// Registers syscall for 32bit only
#define REGISTER_SYSCALL_IMPL_X32(name, lambda) \
  struct impl_##name { \
    impl_##name() \
    { \
      FEX::HLE::x32::RegisterSyscall(x32::SYSCALL_x86_##name, #name, lambda); \
    } } impl_##name
