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
#include <sched.h>

const size_t TRANS_THRESHOLD = 1ULL << 20;
const size_t MAX_SYNC_ATTEMPTS = 5;

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

typedef ssize_t (*IOFunc)(...);

template <typename buf_t>
ssize_t do_ipc_io(int fd, buf_t buf, size_t count, int flags, IOFunc IO) {
  ipc_info *i = getFDDesc(fd);
  assert(i->valid);

  // If localized, just use fast socket:
  if (i->state == STATE_OPTIMIZED) {
    return IO(i->localfd, buf, count, flags);
  }

  // Otherwise, use original fd:
  ssize_t rem = (TRANS_THRESHOLD - i->bytes_trans);
  if (rem > 0 && size_t(rem) <= count) {
    ssize_t ret = IO(fd, buf, rem, flags);

    if (ret == -1) {
      return ret;
    }

    i->bytes_trans += ret;

    if (i->bytes_trans == TRANS_THRESHOLD) {
      ipclog("Completed partial operation to sync at THRESHOLD for fd=%d!\n",
             fd);
      // TODO: Async!
      endpoint remote;
      size_t attempts = 0;
      while (((remote = ipcd_endpoint_kludge(i->ep)) == EP_INVALID) &&
             ++attempts < MAX_SYNC_ATTEMPTS) {
        sched_yield();
      }

      if (remote != EP_INVALID) {
        ipclog("Found remote endpoint! Local=%d, Remote=%d Attempts=%zu!\n",
               i->ep, remote, attempts);

        bool success = ipcd_localize(i->ep, remote);
        assert(success && "Failed to localize! Sadtimes! :(");
        i->localfd = getlocalfd(fd);
        i->state = STATE_OPTIMIZED;

        copy_bufsizes(fd, i->localfd);
      }
    }

    return ret;
  }

  ssize_t ret = IO(fd, buf, count, flags);

  // We don't handle other states yet
  assert(i->state == STATE_UNOPT);

  if (ret == -1)
    return ret;

  // Successful operation, add to running total.
  i->bytes_trans += ret;

  return ret;
}

ssize_t do_ipc_send(int fd, const void *buf, size_t count, int flags) {
  return do_ipc_io(fd, buf, count, flags, (IOFunc)__real_send);
}

ssize_t do_ipc_recv(int fd, void *buf, size_t count, int flags) {
  return do_ipc_io(fd, buf, count, flags, (IOFunc)__real_recv);
}

ssize_t do_ipc_sendto(int fd, const void *message, size_t length, int flags,
                      const struct sockaddr *dest_addr,
                      socklen_t /* unused */) {
  // if (dest_addr)
  //  return __real_sendto(fd, message, length, flags, dest_addr, dest_len);
  // Hardly safe, but if it's non-NULL there's a chance
  // the caller might want us to write something there.
  assert(!dest_addr);

  // As a bonus from the above check, we can forward to plain send()
  return do_ipc_send(fd, message, length, flags);
}
ssize_t do_ipc_recvfrom(int fd, void *buffer, size_t length, int flags,
                        struct sockaddr *address, socklen_t * /* unused */) {
  // if (address)
  //  return __real_recvfrom(fd, buffer, length, flags, address, address_len);
  // Hardly safe, but if it's non-NULL there's a chance
  // the caller might want us to write something there.
  assert(!address);

  // As a bonus from the above check, we can forward to plain recv()
  return do_ipc_recv(fd, buffer, length, flags);
}
