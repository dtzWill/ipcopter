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

#include "socket_inline.h"
#include "getfromlibc.h"
#include "wrapper.h"

BEGIN_EXTERN_C

// Data calls

ssize_t read(int fd, void *buf, size_t count) {
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

// Control calls

int accept(int fd, struct sockaddr *addr, socklen_t *addrlen) {
  return __internal_accept(fd, addr, addrlen);
}

int bind(int fd, const struct sockaddr *addr, socklen_t addrlen) {
  return __internal_bind(fd, addr, addrlen);
}

int close(int fd) { return __internal_close(fd); }

void closefrom(int lowfd) { return __internal_closefrom(lowfd); }

int connect(int fd, const struct sockaddr *addr, socklen_t addrlen) {
  return __internal_connect(fd, addr, addrlen);
}

int listen(int fd, int backlog) { return __internal_listen(fd, backlog); }

int shutdown(int sockfd, int how) { return __internal_shutdown(sockfd, how); }

int socket(int domain, int type, int protocol) {
  return __internal_socket(domain, type, protocol);
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

// Control


int __real_accept(int fd, struct sockaddr *addr, socklen_t *addrlen) {
  CALL_REAL(accept, fd, addr, addrlen);
}

int __real_bind(int fd, const struct sockaddr *addr, socklen_t addrlen) {
  CALL_REAL(bind, fd, addr, addrlen);
}

int __real_connect(int fd, const struct sockaddr *addr, socklen_t addrlen) {
  CALL_REAL(connect, fd, addr, addrlen);
}

int __real_close(int fd) { CALL_REAL(close, fd); }

int __real_listen(int fd, int backlog) { CALL_REAL(listen, fd, backlog); }

int __real_shutdown(int sockfd, int how) { CALL_REAL(shutdown, sockfd, how); }

int __real_socket(int domain, int type, int protocol) {
  CALL_REAL(socket, domain, type, protocol);
}

END_EXTERN_C
