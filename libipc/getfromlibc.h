//===-- getfromlibc.h -----------------------------------------------------===//
//
// Slipstream: Automatic Interprocess Communication Optimization
//
// Copyright (c) 2015, Will Dietz <w@wdtz.org>
// This file is distributed under the ISC license, see LICENSE for details.
//
// http://wdtz.org/slipstream
//
//===----------------------------------------------------------------------===//
//
// Obtain pointer to libc-defined version of specified function.
//
//===----------------------------------------------------------------------===//
#ifndef _GET_FROM_LIBC_
#define _GET_FROM_LIBC_

#include <assert.h>
#include <dlfcn.h>
#include <stdint.h>

static void *libc_handle = 0;

inline static void *get_libc() {
  if (!libc_handle)
    libc_handle = dlopen("libc.so.6", RTLD_LAZY);
  return libc_handle;
}
inline static uintptr_t get_libc_func(const char *name) {
  void *p = dlsym(get_libc(), name);
  assert(p);
  return uintptr_t(p);
}

#endif // _GET_FROM_LIBC_
