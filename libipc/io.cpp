//===-- io.cpp ------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// I/O routines for registered fd's
//
//===----------------------------------------------------------------------===//

#include "ipcopt.h"

#include "debug.h"
#include "ipcreg_internal.h"
#include "socket_inline.h"
#include "ipcd.h"

#include <cassert>

const size_t TRANS_THRESHOLD = 1ULL << 20;

void copy_bufsize(int src, int dst, int buftype) {
  int bufsize;
  socklen_t sz = sizeof(bufsize);
  int ret = getsockopt(src, SOL_SOCKET, buftype, &bufsize, &sz);
  assert(!ret);
  // ipclog("Bufsize %d of fd %d: %d\n", buftype, src, bufsize);
  ret = setsockopt(dst, SOL_SOCKET, buftype, &bufsize, sz);
  assert(!ret);
}
void copy_bufsizes(int src, int dst) {
  copy_bufsize(src, dst, SO_RCVBUF);
  copy_bufsize(src, dst, SO_SNDBUF);
}

ssize_t do_ipc_send(int fd, const void *buf, size_t count, int flags) {
  ipc_info *i = getFDDesc(fd);
  assert(i->valid);
  if (i->local) {
    return __real_send(i->localfd, buf, count, flags);
  } else if ((i->bytes_trans += count) > TRANS_THRESHOLD) {
    ipclog("send of %zu bytes crosses threshold for fd=%d\n", count, fd);
#if 0
    assert(fd == 4);
    assert(i->ep == 1);
    bool success = ipcd_localize(1, 2);
    assert(success && "Failed to localize sadtimes");
    i->local = true;
    // TODO: Async!
    i->localfd = ipcd_getlocalfd(i->ep);

    copy_bufsizes(fd, i->localfd);
#else
    i->local = true;
    i->localfd = fd;
#endif
  }
  return __real_send(fd, buf, count, flags);
}

ssize_t do_ipc_recv(int fd, void *buf, size_t count, int flags) {
  ipc_info *i = getFDDesc(fd);
  assert(i->valid);
  if (i->local) {
    return __real_recv(i->localfd, buf, count, flags);
  } else if ((i->bytes_trans += count) > TRANS_THRESHOLD) {
    ipclog("recv of %zu bytes crosses threshold for fd=%d\n", count, fd);
#if 0
    i->local = true;
    // TODO: Async!
    i->localfd = ipcd_getlocalfd(i->ep);

    copy_bufsizes(fd, i->localfd);
#else
    i->local = true;
    i->localfd = fd;
#endif
  }
  return __real_recv(fd, buf, count, flags);
}

ssize_t do_ipc_sendto(int fd, const void *message, size_t length, int flags,
                      const struct sockaddr *dest_addr, socklen_t dest_len) {
  //if (dest_addr)
  //  return __real_sendto(fd, message, length, flags, dest_addr, dest_len);
  // Hardly safe, but if it's non-NULL there's a chance
  // the caller might want us to write something there.
  assert(!dest_addr);

  // As a bonus from the above check, we can forward to plain send()
  return __real_send(fd, message, length, flags);
}
ssize_t do_ipc_recvfrom(int fd, void *buffer, size_t length, int flags,
                        struct sockaddr *address, socklen_t *address_len) {
  //if (address)
  //  return __real_recvfrom(fd, buffer, length, flags, address, address_len);
  // Hardly safe, but if it's non-NULL there's a chance
  // the caller might want us to write something there.
  assert(!address);

  // As a bonus from the above check, we can forward to plain recv()
  return __real_recv(fd, buffer, length, flags);
}
