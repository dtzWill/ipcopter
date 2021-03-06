//===-- ipcopt.h ------------------------------------------------*- C++ -*-===//
//
// Slipstream: Automatic Interprocess Communication Optimization
//
// Copyright (c) 2015, Will Dietz <w@wdtz.org>
// This file is distributed under the ISC license, see LICENSE for details.
//
// http://wdtz.org/slipstream
//
//===----------------------------------------------------------------------===//
//
// IPC Optimization Central Runtime Management
//
//===----------------------------------------------------------------------===//

#ifndef _IPC_OPT_H_
#define _IPC_OPT_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <poll.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/uio.h>

// Initialization
void __ipcopt_init();

// FD registration
void register_inet_socket(int fd, bool accept);
char is_registered_socket(int fd);
char is_optimized_socket_safe(int fd);
void unregister_inet_socket(int fd);
void dup_inet_socket(int fd, int fd2);

bool is_accept(int fd);

// Options
void set_nonblocking(int fd, bool nonblocking);
bool get_nonblocking(int fd);
void set_local_nonblocking(int fd, bool nonblocking);
void set_cloexec(int fd, bool cloexec);

int getlocalfd(int fd);
void register_inherited_fds();
char is_protected_fd(int fd);

// Timing
struct timespec get_time();
void set_time(int fd, struct timespec start, struct timespec end);
void submit_info_if_needed(int fd);

// R/W operations using best available transport
ssize_t do_ipc_send(int fd, const void *buf, size_t count, int flags);
ssize_t do_ipc_recv(int fd, void *buf, size_t count, int flags);
ssize_t do_ipc_sendto(int fd, const void *message, size_t length, int flags,
                      const struct sockaddr *dest_addr, socklen_t dest_len);
ssize_t do_ipc_recvfrom(int fd, void *buffer, size_t length, int flags,
                        struct sockaddr *address, socklen_t *address_len);
ssize_t do_ipc_writev(int fd, const struct iovec *iov, int iovcnt);
ssize_t do_ipc_readv(int fd, const struct iovec *iov, int iovcnt);
ssize_t do_ipc_sendmsg(int socket, const struct msghdr *message, int flags);
ssize_t do_ipc_recvmsg(int socket, struct msghdr *message, int flags);

int do_ipc_poll(struct pollfd fds[], nfds_t nfds, int timeout);
int do_ipc_pselect(int nfds, fd_set *readfds, fd_set *writefds,
                   fd_set *errorfds, const struct timespec *timeout,
                   const sigset_t *sigmask);
int do_ipc_select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *errorfds,
                  struct timeval *timeout);

int do_ipc_shutdown(int sockfd, int how);

int do_ipc_fcntl(int fd, int cmd, void *arg);
int do_ipc_setsockopt(int socket, int level, int option_name,
                      const void *option_value, socklen_t option_len);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // _IPC_OPT_H_
