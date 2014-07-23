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

#include <assert.h>

#include <stdio.h>

#include <linux/ip.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>

#ifdef __cplusplus
#define EXTERN_C extern "C"
#endif

EXTERN_C int __real_socket(int domain, int type, int protocol);
static inline int __internal_socket(int domain, int type, int protocol) {
  int fd = __real_socket(domain, type, protocol);
  if (domain == AF_INET && type == SOCK_STREAM && fd != -1) {
    register_inet_socket(fd);
  }
  return fd;
}

EXTERN_C ssize_t __real_recv(int fd, void *buf, size_t count, int flags);
static inline ssize_t __internal_recv(int fd, void *buf, size_t count,
                                      int flags) {
  ssize_t ret;
  if (!is_registered_socket(fd))
    ret = __real_recv(fd, buf, count, flags);
  else
    ret = do_ipc_recv(fd, buf, count, flags);
  return ret;
}

EXTERN_C ssize_t __real_send(int fd, const void *buf, size_t count, int flags);
static inline ssize_t __internal_send(int fd, const void *buf, size_t count,
                                      int flags) {
  ssize_t ret;
  if (!is_registered_socket(fd))
    ret = __real_send(fd, buf, count, flags);
  else
    ret = do_ipc_send(fd, buf, count, flags);
  return ret;
}
EXTERN_C ssize_t __real_recvfrom(int fd, void *buffer, size_t length, int flags,
                                 struct sockaddr *address,
                                 socklen_t *address_len);
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

EXTERN_C ssize_t __real_sendto(int fd, const void *message, size_t length,
                               int flags, const struct sockaddr *dest_addr,
                               socklen_t dest_len);
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

EXTERN_C ssize_t __real_read(int fd, void *buf, size_t count);
static inline ssize_t __internal_read(int fd, void *buf, size_t count) {
  ssize_t ret;
  if (!is_registered_socket(fd))
    ret = __real_read(fd, buf, count);
  else
    ret = do_ipc_recv(fd, buf, count, 0);
  return ret;
}
EXTERN_C ssize_t __real_write(int fd, const void *buf, size_t count);
static inline ssize_t __internal_write(int fd, const void *buf, size_t count) {
  ssize_t ret;
  if (!is_registered_socket(fd))
    ret = __real_write(fd, buf, count);
  else
    ret = do_ipc_send(fd, buf, count, 0);
  return ret;
}

EXTERN_C int __real_accept(int fd, struct sockaddr *addr, socklen_t *addrlen);
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

EXTERN_C int __real_bind(int fd, const struct sockaddr *addr,
                         socklen_t addrlen);
static inline int __internal_bind(int fd, const struct sockaddr *addr,
                                  socklen_t addrlen) {
  int ret = __real_bind(fd, addr, addrlen);
  return ret;
}

EXTERN_C int __real_connect(int fd, const struct sockaddr *addr,
                            socklen_t addrlen);
static inline int __internal_connect(int fd, const struct sockaddr *addr,
                                     socklen_t addrlen) {
  int ret = __real_connect(fd, addr, addrlen);
  return ret;
}

EXTERN_C int __real_listen(int fd, int backlog);
static inline int __internal_listen(int fd, int backlog) {
  int ret = __real_listen(fd, backlog);
  return ret;
}

EXTERN_C int __real_close(int fd);
static inline int __internal_close(int fd) {
  if (fd == -1) {
    ipclog("close(-1)??\n");
    return 0;
  }
  if (is_protected_fd(fd)) {
    ipclog("Attempt to close protected fd '%d', ignoring", fd);
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

EXTERN_C int __real_shutdown(int sockfd, int how);
static inline int __internal_shutdown(int sockfd, int how) {
  if (FILE *logfp = getlogfp()) {
    if (int logfd = fileno(logfp)) {
      if (logfd == sockfd) {
        ipclog("Attempt to shutdown our logging fd, ignoring!");
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

EXTERN_C int __real_fcntl(int fd, int cmd, void *arg);
static inline int __internal_fcntl(int fd, int cmd, void *arg) {
  // TODO: do something
  assert(!is_protected_fd(fd));
  int ret = __real_fcntl(fd, cmd, arg);

  return ret;
}
#endif // _SOCKET_INLINE_H_
