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
libipc_state state = {{}, {}, {}};

void invalidateEPMap() {
  // Set all endpoint identifiers to 'invalid'
  for (unsigned i = 0; i < TABLE_SIZE; ++i)
    state.FDMap[i].EP = EP_INVALID;
}

void __attribute__((constructor)) ipcopt_init() {
  invalidateEPMap();
  shm_state_restore();
}

void invalidate(endpoint ep) {
  ipc_info &i = getInfo(ep);
  assert(i.state != STATE_INVALID);
  assert(i.ref_count == 0);
  i.bytes_trans = 0;
  i.localfd = 0;
  i.state = STATE_INVALID;
  i.non_blocking = false;
}

void register_inet_socket(int fd) {
  // Freshly created socket
  ipclog("Registering socket fd=%d\n", fd);
  endpoint &ep = getEP(fd);
  // We better not think we already have an endpoint ID for this fd
  assert(ep == EP_INVALID);
  ep = ipcd_register_socket(fd);

  ipc_info &i = getInfo(ep);
  assert(i.ref_count == 0);
  assert(i.state == STATE_INVALID);
  i.bytes_trans = 0;
  i.localfd = 0;
  i.ref_count++;
  i.state = STATE_UNOPT;
  i.non_blocking = false;
}

void unregister_inet_socket(int fd) {
  if (!inbounds_fd(fd)) {
    return;
  }

  endpoint ep = getEP(fd);
  // Allow attempt to unregister fd's we don't
  // have endpoints for, this happens all the time :)
  if (ep == EP_INVALID) {
    return;
  }
  ipc_info &i = getInfo(ep);
  assert(i.state != STATE_INVALID);
  ipclog("Unregistering socket fd=%d, bytes transferred: %zu\n", fd,
         i.bytes_trans);

  assert(i.ref_count > 0);
  // FD no longer refers to this endpoint!
  getEP(fd) = EP_INVALID;
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

char is_registered_socket(int fd) { return getEP(fd) != EP_INVALID; }

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

  ipclog("** THIS IS TEMPORARY KLUDGE **\n");
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
  endpoint ep = getEP(fd);
  assert(valid_ep(ep));

  getInfo(ep).non_blocking = non_blocking;
}

void set_cloexec(int fd, bool cloexec) {
  getFDInfo(fd).close_on_exec = cloexec;
}
