//===-- ipcreg_internal.h -------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Internal types and data for IPC registration logic
//
//===----------------------------------------------------------------------===//

#ifndef _IPCREG_INTERNAL_H_
#define _IPCREG_INTERNAL_H_

#include "ipcd.h"

#include <assert.h>

typedef struct {
  size_t bytes_trans;
  int localfd;
  endpoint ep;
  char valid;
  char local;
  // More data (key, handle to ipc channel, etc.)
} ipc_info;

// For now, just index directly into pre-allocate table with fd.
// We will also need a way to go from nonce to fd!
const unsigned TABLE_SIZE = 1 << 10;
extern ipc_info IpcDescTable[TABLE_SIZE];

static inline char inbounds_fd(int fd) {
  return (unsigned)fd <= TABLE_SIZE;
}

static inline ipc_info *getFDDesc(int fd) {
  assert(inbounds_fd(fd));
  return &IpcDescTable[fd];
}

#endif // _IPCREG_INTERNAL_H_
