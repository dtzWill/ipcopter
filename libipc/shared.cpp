//===-- shared.c ----------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Socket-related function wrappers for use in shared library
//
//===----------------------------------------------------------------------===//

#include "epoll.h"
#include "getfromlibc.h"
#include "socket_inline.h"
#include "wrapper.h"
#include "system.h"

#include <poll.h>
#include <stdarg.h>

BEGIN_EXTERN_C

#define RESTRICT

// Data calls

ssize_t read(int fd, void *buf, size_t count) {
  return __internal_read(fd, buf, count);
}

ssize_t __read_chk(int fd, void *buf, size_t count, size_t /* buflen */) {
  // TODO: Implement checked version!
  return __internal_read(fd, buf, count);
}

ssize_t recv(int fd, void *buf, size_t count, int flags) {
  return __internal_recv(fd, buf, count, flags);
}

ssize_t recvfrom(int fd, void *buffer, size_t length, int flags,
                 struct sockaddr *address, socklen_t *address_len) {
  return __internal_recvfrom(fd, buffer, length, flags, address, address_len);
}

ssize_t send(int fd, const void *buf, size_t count, int flags) {
  return __internal_send(fd, buf, count, flags);
}

ssize_t sendto(int fd, const void *message, size_t length, int flags,
               const struct sockaddr *dest_addr, socklen_t dest_len) {
  return __internal_sendto(fd, message, length, flags, dest_addr, dest_len);
}

ssize_t write(int fd, const void *buf, size_t count) {
  return __internal_write(fd, buf, count);
}

ssize_t writev(int fd, const struct iovec *iov, int iovcnt) {
  return __internal_writev(fd, iov, iovcnt);
}

ssize_t readv(int fd, const struct iovec *iov, int iovcnt) {
  return __internal_readv(fd, iov, iovcnt);
}

// Control calls

int accept(int fd, struct sockaddr *addr, socklen_t *addrlen) {
  return __internal_accept4(fd, addr, addrlen, 0);
}

int accept4(int fd, struct sockaddr *addr, socklen_t *addrlen, int flags) {
  return __internal_accept4(fd, addr, addrlen, flags);
}

int bind(int fd, const struct sockaddr *addr, socklen_t addrlen) {
  return __internal_bind(fd, addr, addrlen);
}

int close(int fd) { return __internal_close(fd); }

void closefrom(int lowfd) { return __internal_closefrom(lowfd); }

int connect(int fd, const struct sockaddr *addr, socklen_t addrlen) {
  return __internal_connect(fd, addr, addrlen);
}

int dup(int fd) { return __internal_dup(fd); }

int dup2(int fd1, int fd2) { return __internal_dup2(fd1, fd2); }

int fcntl(int fd, int cmd, ...) {
  va_list va;
  va_start(va, cmd);

  // XXX: Kludge that should work, heh.
  // Read extra argument, regardless:
  void *arg = va_arg(va, void *);
  va_end(va);

  return __internal_fcntl(fd, cmd, arg);
}

int getsockopt(int socket, int level, int option_name,
               void *RESTRICT option_value, socklen_t *RESTRICT option_len);

int listen(int fd, int backlog) { return __internal_listen(fd, backlog); }

int poll(struct pollfd fds[], nfds_t nfds, int timeout) {
  return __internal_poll(fds, nfds, timeout);
}
int __poll_chk(struct pollfd fds[], nfds_t nfds, int timeout,
               size_t /* fdslen */) {
  // TODO: Support checking!
  return __internal_poll(fds, nfds, timeout);
}

int pselect(int nfds, fd_set *RESTRICT readfds, fd_set *RESTRICT writefds,
            fd_set *RESTRICT errorfds, const struct timespec *RESTRICT timeout,
            const sigset_t *RESTRICT sigmask) {
  return __internal_pselect(nfds, readfds, writefds, errorfds, timeout,
                            sigmask);
}

int select(int nfds, fd_set *RESTRICT readfds, fd_set *RESTRICT writefds,
           fd_set *RESTRICT errorfds, struct timeval *RESTRICT timeout) {
  return __internal_select(nfds, readfds, writefds, errorfds, timeout);
}

int setsockopt(int socket, int level, int option_name, const void *option_value,
               socklen_t option_len);

int shutdown(int sockfd, int how) { return __internal_shutdown(sockfd, how); }

int socket(int domain, int type, int protocol) {
  return __internal_socket(domain, type, protocol);
}

// System
pid_t fork(void) { return __internal_fork(); }
int execve(const char *path, char *const argv[], char *const envp[]) {
  return __internal_execve(path, argv, envp);
}

// epoll

int epoll_create(int size) { return __internal_epoll_create(size); }

