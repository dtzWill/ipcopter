//===-- ipcopt.c ----------------------------------------------------------===//
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

#include "debug.h"
#include "ipcd.h"
#include "ipcopt.h"
#include "ipcreg_internal.h"
#include "real.h"
#include "shm.h"

#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>

// TODO: This table is presently not used thread-safe at all!
libipc_state state;
libipc_state::libipc_state() {
  for (unsigned i = 0; i < TABLE_SIZE; ++i) {
    FDMap[i] = fd_info();
    EndpointInfo[i] = ipc_info();
  }
}

void scan_for_cloexec() {
  for (unsigned i = 0; i < TABLE_SIZE; ++i) {
    fd_info &f = getFDInfo(i);
    if ((valid_ep(f.EP) || f.epoll.valid) && f.close_on_exec) {
      // This fd is actually already closed!

      // Unregister it, closing localfd as needed...
      unregister_inet_socket(i);
    }
  }
}

void dump_registered_fds() {
  for (unsigned i = 0; i < TABLE_SIZE; ++i) {
    fd_info &f = getFDInfo(i);
    if (valid_ep(f.EP)) {
      ipc_info &info = getInfo(f.EP);
      ipclog("Inherited known fd: %d -> (endpoint: %d, localfd: %d)\n", i, f.EP,
             info.localfd);
    }
  }
}

void __ipcopt_init() {
  state = libipc_state();
  shm_state_restore();
  scan_for_cloexec();
  dump_registered_fds();
}

void __attribute__((destructor)) ipcopt_fini() {
  ipclog("ipcopt_fini()!\n");
  for (unsigned i = 0; i < TABLE_SIZE; ++i)
    if (is_registered_socket(i)) {
      endpoint ep = getEP(i);
      ipc_info &info = getInfo(ep);
      if (is_optimized_socket_safe(i)) {
        ipclog("Optimized endpoint: ep=%d, fd=%d, localfd=%d, S: %zu R: %zu\n",
               ep, i, info.localfd, info.bytes_sent, info.bytes_recv);
      } else
        ipclog("Normal endpoint: ep=%d, fd=%d, S: %zu R: %zu\n",
               ep, i, info.bytes_sent, info.bytes_recv);

    }

  // And ensure they're unregistered!
  for (unsigned i = 0; i < TABLE_SIZE; ++i)
    if (is_registered_socket(i)) {
      // Don't use unregister_inet_socket--
      // we don't want to change state that may
      // break concurrently executing threads,
      // only to let IPCD know we're done with it.

      endpoint ep = getEP(i);
      if (--getInfo(ep).ref_count == 0) {
        bool success = ipcd_unregister_socket(ep);
        if (!success) {
          ipclog("Failure unregistering socket in destructor!\n");
        }
      }
    }
}

void invalidate(endpoint ep) {
  ipc_info &i = getInfo(ep);
  assert(i.state != STATE_INVALID);
  assert(i.ref_count == 0);
  i.reset();
}

void register_inet_socket(int fd, bool is_accept) {
  if (!ipcd_enabled())
    return;
  // Freshly created socket
  ipclog("Registering socket fd=%d\n", fd);
  endpoint &ep = getEP(fd);
  // We better not think we already have an endpoint ID for this fd
  assert(ep == EP_INVALID);
  ep = ipcd_register_socket(fd);

  ipc_info &i = getInfo(ep);
  assert(i.ref_count == 0);
  assert(i.state == STATE_INVALID);
  i.reset();

  i.ref_count++;
  i.is_accept = is_accept;
  i.state = STATE_UNOPT;
}

void unregister_inet_socket(int fd) {
  if (!ipcd_enabled())
    return;
  if (!inbounds_fd(fd)) {
    return;
  }

  // Closing epoll fd makes it no longer valid epoll fd.
  fd_info &f = getFDInfo(fd);
  f.epoll.valid = false;

  endpoint ep = getEP(fd);
  // Allow attempt to unregister fd's we don't
  // have endpoints for, this happens all the time :)
  if (ep == EP_INVALID) {
    return;
  }
  ipc_info &i = getInfo(ep);
  assert(i.state != STATE_INVALID);
  ipclog("Unregistering socket fd=%d, S: %zu R: %zu\n", fd,
         i.bytes_sent, i.bytes_recv);

  assert(i.ref_count > 0);
  // FD no longer refers to this endpoint!
  f.EP = EP_INVALID;
  f.close_on_exec = false;
  if (--i.ref_count == 0) {
    // Last reference to this endpoint,
    // tell ipcd we're done with it.
    bool success = ipcd_unregister_socket(ep);
    if (!success) {
      ipclog("ipcd_unregister_socket(%d) failed!\n", ep);
    }

    // Close local fd if exists
    if (i.localfd) {
      assert(i.state == STATE_OPTIMIZED);
      __real_close(i.localfd);

      ipclog("Closing opt. endpt : ep=%d, fd=%d, localfd=%d, S: %zu R: %zu\n",
             ep, fd, i.localfd, i.bytes_sent, i.bytes_recv);

      bool &isLocal = is_local(i.localfd);
      // Consistency check
      assert(isLocal);
      assert(getEP(i.localfd) == EP_INVALID);
      // No longer special 'local' fd
      isLocal = false;
    }

    invalidate(ep);
    return;
  } else {
    ipclog("Retaining endpoint info for ep=%d, despite unreegistering fd=%d!\n",
           ep, fd);
  }
}

char is_registered_socket(int fd) {
  return inbounds_fd(fd) && (getEP(fd) != EP_INVALID);
}

