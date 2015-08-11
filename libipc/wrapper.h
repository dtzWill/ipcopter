//===-- wrapper.h -----------------------------------------------*- C++ -*-===//
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
// Wrapper-related macros and definitions
//
//===----------------------------------------------------------------------===//

#ifndef _WRAPPER_H_
#define _WRAPPER_H_

#include "getfromlibc.h"

#ifdef __cplusplus
#define BEGIN_EXTERN_C extern "C" {
#define END_EXTERN_C }
#else
#define BEGIN_EXTERN_C
#define END_EXTERN_C
#endif // __cplusplus

#define CALL_REAL(name, ...)                                                   \
  typedef typeof(name) *FPTY;                                                  \
  static FPTY FP = (FPTY)get_libc_func(#name);                                 \
  return FP(__VA_ARGS__);

#endif // _WRAPPER_H_