int epoll_create1(int flags) { return __internal_epoll_create1(flags); }
int epoll_wait(int epfd, struct epoll_event *events, int maxevents,
               int timeout) {
  return __internal_epoll_pwait(epfd, events, maxevents, timeout, 0);
}
int epoll_pwait(int epfd, struct epoll_event *events, int maxevents,
                int timeout, const sigset_t *sigmask) {
  return __internal_epoll_pwait(epfd, events, maxevents, timeout, sigmask);
}
int epoll_ctl(int epfd, int op, int fd, struct epoll_event *event) {
  return __internal_epoll_ctl(epfd, op, fd, event);
}

//===- Access to libc-backed versions -------------------------------------===//

// Data

ssize_t __real_read(int fd, void *buf, size_t count) {
  CALL_REAL(read, fd, buf, count);
}

ssize_t __real_recv(int fd, void *buf, size_t count, int flags) {
  CALL_REAL(recv, fd, buf, count, flags);
}

ssize_t __real_recvfrom(int fd, void *buffer, size_t length, int flags,
                        struct sockaddr *address, socklen_t *address_len) {
  CALL_REAL(recvfrom, fd, buffer, length, flags, address, address_len);
}

ssize_t __real_send(int fd, const void *buf, size_t count, int flags) {
  CALL_REAL(send, fd, buf, count, flags);
}

ssize_t __real_sendto(int fd, const void *message, size_t length, int flags,
                      const struct sockaddr *dest_addr, socklen_t dest_len) {
  CALL_REAL(sendto, fd, message, length, flags, dest_addr, dest_len);
}

ssize_t __real_write(int fd, const void *buf, size_t count) {
  CALL_REAL(write, fd, buf, count);
}

ssize_t __real_writev(int fd, const struct iovec *iov, int iovcnt) {
  CALL_REAL(writev, fd, iov, iovcnt);
}

ssize_t __real_readv(int fd, const struct iovec *iov, int iovcnt) {
  CALL_REAL(readv, fd, iov, iovcnt);
}

// Control

int __real_accept4(int fd, struct sockaddr *addr, socklen_t *addrlen,
                   int flags) {
  CALL_REAL(accept4, fd, addr, addrlen, flags);
}

int __real_bind(int fd, const struct sockaddr *addr, socklen_t addrlen) {
  CALL_REAL(bind, fd, addr, addrlen);
}

int __real_connect(int fd, const struct sockaddr *addr, socklen_t addrlen) {
  CALL_REAL(connect, fd, addr, addrlen);
}

int __real_close(int fd) { CALL_REAL(close, fd); }

int __real_dup(int fd) { CALL_REAL(dup, fd); }

int __real_dup2(int fd1, int fd2) { CALL_REAL(dup2, fd1, fd2); }

int __real_fcntl(int fd, int cmd, void *arg) { CALL_REAL(fcntl, fd, cmd, arg); }

int __real_getsockopt(int socket, int level, int option_name,
                      void *RESTRICT option_value,
                      socklen_t *RESTRICT option_len) {
  CALL_REAL(getsockopt, socket, level, option_name, option_value, option_len);
}
int __real_listen(int fd, int backlog) { CALL_REAL(listen, fd, backlog); }

int __real_poll(struct pollfd fds[], nfds_t nfds, int timeout) {
  CALL_REAL(poll, fds, nfds, timeout);
}

int __real_pselect(int nfds, fd_set *RESTRICT readfds,
                   fd_set *RESTRICT writefds, fd_set *RESTRICT errorfds,
                   const struct timespec *RESTRICT timeout,
                   const sigset_t *RESTRICT sigmask) {
  CALL_REAL(pselect, nfds, readfds, writefds, errorfds, timeout, sigmask);
}

int __real_select(int nfds, fd_set *RESTRICT readfds, fd_set *RESTRICT writefds,
                  fd_set *RESTRICT errorfds, struct timeval *RESTRICT timeout) {
  CALL_REAL(select, nfds, readfds, writefds, errorfds, timeout);
}

int __real_setsockopt(int socket, int level, int option_name,
                      const void *option_value, socklen_t option_len) {
  CALL_REAL(setsockopt, socket, level, option_name, option_value, option_len);
}

int __real_shutdown(int sockfd, int how) { CALL_REAL(shutdown, sockfd, how); }

int __real_socket(int domain, int type, int protocol) {
  CALL_REAL(socket, domain, type, protocol);
}

// epoll

int __real_epoll_create(int size) { CALL_REAL(epoll_create, size); }
int __real_epoll_create1(int flags) { CALL_REAL(epoll_create1, flags); }
int __real_epoll_pwait(int epfd, struct epoll_event *events, int maxevents,
                       int timeout, const sigset_t *sigmask) {
  CALL_REAL(epoll_pwait, epfd, events, maxevents, timeout, sigmask);
}
int __real_epoll_ctl(int epfd, int op, int fd, struct epoll_event *event) {
  CALL_REAL(epoll_ctl, epfd, op, fd, event);
}

END_EXTERN_C
