//===-- control.cpp -------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Control-related operations for registered sockets
//
//===----------------------------------------------------------------------===//

#include "ipcopt.h"

#include "debug.h"
#include "ipcreg_internal.h"
#include "socket_inline.h"
#include "ipcd.h"

int do_ipc_shutdown(int sockfd, int how) {
  ipc_info *i = getFDDesc(sockfd);
  assert(i->valid);

  int ret = __real_shutdown(sockfd, how);

  // Do similar shutdown operation on local fd, if exists:
  if (i->state == STATE_OPTIMIZED) {
    assert(i->localfd);
    int localret = __real_shutdown(i->localfd, how);
    assert(localret == ret);
  }

  return ret;
}
