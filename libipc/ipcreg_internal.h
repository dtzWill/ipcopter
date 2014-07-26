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

typedef enum {
  STATE_INVALID = 0,
  STATE_UNOPT,
  STATE_ID_EXCHANGE,
  STATE_OPTIMIZED,
} EndpointState;

typedef struct {
  // Bytes transmitted through this endpoint
  size_t bytes_trans;
  // Does this endpoint have a local fd?
  int localfd;
  uint16_t ref_count;
  EndpointState state;
} ipc_info;

// For now, just index directly into pre-allocate table with fd.
// We will also need a way to go from nonce to fd!
const unsigned TABLE_SIZE = 1 << 10;
extern endpoint EPMap[TABLE_SIZE];
extern ipc_info EndpointInfo[TABLE_SIZE];
extern bool IsLocalFD[TABLE_SIZE];

static inline char inbounds_fd(int fd) { return (unsigned)fd <= TABLE_SIZE; }
static inline char valid_ep(endpoint ep) { return (unsigned)ep <= TABLE_SIZE; }
static inline bool &is_local(int fd) {
  assert(inbounds_fd(fd));
  return IsLocalFD[fd];
}

static inline endpoint &getEP(int fd) {
  if (!inbounds_fd(fd)) {
    ipclog("Attempt to access out-of-bounds fd: %d (TABLE_SIZE=%u)\n", fd,
           TABLE_SIZE);
  }
  assert(inbounds_fd(fd));
  return EPMap[fd];
}
static inline ipc_info &getInfo(endpoint ep) {
  assert(valid_ep(ep));
  return EndpointInfo[ep];
}

#endif // _IPCREG_INTERNAL_H_
