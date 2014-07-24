//===-- ipcopt.h ------------------------------------------------*- C++ -*-===//
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

#ifndef _IPC_OPT_H_
#define _IPC_OPT_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <poll.h>
#include <stdint.h>
#include <unistd.h>

// FD registration
void register_inet_socket(int fd);
char is_registered_socket(int fd);
char is_optimized_socket_safe(int fd);
void unregister_inet_socket(int fd);

int getlocalfd(int fd);
void register_inherited_fds();
char is_protected_fd(int fd);

// R/W operations using best available transport
ssize_t do_ipc_send(int fd, const void *buf, size_t count, int flags);
ssize_t do_ipc_recv(int fd, void *buf, size_t count, int flags);
ssize_t do_ipc_sendto(int fd, const void *message, size_t length, int flags,
                      const struct sockaddr *dest_addr, socklen_t dest_len);
ssize_t do_ipc_recvfrom(int fd, void *buffer, size_t length, int flags,
                        struct sockaddr *address, socklen_t *address_len);


int do_ipc_poll(struct pollfd fds[], nfds_t nfds, int timeout);

int do_ipc_shutdown(int sockfd, int how);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // _IPC_OPT_H_
