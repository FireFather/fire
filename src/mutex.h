#pragma once
#include <condition_variable>
#include <mutex>

#if defined(_WIN32) && !defined(_MSC_VER)
#include <windows.h>

struct Mutex {
  Mutex() { InitializeCriticalSection(&cs); }
  ~Mutex() { DeleteCriticalSection(&cs); }
  void lock() { EnterCriticalSection(&cs); }
  void unlock() { LeaveCriticalSection(&cs); }
 private:
  CRITICAL_SECTION cs;
};

typedef std::condition_variable_any ConditionVariable;
#else
using Mutex = std::mutex;
using ConditionVariable = std::condition_variable;
#endif