char is_optimized_socket_safe(int fd) {
  if (!inbounds_fd(fd) || is_local(fd))
    return false;

  endpoint ep = getEP(fd);
  if (ep == EP_INVALID)
    return false;

  return getInfo(ep).state == STATE_OPTIMIZED;
}

int getlocalfd(int fd) {
  assert(is_registered_socket(fd));
  int local = ipcd_getlocalfd(getEP(fd));

  bool &isLocal = is_local(local);
  assert(!isLocal);
  assert(getEP(local) == EP_INVALID);
  isLocal = true;

  return local;
}

char is_protected_fd(int fd) {
  // Logging fd is protected
#if USE_DEBUG_LOGGER
  if (FILE *logfp = getlogfp())
    if (int logfd = fileno(logfp))
      if (logfd == fd)
        return true;
#endif

  if (!ipcd_enabled())
    return false;

  // Check for ipcd protected sockets:
  if (ipcd_is_protected(fd))
    return true;

  if (!inbounds_fd(fd)) {
    return false;
  }

  // So are all local fd's:
  if (is_local(fd))
    return true;

  // Everything else is unprotected
  return false;
}

void register_inherited_fds() {
  for (unsigned ep = 0; ep < TABLE_SIZE; ++ep) {
    ipc_info &i = getInfo(ep);
    if (i.state != STATE_INVALID) {
      assert(i.ref_count > 0);
      bool ret = ipcd_reregister_socket(ep, 0 /* XXX */);
      if (!ret) {
        ipclog("Failed to reregister endpoint '%d'\n", ep);
      }
    }
  }
}

void dup_inet_socket(int fd1, int fd2) {
  assert(!is_protected_fd(fd2));

  // Ensure fd2 is closed if we didn't already think so.
  unregister_inet_socket(fd2);

  if (!is_registered_socket(fd1)) {
    return;
  }

  ipclog("Dup: %d -> %d\n", fd1, fd2);

  // Get info for the source descriptor
  endpoint ep = getEP(fd1);
  assert(valid_ep(ep));

  ipc_info &i = getInfo(ep);
  assert(i.ref_count > 0);
  assert(i.state != STATE_INVALID);
  // Bump reference count
  i.ref_count++;

  // Point fd2 at this ep:
  assert(getEP(fd2) == EP_INVALID);
  getEP(fd2) = ep;
}

void set_nonblocking(int fd, bool non_blocking) {
  if (!ipcd_enabled())
    return;
  endpoint ep = getEP(fd);
  assert(valid_ep(ep));

  getInfo(ep).non_blocking = non_blocking;
}

bool get_nonblocking(int fd) {
  assert(ipcd_enabled());

  endpoint ep = getEP(fd);
  assert(valid_ep(ep));

  return getInfo(ep).non_blocking;
}

void set_cloexec(int fd, bool cloexec) {
  if (!ipcd_enabled())
    return;
  getFDInfo(fd).close_on_exec = cloexec;
}

bool is_accept(int fd) {
  endpoint ep = getEP(fd);
  assert(valid_ep(ep));

  return getInfo(ep).is_accept;
}

struct timespec get_time() {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ts;
}

// XXX: Put this elsewhere
bool get_netaddr(int fd, netaddr &na, bool local) {
  struct sockaddr_storage addr;
  socklen_t len = sizeof(addr);
  int ret;
  if (local)
    ret = getsockname(fd, (struct sockaddr *)&addr, &len);
  else
    ret = getpeername(fd, (struct sockaddr *)&addr, &len);
  if (ret != 0) {
    perror("get_netaddr getsockname/getpeername");
    return false;
  }

  if (addr.ss_family == AF_INET) {
    struct sockaddr_in *s = (struct sockaddr_in *)&addr;
    na.port = ntohs(s->sin_port);
    const char *retstr = inet_ntop(AF_INET, &s->sin_addr, na.addr, sizeof(na.addr));
    assert(retstr != NULL);
  } else {
    struct sockaddr_in6 *s = (struct sockaddr_in6 *)&addr;
    na.port = ntohs(s->sin6_port);
    const char *retstr =
        inet_ntop(AF_INET6, &s->sin6_addr, na.addr, sizeof(na.addr));
    assert(retstr != NULL);
  }

  return true;
}


void set_time(int fd, struct timespec start, struct timespec end) {
  endpoint ep = getEP(fd);
  assert(valid_ep(ep));

  ipc_info &i = getInfo(ep);
  assert((i.connect_start.tv_sec == 0 && i.connect_start.tv_nsec == 0) &&
         "Start already set");
  assert((i.connect_end.tv_sec == 0 && i.connect_end.tv_nsec == 0) &&
         "End already set");

  i.connect_start = start;
  i.connect_end = end;

  assert(!i.sent_info);
}

void submit_info_if_needed(int fd) {
  if (!is_registered_socket(fd))
    return;
  endpoint ep = getEP(fd);

  ipc_info &i = getInfo(ep);
  if (i.state != STATE_UNOPT)
    return;

  if (i.sent_info)
    return;

  endpoint_info ei;
  ei.is_accept = i.is_accept;
  ei.connect_start = i.connect_start;
  ei.connect_end = i.connect_end;

  if (!get_netaddr(fd, ei.src, true) ||
      !get_netaddr(fd, ei.dst, false)) {
    ipclog("Unable to gather info for fd=%d, ep=%d\n", fd, ep);
    return;
  }
  i.sent_info = ipcd_endpoint_info(ep, ei);
  assert(i.sent_info);
  ipclog("submitted info for fd=%d, ep=%d\n", fd, ep);
}
