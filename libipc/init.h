//===-- init.h ------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
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
