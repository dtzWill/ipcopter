//===-- real.h --------------------------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Declaration of 'real' versions of hooked functions, __real_*
//
//===----------------------------------------------------------------------===//
#ifndef _REAL_H_
#define _REAL_H_

#include <poll.h>
#include <stdint.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/epoll.h>

#ifdef __cplusplus
#define EXTERN_C extern "C"
#endif

EXTERN_C pid_t __real_fork(void);
EXTERN_C int __real_execve(const char *path, char *const argv[],
                           char *const envp[]);
EXTERN_C int __real_socket(int domain, int type, int protocol);
EXTERN_C ssize_t __real_recv(int fd, void *buf, size_t count, int flags);
EXTERN_C ssize_t __real_send(int fd, const void *buf, size_t count, int flags);
EXTERN_C ssize_t __real_recvfrom(int fd, void *buffer, size_t length, int flags,
                                 struct sockaddr *address,
                                 socklen_t *address_len);
EXTERN_C ssize_t __real_sendto(int fd, const void *message, size_t length,
                               int flags, const struct sockaddr *dest_addr,
                               socklen_t dest_len);
EXTERN_C ssize_t __real_read(int fd, void *buf, size_t count);
EXTERN_C ssize_t __real_write(int fd, const void *buf, size_t count);
EXTERN_C ssize_t __real_writev(int fd, const struct iovec *iov, int iovcnt);
EXTERN_C ssize_t __real_readv(int fd, const struct iovec *iov, int iovcnt);
EXTERN_C int __real_accept4(int fd, struct sockaddr *addr, socklen_t *addrlen,
                            int flags);
EXTERN_C int __real_bind(int fd, const struct sockaddr *addr,
                         socklen_t addrlen);
EXTERN_C int __real_connect(int fd, const struct sockaddr *addr,
                            socklen_t addrlen);
EXTERN_C int __real_listen(int fd, int backlog);

EXTERN_C int __real_close(int fd);
EXTERN_C int __real_shutdown(int sockfd, int how);

EXTERN_C int __real_fcntl(int fd, int cmd, void *arg);
// Helper to avoid annoying cast:
static inline int __real_fcntl_int(int fd, int cmd, int arg) {
  return __real_fcntl(fd, cmd, (void *)(uintptr_t)(unsigned)arg);
}
EXTERN_C int __real_getsockopt(int socket, int level, int option_name,
                               void *option_value, socklen_t *option_len);
EXTERN_C int __real_dup(int fd);
EXTERN_C int __real_dup2(int fd1, int fd2);
EXTERN_C int __real_poll(struct pollfd fds[], nfds_t nfds, int timeout);
EXTERN_C int __real_pselect(int nfds, fd_set *readfds, fd_set *writefds,
                            fd_set *errorfds, const struct timespec *timeout,
                            const sigset_t *sigmask);
EXTERN_C int __real_select(int nfds, fd_set *readfds, fd_set *writefds,
                           fd_set *errorfds, struct timeval *timeout);
EXTERN_C int __real_setsockopt(int socket, int level, int option_name,
                               const void *option_value, socklen_t option_len);

// epoll

EXTERN_C int __real_epoll_create(int size);
EXTERN_C int __real_epoll_create1(int flags);
EXTERN_C int __real_epoll_pwait(int epfd, struct epoll_event *events,
                                int maxevents, int timeout,
                                const sigset_t *sigmask);
EXTERN_C int __real_epoll_ctl(int epfd, int op, int fd,
                              struct epoll_event *event);

#endif // _REAL_H_
