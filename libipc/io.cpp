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
#include <arpa/inet.h>
#include <cassert>
#include <limits.h>
#include <netinet/in.h>
#include <sched.h>
#include <stdlib.h>
#include <string.h>

const size_t TRANS_THRESHOLD = 1ULL << 16;
const size_t MAX_SYNC_ATTEMPTS = 20;
const size_t MILLIS_IN_MICROSECONDS = 1000;
const size_t IPCD_SYNC_DELAY = 100 * MILLIS_IN_MICROSECONDS;
const size_t ATTEMPT_SLEEP_INTERVAL = IPCD_SYNC_DELAY / MAX_SYNC_ATTEMPTS;

void copy_bufsize(int src, int dst, int buftype) {
  int bufsize;
  socklen_t sz = sizeof(bufsize);
  int ret = __real_getsockopt(src, SOL_SOCKET, buftype, &bufsize, &sz);
  assert(!ret);

  // getsockopt returns doubled value of buffer,
  // which should be used in setsockopt().
  bufsize/=2;

  ret = __real_setsockopt(dst, SOL_SOCKET, buftype, &bufsize, sz);
  assert(!ret);
}
void copy_bufsizes(int src, int dst) {
  copy_bufsize(src, dst, SO_RCVBUF);
  copy_bufsize(src, dst, SO_SNDBUF);
}

size_t &get_byte_counter(ipc_info &i, bool send) {
  return send ? i.bytes_sent : i.bytes_recv;
}

char get_threshold_indicator_char(ipc_info &i, bool send) {
  size_t bytes = get_byte_counter(i, send);
  if (bytes > TRANS_THRESHOLD)
    return '>';
  if (bytes < TRANS_THRESHOLD)
    return '<';
  return '=';
}

void add_sockaddr_to_checksum(int fd, bool local, boost::crc_32_type &crc) {
  struct sockaddr_storage addr;
  socklen_t len = sizeof(addr);
  int port;
  int ret;
  char ipstr[INET6_ADDRSTRLEN];
  if (local)
    ret = getsockname(fd, (struct sockaddr *)&addr, &len);
  else
    ret = getpeername(fd, (struct sockaddr *)&addr, &len);
  assert(ret == 0);

  if (addr.ss_family == AF_INET) {
    struct sockaddr_in *s = (struct sockaddr_in *)&addr;
    port = ntohs(s->sin_port);
    const char *retstr = inet_ntop(AF_INET, &s->sin_addr, ipstr, sizeof(ipstr));
    assert(retstr != NULL);
  } else {
    struct sockaddr_in6 *s = (struct sockaddr_in6 *)&addr;
    port = ntohs(s->sin6_port);
    const char *retstr =
        inet_ntop(AF_INET6, &s->sin6_addr, ipstr, sizeof(ipstr));
    assert(retstr != NULL);
  }

  crc.process_bytes(ipstr, strlen(ipstr));
  crc.process_bytes(&port, sizeof(int));
}

void update_stats(int fd, bool send, const void*buf, ssize_t cnt) {
  ipc_info &i = getInfo(getEP(fd));
  size_t &bytes = get_byte_counter(i, send);
  if (cnt > 0) {

    if (get_byte_counter(i, true) == 0 &&
        get_byte_counter(i, false) == 0) {
      // sent: Local -> Remote
      add_sockaddr_to_checksum(fd, true, i.crc_sent);
      add_sockaddr_to_checksum(fd, false, i.crc_sent);

      // recv: Remote -> Local
      add_sockaddr_to_checksum(fd, false, i.crc_recv);
      add_sockaddr_to_checksum(fd, true, i.crc_recv);
    }

    if (bytes < TRANS_THRESHOLD) {
      if (send) {
        i.crc_sent.process_bytes(buf, cnt);
      } else {
        i.crc_recv.process_bytes(buf, cnt);
      }
    }
    bytes += cnt;
  }
}
void update_stats_vec(int fd, bool send, const struct iovec *vec,
                      ssize_t cnt) {
  int index = 0;
  while (cnt > 0) {
    int bytes = std::min(size_t(cnt), vec[index].iov_len);

    update_stats(fd, send, vec[index].iov_base, bytes);

    cnt -= bytes;
    ++index;
  }
}


