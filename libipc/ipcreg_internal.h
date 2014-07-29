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
#include "debug.h"

#include <assert.h>

typedef enum {
  STATE_INVALID = 0,
  STATE_UNOPT,
  STATE_ID_EXCHANGE,
  STATE_OPTIMIZED,
} EndpointState;

typedef struct {
  // Does this FD have an EP to go with it?
  endpoint EP;
  // Is it set to close-on-exec?
  bool close_on_exec;
  // Local?
  bool is_local;
} fd_info;

typedef struct {
  // Bytes transmitted through this endpoint
  size_t bytes_trans;
  // Does this endpoint have a local fd?
  int localfd;
  uint16_t ref_count;
  EndpointState state;
  // Non-blocking is descriptor-specific
  bool non_blocking;
} ipc_info;

// For now, just index directly into pre-allocate table with fd.
// We will also need a way to go from nonce to fd!
const unsigned TABLE_SIZE = 1 << 14;
typedef struct {
  fd_info FDMap[TABLE_SIZE];
  ipc_info EndpointInfo[TABLE_SIZE];
} libipc_state;

extern libipc_state state;

static inline char inbounds_fd(int fd) { return (unsigned)fd <= TABLE_SIZE; }
static inline char valid_ep(endpoint ep) { return (unsigned)ep <= TABLE_SIZE; }

static inline fd_info &getFDInfo(int fd) {
  if (!inbounds_fd(fd)) {
    ipclog("Attempt to access out-of-bounds fd: %d (TABLE_SIZE=%u)\n", fd,
           TABLE_SIZE);
  }
  assert(inbounds_fd(fd));

  return state.FDMap[fd];
}
static inline bool &is_local(int fd) {
  return getFDInfo(fd).is_local;
}

static inline endpoint &getEP(int fd) { return getFDInfo(fd).EP; }

static inline ipc_info &getInfo(endpoint ep) {
  assert(valid_ep(ep));
  return state.EndpointInfo[ep];
}

#endif // _IPCREG_INTERNAL_H_
