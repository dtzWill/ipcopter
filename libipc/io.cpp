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

void attempt_optimization(int fd, bool send) {
  endpoint ep = getEP(fd);
  assert(valid_ep(ep));
  ipc_info &i = getInfo(ep);
  assert(i.state != STATE_INVALID);

  size_t &bytes = send ? i.bytes_sent : i.bytes_recv;
  if (bytes == TRANS_THRESHOLD) {
    ipclog("Completed partial operation to sync at THRESHOLD for fd=%d!\n", fd);
    // TODO: Async!
    endpoint remote = EP_INVALID;
    size_t attempts = 0;
    while (true) {
      remote = ipcd_endpoint_kludge(ep);
      if (remote != EP_INVALID)
        break;
      ++attempts;
      if (attempts >= MAX_SYNC_ATTEMPTS + 3)
        break;
      if (attempts > 3) {
        sched_yield();
        usleep(ATTEMPT_SLEEP_INTERVAL);
      }
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
}

typedef ssize_t (*IOFunc)(...);

template <typename buf_t>
ssize_t do_ipc_io(int fd, buf_t buf, size_t count, int flags, IOFunc IO,
                  bool send) {
  endpoint ep = getEP(fd);
  ipc_info &i = getInfo(ep);
  assert(i.state != STATE_INVALID);

  // If localized, just use fast socket:
  size_t &bytes = send ? i.bytes_sent : i.bytes_recv;
  if (i.state == STATE_OPTIMIZED) {
    ssize_t ret = IO(i.localfd, buf, count, flags);
    if (ret != -1 && !(flags & MSG_PEEK)) {
      bytes += ret;
    }
    return ret;
  }

  // Otherwise, use original fd:
  ssize_t rem = (TRANS_THRESHOLD - bytes);
  if (rem > 0 && size_t(rem) <= count) {
    ssize_t ret = IO(fd, buf, rem, flags);

    if (ret == -1 || (flags & MSG_PEEK)) {
      return ret;
    }

    bytes += ret;
    assert(bytes <= TRANS_THRESHOLD);

    attempt_optimization(fd, send);

    return ret;
  }

  assert(i.state == STATE_UNOPT);

  ssize_t ret = IO(fd, buf, count, flags);

  if (ret == -1 || (flags & MSG_PEEK))
    return ret;

  // Successful operation, add to running total.
  bytes += ret;

  return ret;
}

ssize_t do_ipc_send(int fd, const void *buf, size_t count, int flags) {
  return do_ipc_io(fd, buf, count, flags, (IOFunc)__real_send, true);
}

ssize_t do_ipc_recv(int fd, void *buf, size_t count, int flags) {
  return do_ipc_io(fd, buf, count, flags, (IOFunc)__real_recv, false);
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

template <typename IOVFunc>
ssize_t do_ipc_iov(int fd, const struct iovec *vec, int count, IOVFunc IO,
                   bool send) {
  // TODO: Combine shared logic with do_ipc_io!

  ipc_info &i = getInfo(getEP(fd));
  assert(i.state != STATE_INVALID);

  // If localized, just use fast socket!
  size_t &bytes_stat = send ? i.bytes_sent : i.bytes_recv;
  if (i.state == STATE_OPTIMIZED) {
    ssize_t ret = IO(i.localfd, vec, count);
    if (ret != -1) {
      bytes_stat += ret;
    }
    return ret;
  }

  size_t bytes = 0;
  for (int i = 0; i < count; ++i) {
    // Overflow check...
    if (SSIZE_MAX - bytes < vec[i].iov_len) {
      // We could try to set errno here,
      // but easier to just let actual writev/readv do this:
      return IO(fd, vec, count);
    }

    bytes += vec[i].iov_len;
  }

  ssize_t rem = (TRANS_THRESHOLD - bytes_stat);
  if (rem > 0 && size_t(rem) <= bytes) {

    // Need to construct alternate iovec!
    iovec newvec[100];
    assert(100 > count);

    int newcount = 0;
    for (; newcount < count && rem != 0; ++newcount) {
      int copy = std::min(size_t(rem), vec[newcount].iov_len);

      // Put this iov into our newvec
      newvec[newcount].iov_base = vec[newcount].iov_base;
      newvec[newcount].iov_len = copy;

      rem -= copy;
    }
    assert(rem == 0);

    ssize_t ret = IO(fd, newvec, newcount);

    if (ret == -1)
      return ret;

    bytes_stat += ret;
    assert(bytes_stat <= TRANS_THRESHOLD);

    attempt_optimization(fd, send);

    return ret;
  }

  ssize_t ret = IO(fd, vec, count);

  // We don't handle other states yet
  assert(i.state == STATE_UNOPT);

  if (ret == -1)
    return ret;

  // Successful operation, add to running total.
  bytes_stat += ret;

  return ret;
}

ssize_t do_ipc_writev(int fd, const struct iovec *vec, int count) {
  return do_ipc_iov(fd, vec, count, __real_writev, true);
}

ssize_t do_ipc_readv(int fd, const struct iovec *vec, int count) {
  return do_ipc_iov(fd, vec, count, __real_readv, false);
}

ssize_t do_ipc_sendmsg(int socket, const struct msghdr *message, int flags) {
  ipc_info &i = getInfo(getEP(socket));
  assert(i.state != STATE_INVALID);

  // If optimized, simply perform operation on local socket
  size_t &bytes = i.bytes_sent;
  if (i.state == STATE_OPTIMIZED) {
    struct msghdr tmp = *message;
    tmp.msg_name = 0;
    tmp.msg_namelen = 0;
    ssize_t ret = __real_sendmsg(i.localfd, &tmp, flags);
    if (ret != -1) {
      bytes += ret;
    }
    return ret;
  }

  ssize_t ret = __real_sendmsg(socket, message, flags);
  if (ret != -1) {
    ssize_t rem = (TRANS_THRESHOLD - bytes);
    if (rem > 0 && rem <= ret) {
      ipclog("Missed threshold due to sendmsg()!\n");
      assert(0);
    }
    bytes += ret;
  }

  return ret;
}

ssize_t do_ipc_recvmsg(int socket, struct msghdr *message, int flags) {
  ipc_info &i = getInfo(getEP(socket));
  assert(i.state != STATE_INVALID);

  // If optimized, simply perform operation on local socket
  size_t &bytes = i.bytes_recv;
  if (i.state == STATE_OPTIMIZED) {
    struct msghdr tmp = *message;
    tmp.msg_name = 0;
    tmp.msg_namelen = 0;
    ssize_t ret = __real_recvmsg(i.localfd, &tmp, flags);
    if (ret != -1) {
      *message = tmp;
      bytes += ret;
    }
    return ret;
  }

  ssize_t ret = __real_recvmsg(socket, message, flags);
  if (ret != -1) {
    ssize_t rem = (TRANS_THRESHOLD - bytes);
    if (rem > 0 && rem <= ret) {
      ipclog("Missed threshold due to recvmsg()!\n");
      assert(0);
    }
    bytes += ret;
  }

  return ret;
}
