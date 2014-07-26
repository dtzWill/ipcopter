//===-- socket_inline.h -----------------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Internal implementation of socket API for use in
// the various interception forms.
//
//===----------------------------------------------------------------------===//

#ifndef _SOCKET_INLINE_H_
#define _SOCKET_INLINE_H_

#include "debug.h"
#include "ipcopt.h"
#include "real.h"

#include <assert.h>

#include <errno.h>
#include <linux/ip.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static inline int __internal_socket(int domain, int type, int protocol) {
  int fd = __real_socket(domain, type, protocol);

  bool ip_domain = (domain == AF_INET) || (domain == AF_INET6);
  bool stream_sock = (type & SOCK_STREAM) != 0;
  bool tcp = ip_domain && stream_sock;
  bool valid_fd = (fd != -1);
  if (tcp && valid_fd) {
    register_inet_socket(fd);
    set_nonblocking(fd, (type & SOCK_NONBLOCK) != 0);
    set_cloexec(fd, (type & SOCK_CLOEXEC) != 0);
  }
  return fd;
}

static inline ssize_t __internal_recv(int fd, void *buf, size_t count,
                                      int flags) {
  ssize_t ret;
  if (!is_registered_socket(fd))
    ret = __real_recv(fd, buf, count, flags);
  else
    ret = do_ipc_recv(fd, buf, count, flags);
  return ret;
}

static inline ssize_t __internal_send(int fd, const void *buf, size_t count,
                                      int flags) {
  ssize_t ret;
  if (!is_registered_socket(fd))
    ret = __real_send(fd, buf, count, flags);
  else
    ret = do_ipc_send(fd, buf, count, flags);
  return ret;
}
static inline ssize_t __internal_recvfrom(int fd, void *buffer, size_t length,
                                          int flags, struct sockaddr *address,
                                          socklen_t *address_len) {
  ssize_t ret;
  if (!is_registered_socket(fd))
    ret = __real_recvfrom(fd, buffer, length, flags, address, address_len);
  else
    ret = do_ipc_recvfrom(fd, buffer, length, flags, address, address_len);
  return ret;
}

static inline ssize_t __internal_sendto(int fd, const void *message,
                                        size_t length, int flags,
                                        const struct sockaddr *dest_addr,
                                        socklen_t dest_len) {
  ssize_t ret;
  if (!is_registered_socket(fd))
    ret = __real_sendto(fd, message, length, flags, dest_addr, dest_len);
  else
    ret = do_ipc_sendto(fd, message, length, flags, dest_addr, dest_len);
  return ret;
}

static inline ssize_t __internal_read(int fd, void *buf, size_t count) {
  ssize_t ret;
  if (!is_registered_socket(fd))
    ret = __real_read(fd, buf, count);
  else
    ret = do_ipc_recv(fd, buf, count, 0);
  return ret;
}
static inline ssize_t __internal_write(int fd, const void *buf, size_t count) {
  ssize_t ret;
  if (!is_registered_socket(fd))
    ret = __real_write(fd, buf, count);
  else
    ret = do_ipc_send(fd, buf, count, 0);
  return ret;
}

static inline int __internal_accept(int fd, struct sockaddr *addr,
                                    socklen_t *addrlen) {
  int ret = __real_accept(fd, addr, addrlen);
  if (is_registered_socket(fd)) {
    ipclog("accept(%d) -> %d\n", fd, ret);
    if (ret != -1)
      register_inet_socket(ret);
  }
  return ret;
}

static inline int __internal_bind(int fd, const struct sockaddr *addr,
                                  socklen_t addrlen) {
  int ret = __real_bind(fd, addr, addrlen);
  return ret;
}

static inline int __internal_connect(int fd, const struct sockaddr *addr,
                                     socklen_t addrlen) {
  int ret = __real_connect(fd, addr, addrlen);
  return ret;
}

static inline int __internal_listen(int fd, int backlog) {
  int ret = __real_listen(fd, backlog);
  return ret;
}

static inline int __internal_close(int fd) {
  if (fd == -1) {
    ipclog("close(-1)??\n");
    return 0;
  }
  if (is_protected_fd(fd)) {
    ipclog("Attempt to close protected fd '%d', ignoring\n", fd);
    return 0;
  }
  int ret = __real_close(fd);

  unregister_inet_socket(fd);

  return ret;
}


