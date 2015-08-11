//===-- init.h ------------------------------------------------------------===//
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
// Ensure proper initialization.
//
//===----------------------------------------------------------------------===//
#ifndef _INIT_H_
#define _INIT_H_

bool ensureInit();
void __ensure_init_started();

// Ensure init is run as ctor in this TU.
static void __attribute__((constructor)) __ipc_init_ctor() {
  __ensure_init_started();
}

#endif // _INIT_H_
