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
#include "ipcd.h"
#include "ipcreg_internal.h"
#include "real.h"

#include <algorithm>
#include <fcntl.h>
#include <string.h>

int do_ipc_shutdown(int sockfd, int how) {
  ipc_info &i = getInfo(getEP(sockfd));
  assert(i.state != STATE_INVALID);

  int ret = __real_shutdown(sockfd, how);

  // Do similar shutdown operation on local fd, if exists:
  if (i.state == STATE_OPTIMIZED) {
    assert(i.localfd);
    int localret = __real_shutdown(i.localfd, how);
    if (localret == 0 && ret == -1) {
      ipclog("shutdown(%d, %d) error, local succeeded\n", sockfd, how);
    }
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
  int ret = __real_poll(newfds, nfds, timeout);

  // Copy 'revents' back out from newfds,
  // as written by 'poll':
  for (nfds_t i = 0; i < nfds; ++i) {
    fds[i].revents = newfds[i].revents;
  }

  return ret;
}

fd_set *copy_if_needed(fd_set *src, fd_set *copy, int &nfds) {
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

  FD_ZERO(copy);

  // Make second pass, copying into 'copy'
  // either original fd or the localfd for optimized connections.
  fds_found = 0;
  int orig_nfds = nfds;
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
      // 'nfds' needs to be value of largest fd plus 1
      nfds = std::max(localfd+1, nfds);
    }
  }

  if (false) {
    if (nfds != orig_nfds)
      ipclog("In handling call to (p)select(), increased nfds: %d -> %d\n",
             orig_nfds, nfds);
  }

  return copy;
}

void select__copy_to_output(fd_set *out, fd_set*givenout, int nfds) {
  // If they're the same, output was already written to givenout
  if (out == givenout)
    return;

  int maxfd = std::min<int>(FD_SETSIZE, TABLE_SIZE);

  // For each fd set in 'givenout', check if
  // it or its optimized local version were set
  // by select in the given 'out' set.
  int fds_found = 0;
  for (int fd = 0; fd < maxfd && fds_found < nfds; ++fd) {
    if (!FD_ISSET(fd, givenout))
      continue;
    ++fds_found;
    int equiv_fd = fd;
    if (is_optimized_socket_safe(fd))
      equiv_fd = getInfo(getEP(fd)).localfd;

    // If the equivalent (local if exists) fd
    // was not marked by select(), clear it here as well:
    if (!FD_ISSET(equiv_fd, out))
      FD_CLR(fd, givenout);
  }
}

int do_ipc_pselect(int nfds, fd_set *readfds, fd_set *writefds,
                   fd_set *errorfds, const struct timespec *timeout,
                   const sigset_t *sigmask) {
  assert(nfds >= 0);

  fd_set rcopy, wcopy, ecopy;

  fd_set *r = copy_if_needed(readfds, &rcopy, nfds);
  fd_set *w = copy_if_needed(writefds, &wcopy, nfds);
  fd_set *e = copy_if_needed(errorfds, &ecopy, nfds);

  int ret = __real_pselect(nfds, r, w, e, timeout, sigmask);
  select__copy_to_output(r, readfds, nfds);
  select__copy_to_output(w, writefds, nfds);
  select__copy_to_output(e, errorfds, nfds);

  return ret;
}
int do_ipc_select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *errorfds,
                  struct timeval *timeout) {
  assert(nfds >= 0);

  fd_set rcopy, wcopy, ecopy;

  fd_set *r = copy_if_needed(readfds, &rcopy, nfds);
  fd_set *w = copy_if_needed(writefds, &wcopy, nfds);
  fd_set *e = copy_if_needed(errorfds, &ecopy, nfds);

  int ret = __real_select(nfds, r, w, e, timeout);

  select__copy_to_output(r, readfds, nfds);
  select__copy_to_output(w, writefds, nfds);
  select__copy_to_output(e, errorfds, nfds);

  return ret;
}

void set_local_nonblocking(int fd, bool nonblocking) {
  ipc_info & i = getInfo(getEP(fd));
  assert(i.state == STATE_OPTIMIZED);
  assert(i.localfd);

  int flags = __real_fcntl(i.localfd, F_GETFL, /* kludge*/ 0);
  assert(flags >= 0);
  if (nonblocking)
    flags |= O_NONBLOCK;
  else
    flags &= ~O_NONBLOCK;
  int ret = __real_fcntl_int(i.localfd, F_SETFL, flags);
  assert(ret != -1);
}

int do_ipc_fcntl(int fd, int cmd, void *arg) {
  int ret = __real_fcntl(fd, cmd, arg);

  if (ret == -1) {
    // Ignore erroneous fcntl commands
    return ret;
  }

  int iarg = (int)(uintptr_t)arg; // XXX: Desired casting behavior?
  switch (cmd) {
  case F_SETFD:
    // Setting fd options
    set_cloexec(fd, (iarg & FD_CLOEXEC) != 0);
    break;
  case F_SETFL: {
    // Setting description/endpoint options
    bool non_blocking = (iarg & O_NONBLOCK) != 0;
    set_nonblocking(fd, non_blocking);
    if (is_optimized_socket_safe(fd)) {
      // Ensure localfd has same non-blocking features
      set_local_nonblocking(fd, non_blocking);
    }
    break;
  }
  default:
    // Ignore for now.
    break;
  }

  return ret;
}
int do_ipc_setsockopt(int socket, int level, int option_name,
                      const void *option_value, socklen_t option_len) {
  int ret = __real_setsockopt(socket, level, option_name, option_value, option_len);

  // TODO: ... what options do we care about?

  return ret;
}
