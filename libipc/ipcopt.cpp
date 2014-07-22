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

ipc_info IpcDescTable[TABLE_SIZE] = {};

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
  i->ep = ep;
  i->valid = 1;
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
      __real_close(i->localfd);
    }
    assert(i->valid);
    i->valid = 0;
    i->ep = EP_INVALID;
  }
}



// TODO: Inline this
char is_registered_socket(int fd) {
  ipc_info *i = getFDDesc(fd);
  return i->valid;
}
