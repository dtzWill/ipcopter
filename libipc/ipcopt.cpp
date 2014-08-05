//===-- ipcopt.c ----------------------------------------------------------===//
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

#include "debug.h"
#include "ipcd.h"
#include "ipcopt.h"
#include "ipcreg_internal.h"
#include "real.h"
#include "shm.h"

#include <assert.h>
#include <stdio.h>
#include <unistd.h>

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
    if (is_registered_socket(i))
      unregister_inet_socket(i);
}

void invalidate(endpoint ep) {
  ipc_info &i = getInfo(ep);
  assert(i.state != STATE_INVALID);
  assert(i.ref_count == 0);
  i.reset();
}

void register_inet_socket(int fd) {
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
  if (FILE *logfp = getlogfp())
    if (int logfd = fileno(logfp))
      if (logfd == fd)
        return true;

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
  // We just forked, bump ref count
  // on the fd's we inherited.
  for (unsigned fd = 0; fd < TABLE_SIZE; ++fd) {
    if (is_registered_socket(fd)) {
      endpoint ep = getEP(fd);
      assert(ep != EP_INVALID);
      bool ret = ipcd_reregister_socket(ep, fd);
      if (!ret) {
        ipclog("Failed to reregister endpoint '%d' for inherited fd '%d'\n",
               ep, fd);
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

void set_cloexec(int fd, bool cloexec) {
  if (!ipcd_enabled())
    return;
  getFDInfo(fd).close_on_exec = cloexec;
}
