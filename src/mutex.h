#pragma once
#include <condition_variable>
#include <mutex>

#if defined(_WIN32) && !defined(_MSC_VER)
#include <windows.h>

struct std_mutex {
  std_mutex() { InitializeCriticalSection(&cs); }
  ~std_mutex() { DeleteCriticalSection(&cs); }
  void lock() { EnterCriticalSection(&cs); }
  void unlock() { LeaveCriticalSection(&cs); }
private:
  CRITICAL_SECTION cs;
};

typedef std::condition_variable_any cond_var;
#else
using std_mutex=std::mutex;
using cond_var=std::condition_variable;
#endif