static inline void __internal_closefrom(int lowfd) {
  // XXX
  // Actual closefrom implementation looks
  // at /proc/$PID/fd/ and just closes fd's it finds
  // there larger than given value.
  // It handles all kinds of other details as well...
  // TODO: Improve our implementation!
  // Misc useful reference impl:
  // https://code.google.com/p/flowd/source/browse/closefrom.c?name=FLOWD_0_8_5
  long maxfd = sysconf(_SC_OPEN_MAX);

  for (long fd = lowfd; fd < maxfd; ++fd)
    (void)close((int)fd);
}

static inline int __internal_shutdown(int sockfd, int how) {
  if (FILE *logfp = getlogfp()) {
    if (int logfd = fileno(logfp)) {
      if (logfd == sockfd) {
        ipclog("Attempt to shutdown our logging fd, ignoring!\n");
        return 0;
      }
    }
  }

  // TODO: Hmm, what should be done here?

  if (is_registered_socket(sockfd))
    return do_ipc_shutdown(sockfd, how);
  else
    return __real_shutdown(sockfd, how);
}

static inline int __internal_fcntl(int fd, int cmd, void *arg) {
  // TODO: do something
  if (is_protected_fd(fd)) {
    ipclog("Attempting fcntl(fd=%d, cmd=%d, arg=%p) for protected fd!\n", fd,
           cmd, arg);
  }

  // TODO: Handle F_DUPFD and friends.

  int ret;
  if (!is_registered_socket(fd))
    ret = __real_fcntl(fd, cmd, arg);
  else
    ret = do_ipc_fcntl(fd, cmd, arg);

  return ret;
}

static inline int __internal_dup(int fd) {
  // ipclog("dup(%d) called\n", fd);
  if (is_protected_fd(fd)) {
    ipclog("Attempt to dup protected fd '%d'?\n", fd);
  }
  assert(!is_protected_fd(fd));
  int ret = __real_dup(fd);

  if (is_registered_socket(fd)) {
    dup_inet_socket(fd, ret);
  }
  
  return ret;
}
static inline int __internal_dup2(int fd1, int fd2) {
  // ipclog("dup(%d, %d) called\n", fd1, fd2);
  assert(!is_protected_fd(fd1) && "Application attempted to dup protected fd");
  if (is_protected_fd(fd2)) {
    ipclog("Attempting dup2(src=%d, dst=%d), dst fd is protected\n", fd1, fd2);
    errno = EBADF;
    return -1;
  }
  int ret = __real_dup2(fd1, fd2);

  dup_inet_socket(fd1, fd2);

  return ret;
}

static inline int __internal_poll(struct pollfd fds[], nfds_t nfds,
                                  int timeout) {
  bool use_internal = false;
  for (nfds_t i = 0; i < nfds; ++i) {
    if (is_optimized_socket_safe(fds[i].fd)) {
      use_internal = true;
      break;
    }
  }

  if (use_internal)
    return do_ipc_poll(fds, nfds, timeout);
  return __real_poll(fds, nfds, timeout);
}

static inline int __internal_pselect(int nfds, fd_set *readfds,
                                     fd_set *writefds, fd_set *errorfds,
                                     const struct timespec *timeout,
                                     const sigset_t *sigmask) {
  return do_ipc_pselect(nfds, readfds, writefds, errorfds, timeout, sigmask);
}

static inline int __internal_select(int nfds, fd_set *readfds, fd_set *writefds,
                                    fd_set *errorfds, struct timeval *timeout) {
  return do_ipc_select(nfds, readfds, writefds, errorfds, timeout);
}

static inline int __internal_getsockopt(int socket, int level, int option_name,
                                        void *option_value,
                                        socklen_t *option_len) {
  // TODO: Do something here besides forwarding to actual impl?
  return __real_getsockopt(socket, level, option_name, option_value,
                           option_len);
}

static inline int __internal_setsockopt(int socket, int level, int option_name,
                                        const void *option_value,
                                        socklen_t option_len) {
  int ret;
  if (!is_registered_socket(socket))
    ret =
        __real_setsockopt(socket, level, option_name, option_value, option_len);
  else
    ret =
        do_ipc_setsockopt(socket, level, option_name, option_value, option_len);

  return ret;
}
#endif // _SOCKET_INLINE_H_
