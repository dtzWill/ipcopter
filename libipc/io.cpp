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
#include "ipcd.h"
#include "ipcreg_internal.h"
#include "real.h"

#include <algorithm>
#include <cassert>
#include <limits.h>
#include <sched.h>
#include <stdlib.h>
#include <string.h>

const size_t TRANS_THRESHOLD = 1ULL << 20;
const size_t MAX_SYNC_ATTEMPTS = 20;
const size_t MILLIS_IN_MICROSECONDS = 1000;
const size_t IPCD_SYNC_DELAY = 100 * MILLIS_IN_MICROSECONDS;
const size_t ATTEMPT_SLEEP_INTERVAL = IPCD_SYNC_DELAY / MAX_SYNC_ATTEMPTS;

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
  endpoint ep = getEP(fd);
  ipc_info &i = getInfo(ep);
  assert(i.state != STATE_INVALID);

  // If localized, just use fast socket:
  if (i.state == STATE_OPTIMIZED) {
    ssize_t ret = IO(i.localfd, buf, count, flags);
    if (ret != -1) {
      i.bytes_trans += ret;
    }
    return ret;
  }

  // Otherwise, use original fd:
  ssize_t rem = (TRANS_THRESHOLD - i.bytes_trans);
  if (rem > 0 && size_t(rem) <= count) {
    ssize_t ret = IO(fd, buf, rem, flags);

    if (ret == -1) {
      return ret;
    }

    i.bytes_trans += ret;

    if (i.bytes_trans == TRANS_THRESHOLD) {
      ipclog("Completed partial operation to sync at THRESHOLD for fd=%d!\n",
             fd);
      // TODO: Async!
      endpoint remote;
      size_t attempts = 0;
      while (((remote = ipcd_endpoint_kludge(ep)) == EP_INVALID) &&
             ++attempts < MAX_SYNC_ATTEMPTS) {
        sched_yield();
        usleep(ATTEMPT_SLEEP_INTERVAL);
      }

      if (valid_ep(remote)) {
        ipclog("Found remote endpoint! Local=%d, Remote=%d Attempts=%zu!\n", ep,
               remote, attempts);

        bool success = ipcd_localize(ep, remote);
        assert(success && "Failed to localize! Sadtimes! :(");
        i.localfd = getlocalfd(fd);
        i.state = STATE_OPTIMIZED;

        // Configure localfd
        copy_bufsizes(fd, i.localfd);
        set_local_nonblocking(fd, i.non_blocking);
      }
    }

    return ret;
  }

  ssize_t ret = IO(fd, buf, count, flags);

  // We don't handle other states yet
  assert(i.state == STATE_UNOPT);

  if (ret == -1)
    return ret;

  // Successful operation, add to running total.
  i.bytes_trans += ret;

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

ssize_t do_ipc_writev(int fd, const struct iovec *vec, int count) {
  // TODO: Use alloca for small sizes
  // Based on implementation found here:
  // http://www.oschina.net/code/explore/glibc-2.9/sysdeps/posix/writev.c

  size_t bytes = 0;
  for (int i = 0; i < count; ++i) {
    // Overflow check...
    if (SSIZE_MAX - bytes < vec[i].iov_len) {
      // We could try to set errno here,
      // but easier to just let actual writev do this:
      return __real_writev(fd, vec, count);
    }

    bytes += vec[i].iov_len;
  }

  char *buffer = (char *)malloc(bytes);
  assert(buffer);

  char *bp = buffer;
  size_t to_copy = bytes;
  for (int i = 0; i < count; ++i) {
    size_t copy = std::min(vec[i].iov_len, to_copy);

    bp = (char *)mempcpy((void *)bp, (void *)vec[i].iov_base, copy);
    to_copy -= copy;
    if (to_copy == 0)
      break;
  }

  ssize_t ret = do_ipc_send(fd, buffer, bytes, 0);

  free(buffer);

  return ret;
}
