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

int do_ipc_poll(struct pollfd fds[], nfds_t nfds, int timeout) {
  // For now, replace registered fd's with optimized versions
  const nfds_t MAX_POLL_FDS = 128;
  struct pollfd newfds[MAX_POLL_FDS];
  assert(nfds < MAX_POLL_FDS);

  memcpy(newfds, fds, sizeof(fds[0]) * nfds);

  for (nfds_t i = 0; i < nfds; ++i) {
    int fd = newfds[i].fd;
    if (!is_optimized_socket_safe(fd))
      continue;
    ipc_info *info = getFDDesc(fd);
    newfds[i].fd = info->localfd;
  }
  return __real_poll(newfds, nfds, timeout);
}
