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
#include <sys/epoll.h>

#include <boost/crc.hpp>

const unsigned TABLE_SIZE = 1 << 10;
// EPOLL size here is conservative to avoid
// bloating our table.  This might break some
// epoll programs, but the fix isn't bumping
// this back up but rather to store epoll
// information elsewhere--statically allocating
// maximum possible memory requirements is silly
// and is causing us problems.
const unsigned MAX_EPOLL_ENTRIES = 5;

enum EndpointState {
  STATE_INVALID = 0,
  STATE_UNOPT,
  STATE_ID_EXCHANGE,
  STATE_OPTIMIZED,
};

struct epoll_entry {
  int fd;
  epoll_event event;
};

struct epoll_info {
  bool valid;
  unsigned count;
  epoll_entry entries[MAX_EPOLL_ENTRIES];
};

struct fd_info {
  // Does this FD have an EP to go with it?
  endpoint EP;
  // Is it set to close-on-exec?
  bool close_on_exec;
  // Local?
  bool is_local;
  // epoll information, if applicible...
  epoll_info epoll;

  fd_info() : EP(EP_INVALID), close_on_exec(false), is_local(false), epoll() {}
};

struct ipc_info {
  // Bytes transmitted through this endpoint
  size_t bytes_sent;
  size_t bytes_recv;
  boost::crc_32_type crc_sent;
  boost::crc_32_type crc_recv;
  // Does this endpoint have a local fd?
  int localfd;
  uint16_t ref_count;
  EndpointState state;
  // Non-blocking is descriptor-specific
  bool non_blocking;

  ipc_info() { reset(); }
  void reset() {
    bytes_sent = 0;
    bytes_recv = 0;
    crc_sent.reset();
    crc_recv.reset();
    localfd = 0;
    ref_count = 0;
    state = STATE_INVALID;
    non_blocking = false;
  }
};

struct libipc_state {
  fd_info FDMap[TABLE_SIZE];
  ipc_info EndpointInfo[TABLE_SIZE];
  libipc_state();
};

extern libipc_state state;

static inline char inbounds_fd(int fd) { return (unsigned)fd < TABLE_SIZE; }
static inline char valid_ep(endpoint ep) { return (unsigned)ep < TABLE_SIZE; }

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

static inline epoll_info &getEpollInfo(int fd) { return getFDInfo(fd).epoll; }

static inline endpoint &getEP(int fd) { return getFDInfo(fd).EP; }

static inline ipc_info &getInfo(endpoint ep) {
  assert(valid_ep(ep));
  return state.EndpointInfo[ep];
}

#endif // _IPCREG_INTERNAL_H_