void attempt_optimization(int fd, bool send) {
  endpoint ep = getEP(fd);
  assert(valid_ep(ep));
  ipc_info &i = getInfo(ep);
  assert(i.state != STATE_INVALID);

  if (get_byte_counter(i, send) == TRANS_THRESHOLD) {
    ipclog("Completed partial operation to sync at THRESHOLD for fd=%d!\n", fd);

    // TODO: Async!
    endpoint remote = EP_INVALID;
    size_t attempts = 0;
    uint32_t crc_sent = i.crc_sent.checksum();
    uint32_t crc_recv = i.crc_recv.checksum();
    ipclog("CRC's: %x, %x\n", crc_sent, crc_recv);
    while (true) {
      bool last = (++attempts >= MAX_SYNC_ATTEMPTS + 3);
      remote =
          ipcd_crc_kludge(ep, crc_sent, crc_recv, last);
      if (remote != EP_INVALID)
        break;
      if (last)
        break;
      if (attempts > 3) {
        sched_yield();
        usleep(ATTEMPT_SLEEP_INTERVAL);
      }
    }

    if (valid_ep(remote)) {
      ipclog("Found remote endpoint! Local=%d, Remote=%d Attempts=%zu!\n", ep,
             remote, attempts);
      ipclog("Send counter %zu%c (%x), recv: %zu%c (%x)\n", i.bytes_sent,
             get_threshold_indicator_char(i, true), i.crc_sent.checksum(),
             i.bytes_recv, get_threshold_indicator_char(i, false),
             i.crc_recv.checksum());

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
  if (i.state == STATE_OPTIMIZED) {
    ssize_t ret = IO(i.localfd, buf, count, flags);
    if (!(flags & MSG_PEEK)) {
      update_stats(fd, send, buf, ret);
    }
    return ret;
  }

  // Otherwise, use original fd:
  size_t &bytes = get_byte_counter(i, send);
  ssize_t rem = (TRANS_THRESHOLD - bytes);
  if (rem > 0 && size_t(rem) <= count) {
    ssize_t ret = IO(fd, buf, rem, flags);

    if (ret == -1 || (flags & MSG_PEEK)) {
      return ret;
    }

    update_stats(fd, send, buf, ret);
    assert(bytes <= TRANS_THRESHOLD);

    attempt_optimization(fd, send);

    // Did we truncate a blocking operation?

    // Only continue operation if socket is blocking
    // (otherwise the app definitely should be okay with short ops)
    bool continue_partial_op = !i.non_blocking;
    // Did we truncate from originally requested amount?
    assert(count >= size_t(ret));
    size_t left = count - size_t(ret);
    continue_partial_op &= (left > 0);
    // Did we receive the full amount requested?
    // If not, requesting the original amount
    // instead would have similarly returned a shorter amount.
    continue_partial_op &= (ret == rem);

    if (continue_partial_op) {
      ipclog("Attempting to continue partial op...fd=%d, count=%zu, left=%zu\n",
             fd, count, left);
      buf_t buf2 = buf_t(uintptr_t(buf) + ret);
      // Use MSG_DONTWAIT to get data already in buffer, no more.
      // Reasoning: original blocking call returned for some reason,
      // and we're only interested in patching around the reason
      // being that we requested fewer bytes than were available.
      ssize_t ret2 = do_ipc_io(fd, buf2, left, flags | MSG_DONTWAIT, IO, send);
      if (ret2 != -1) {
        ret += ret2;
      }
    }

    return ret;
  }

  assert(i.state == STATE_UNOPT);

  ssize_t ret = IO(fd, buf, count, flags);

  if (!(flags & MSG_PEEK)) {
    update_stats(fd, send, buf, ret);
  }

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

int truncate_iov(struct iovec newvec[100], const struct iovec *vec,
                 size_t bytes, int count) {
  assert(100 > count);

  int newcount = 0;
  for (; newcount < count &&bytes != 0; ++newcount) {
    int copy = std::min(bytes, vec[newcount].iov_len);

    // Put this iov into our newvec
    newvec[newcount].iov_base = vec[newcount].iov_base;
    newvec[newcount].iov_len = copy;

    bytes -= copy;
  }
  assert(bytes == 0);

  return newcount;
}

template <typename IOVFunc>
ssize_t do_ipc_iov(int fd, const struct iovec *vec, int count, IOVFunc IO,
                   bool send) {
  // TODO: Combine shared logic with do_ipc_io!

  ipc_info &i = getInfo(getEP(fd));
  assert(i.state != STATE_INVALID);

  // If localized, just use fast socket!
  if (i.state == STATE_OPTIMIZED) {
    ssize_t ret = IO(i.localfd, vec, count);
    update_stats_vec(fd, send, vec, ret);
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

  size_t &bytes_stat = get_byte_counter(i, send);
  ssize_t rem = (TRANS_THRESHOLD - bytes_stat);
  if (rem > 0 && size_t(rem) <= bytes) {

    // Need to construct alternate iovec!
    iovec newvec[100];
    int newcount = truncate_iov(newvec, vec, size_t(rem), count);

    ssize_t ret = IO(fd, newvec, newcount);

    if (ret == -1)
      return ret;

    update_stats_vec(fd, send, newvec, ret);
    assert(bytes_stat <= TRANS_THRESHOLD);

    attempt_optimization(fd, send);

    return ret;
  }

  // We don't handle other states yet
  assert(i.state == STATE_UNOPT);

  ssize_t ret = IO(fd, vec, count);

  update_stats_vec(fd, send, vec, ret);
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
  if (i.state == STATE_OPTIMIZED) {
    struct msghdr tmp = *message;
    tmp.msg_name = 0;
    tmp.msg_namelen = 0;
    ssize_t ret = __real_sendmsg(i.localfd, &tmp, flags);
    update_stats_vec(socket, true, tmp.msg_iov, ret);
    return ret;
  }

  size_t bytes = 0;
  int count = message->msg_iovlen;
  for (int i = 0; i < count; ++i) {
    // Overflow check...
    if (SSIZE_MAX - bytes < message->msg_iov[i].iov_len) {
      // We could try to set errno here,
      // but easier to just let actual handler do this:
      return __real_sendmsg(socket, message, flags);
    }

    bytes += message->msg_iov[i].iov_len;
  }

  size_t &bytes_stat = get_byte_counter(i, true);
  ssize_t rem = (TRANS_THRESHOLD - bytes_stat);
  if (rem > 0 && size_t(rem) <= bytes) {

    iovec newvec[100];
    int newcount = truncate_iov(newvec, message->msg_iov, size_t(rem), count);

    struct msghdr newmsg = *message;
    newmsg.msg_iov = newvec;
    newmsg.msg_iovlen = newcount;
    ssize_t ret = __real_sendmsg(socket, &newmsg, flags);

    if (ret == -1) {
      return ret;
    }

    update_stats_vec(socket, true, newvec, ret);
    assert(bytes_stat <= TRANS_THRESHOLD);

    attempt_optimization(socket, true);

    return ret;
  }

  ssize_t ret = __real_sendmsg(socket, message, flags);

  if (ret != -1) {
    update_stats_vec(socket, true, message->msg_iov, ret);
  }

  return ret;
}

ssize_t do_ipc_recvmsg(int socket, struct msghdr *message, int flags) {
  ipc_info &i = getInfo(getEP(socket));
  assert(i.state != STATE_INVALID);

  // If optimized, simply perform operation on local socket
  if (i.state == STATE_OPTIMIZED) {
    struct msghdr tmp = *message;
    tmp.msg_name = 0;
    tmp.msg_namelen = 0;
    ssize_t ret = __real_recvmsg(i.localfd, &tmp, flags);
    if (!(flags & MSG_PEEK)) {
      update_stats_vec(socket, false, tmp.msg_iov, ret);
    }
    if (ret != -1) {
      *message = tmp;
    }
    return ret;
  }

  size_t bytes = 0;
  int count = message->msg_iovlen;
  for (int i = 0; i < count; ++i) {
    // Overflow check...
    if (SSIZE_MAX - bytes < message->msg_iov[i].iov_len) {
      // We could try to set errno here,
      // but easier to just let actual handler do this:
      return __real_recvmsg(socket, message, flags);
    }

    bytes += message->msg_iov[i].iov_len;
  }

  size_t &bytes_stat = get_byte_counter(i, false);
  ssize_t rem = (TRANS_THRESHOLD - bytes_stat);
  if (rem > 0 && size_t(rem) <= bytes) {
    iovec newvec[100];
    int newcount = truncate_iov(newvec, message->msg_iov, rem, count);

    struct msghdr newmsg = *message;
    newmsg.msg_iov = newvec;
    newmsg.msg_iovlen = newcount;
    ssize_t ret = __real_recvmsg(socket, &newmsg, flags);

    if (ret == -1)
      return ret;
    if (flags & MSG_PEEK)
      return ret;

    // Copy message out to reader
    *message = newmsg;

    update_stats_vec(socket, false, newvec, ret);
    assert(bytes_stat <= TRANS_THRESHOLD);

    attempt_optimization(socket, false);

    return ret;
  }

  ssize_t ret = __real_recvmsg(socket, message, flags);
  if (!(flags & MSG_PEEK)) {
    update_stats_vec(socket, false, message->msg_iov, ret);
  }

  return ret;
}
