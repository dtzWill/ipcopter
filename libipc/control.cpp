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

#include <algorithm>

int do_ipc_shutdown(int sockfd, int how) {
  ipc_info &i = getInfo(getEP(sockfd));
  assert(i.state != STATE_INVALID);

  int ret = __real_shutdown(sockfd, how);

  // Do similar shutdown operation on local fd, if exists:
  if (i.state == STATE_OPTIMIZED) {
    assert(i.localfd);
    int localret = __real_shutdown(i.localfd, how);
    assert(localret == ret); // We don't handle mismatch yet
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
    newfds[i].fd = getInfo(getEP(fd)).localfd;
  }
  return __real_poll(newfds, nfds, timeout);
}

fd_set *copy_if_needed(fd_set *src, fd_set *copy, int nfds) {
  // Portable way to find fd's contained in the fd_set
  // More intrusive implementation would be much more efficient!

  int maxfd = std::min<int>(FD_SETSIZE, TABLE_SIZE);

  // NULL sets are easy :)
  if (src == NULL)
    return src;

  bool need_copy = false;
  int fds_found = 0;
  for (int fd = 0; fd < maxfd && fds_found < nfds; ++fd) {
    if (!FD_ISSET(fd, src))
      continue;
    ++fds_found;
    if (!is_optimized_socket_safe(fd))
      continue;

    need_copy = true;
  }

  if (!need_copy)
    return src;

  // Make second pass, copying into 'copy'
  // either original fd or the localfd for optimized connections.
  fds_found = 0;
  for (int fd = 0; fd < maxfd && fds_found < nfds; ++fd) {
    if (!FD_ISSET(fd, src))
      continue;
    ++fds_found;
    if (!is_optimized_socket_safe(fd)) {
      assert(!FD_ISSET(fd, copy));
      FD_SET(fd, copy);
    } else {
      int localfd = getInfo(getEP(fd)).localfd;
      assert(!FD_ISSET(fd, copy));
      FD_SET(localfd, copy);
    }
  }

  return copy;
}

int do_ipc_pselect(int nfds, fd_set *readfds, fd_set *writefds,
                   fd_set *errorfds, const struct timespec *timeout,
                   const sigset_t *sigmask) {
  assert(nfds >= 0);

  fd_set rcopy, wcopy, ecopy;

  fd_set *r = copy_if_needed(readfds, &rcopy, nfds);
  fd_set *w = copy_if_needed(writefds, &wcopy, nfds);
  fd_set *e = copy_if_needed(errorfds, &ecopy, nfds);

  return __real_pselect(nfds, r, w, e, timeout, sigmask);
}
int do_ipc_select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *errorfds,
                  struct timeval *timeout) {
  assert(nfds >= 0);

  fd_set rcopy, wcopy, ecopy;

  fd_set *r = copy_if_needed(readfds, &rcopy, nfds);
  fd_set *w = copy_if_needed(writefds, &wcopy, nfds);
  fd_set *e = copy_if_needed(errorfds, &ecopy, nfds);

  return __real_select(nfds, r, w, e, timeout);
}
