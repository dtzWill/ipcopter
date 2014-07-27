//===-- lock.h --------------------------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Simple lock class, uses external storage.
//
//===----------------------------------------------------------------------===//

#ifndef _LOCK_H_
#define _LOCK_H_

#include <stdint.h>
#include <sched.h>

// Using this to avoid pulling in pthread or others.
// Definitely not good for use outside this project.
class SimpleLock {
  volatile uint32_t __state;

  void Init() { __state = 0; }

public:
  SimpleLock() { Init(); }

  bool TryLock() { return __sync_lock_test_and_set(&__state, 1) == 0; }
  void Lock() {
    while (!TryLock()) {
      while (__state) {
        sched_yield();
      }
    }
  }

  void Unlock() { __sync_lock_release(&__state); }
};

class ScopedLock {
public:
  explicit ScopedLock(SimpleLock &L) : L(L) { L.Lock(); }

  ~ScopedLock() { L.Unlock(); }

private:
  SimpleLock &L;

  ScopedLock(const ScopedLock &);
  void operator=(const ScopedLock &);
};

#endif // _LOCK_H_
