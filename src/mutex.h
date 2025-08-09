#pragma once
#include <condition_variable>
#include <mutex>

// This header defines a portable mutex and condition variable abstraction.
//
// For most platforms (POSIX, MSVC on Windows), it simply aliases std::mutex
// and std::condition_variable. For MinGW or other non-MSVC builds on Windows,
// it implements a mutex wrapper around Win32 CRITICAL_SECTION to avoid
// compatibility issues with std::mutex on those toolchains.

#if defined(_WIN32) && !defined(_MSC_VER)
#include <windows.h>

// -----------------------------------------------------------------------------
// std_mutex: wrapper around Win32 CRITICAL_SECTION
// -----------------------------------------------------------------------------
//
// Provides the same lock() and unlock() interface as std::mutex.
// CRITICAL_SECTION is lighter weight than a Win32 mutex and works well for
// intra-process locking.
//
// Note:
// - CRITICAL_SECTION must be initialized and deleted explicitly.
// - It is recursive only if initialized as such (not in this wrapper).
// - lock() will block until the mutex becomes available.
// - unlock() releases the mutex so another thread can acquire it.
struct std_mutex {
  std_mutex() { InitializeCriticalSection(&cs); }
  ~std_mutex() { DeleteCriticalSection(&cs); }
  void lock() { EnterCriticalSection(&cs); }
  void unlock() { LeaveCriticalSection(&cs); }
private:
  CRITICAL_SECTION cs; // underlying Win32 primitive
};

// Alias condition_variable_any to cond_var for cross-platform code.
// condition_variable_any can work with any BasicLockable, including our std_mutex.
typedef std::condition_variable_any cond_var;

#else

// For non-Windows or MSVC builds, use standard types directly.
using std_mutex=std::mutex;
using cond_var=std::condition_variable;

#endif
