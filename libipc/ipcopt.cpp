//===-- ipcopt.c ----------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// IPC Optimization Central Runtime Management
//
//===----------------------------------------------------------------------===//

#include "debug.h"
#include "ipcopt.h"
#include "ipcreg_internal.h"
#include "ipcd.h"
#include "socket_inline.h"

#include <assert.h>
#include <stdio.h>
#include <unistd.h>

// TODO: This table is presently not used thread-safe at all!
ipc_info IpcDescTable[TABLE_SIZE] = {};

void invalidate(ipc_info *i) {
  assert(i->valid);
  // Mark as invalid
  i->valid = false;
  // And invalidate the data it contains for good measure
  i->bytes_trans = 0;
  i->localfd = 0;
  i->ep = EP_INVALID;
  i->state = STATE_UNOPT;
}

void register_inet_socket(int fd) {
  ipclog("Registering socket fd=%d\n", fd);
  ipc_info *i = getFDDesc(fd);

  // Gracefully handle duplicate registrations?
  if (i->valid) {
    ipclog("Already registered fd %d?? Ignoring...\n", fd);
    return;
  }
  assert(!i->valid);
  endpoint ep = ipcd_register_socket(fd);
  assert(ep != EP_INVALID);
  // Initialize!
  i->bytes_trans = 0;
  i->localfd = 0;
  i->ep = ep;
  i->state = STATE_UNOPT;
  i->valid = true;
}

void unregister_inet_socket(int fd) {
  if (!inbounds_fd(fd)) {
    return;
  }
  // ipclog("Unregistering socket fd=%d\n", fd);
  ipc_info *i = getFDDesc(fd);
  if (i->valid) {
    ipclog("Unregistering socket fd=%d, bytes transferred: %zu\n", fd,
           i->bytes_trans);
    assert(i->ep != EP_INVALID);
    bool success = ipcd_unregister_socket(i->ep);
    if (!success) {
      ipclog("ipcd_unregister_socket(%d) failed!\n", i->ep);
      return;
    }
    assert(success);
    // TODO: Should IPCD do this? *Can* it?
    if (i->localfd) {
      assert(i->state == STATE_OPTIMIZED);
      __real_close(i->localfd);

      // Remove entry for local fd
      ipc_info *li = getFDDesc(i->localfd);
      assert(li->state == STATE_LOCALFD);
      invalidate(li);
    }
    invalidate(i);
  }
}

char is_registered_socket(int fd) {
  ipc_info *i = getFDDesc(fd);
  return i->valid && (i->state != STATE_LOCALFD);
}

int getlocalfd(int fd) {
  assert(is_registered_socket(fd));
  int local = ipcd_getlocalfd(fd);

  // Create entry in fd table indicating this
  // fd is being used for optimized transport.
  ipc_info *i = getFDDesc(local);
  i->bytes_trans = 0;
  i->localfd = 0;
  i->ep = EP_INVALID;
  i->state = STATE_LOCALFD;
  i->valid = true;

  return local;
}

char is_protected_fd(int fd) {
  // Logging fd is protected
  if (FILE *logfp = getlogfp())
    if (int logfd = fileno(logfp))
      if (logfd == fd)
        return true;

  // So are all local fd's:
  ipc_info *i = getFDDesc(fd);
  if (i->valid && i->state == STATE_LOCALFD)
    return true;

  // Check for ipcd protected sockets:
  if (ipcd_is_protected(fd))
    return true;

  // Everything else is unprotected
  return false;
}
