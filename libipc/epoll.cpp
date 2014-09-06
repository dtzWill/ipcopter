//===-- epoll.cpp ---------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// epoll handling.
//
//===----------------------------------------------------------------------===//

#include "epoll.h"

#include "debug.h"
#include "ipcopt.h"
#include "ipcreg_internal.h"
#include "real.h"

#include <algorithm>

int __internal_epoll_create(int size) {
  // ipclog("epoll_create(size=%d)\n", size);
  int ret = __real_epoll_create(size);
  if (ret != -1) {
    fd_info &f = getFDInfo(ret);
    assert(!valid_ep(f.EP));
    assert(!f.is_local);
    assert(!f.close_on_exec);

    assert(!f.epoll.valid);

    f.epoll.count = 0;
    f.epoll.valid = true;
  }
  return ret;
}
int __internal_epoll_create1(int flags) {
  int ret = __real_epoll_create1(flags);
  if (ret != -1) {
    fd_info &f = getFDInfo(ret);
    assert(!valid_ep(f.EP));
    assert(!f.is_local);
    assert(!f.close_on_exec);

    assert(!f.epoll.valid);

    f.epoll.count = 0;
    f.close_on_exec = (flags & EPOLL_CLOEXEC) != 0;
    f.epoll.valid = true;
  }
  return ret;
  return __real_epoll_create1(flags);
}

epoll_entry *find_epoll_entry(int epfd, int fd) {
  epoll_info &ei = getEpollInfo(epfd);
  assert(ei.valid);
  for (unsigned i = 0; i < ei.count; ++i) {
    if (ei.entries[i].fd == fd) {
      return &ei.entries[i];
    }
  }

  return NULL;
}


int __internal_epoll_pwait(int epfd, struct epoll_event *events, int maxevents,
                           int timeout, const sigset_t *sigmask) {

  // Ensure this epfd doesn't have any references to fd's
  // that have since been replaced with local ones!
  epoll_info &ei = getEpollInfo(epfd);
  assert(ei.valid);

  for (unsigned i = 0; i < ei.count; ++i) {
    int fd = ei.entries[i].fd;
    if (is_optimized_socket_safe(fd)) {
      // Replace with localfd!
      int ret = __real_epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
      assert(ret == 0);
      int localfd = getInfo(getEP(fd)).localfd;
      ret =
          __real_epoll_ctl(epfd, EPOLL_CTL_ADD, localfd, &ei.entries[i].event);
      // Might fail if user adds multiple fd's with same description
      // to the same epoll set.  We don't support this currently.
      assert(ret == 0);
      ei.entries[i].fd = localfd;
    }
  }

  return __real_epoll_pwait(epfd, events, maxevents, timeout, sigmask);
}

int __internal_epoll_ctl(int epfd, int op, int fd, struct epoll_event *event) {
  epoll_info &ei = getEpollInfo(epfd);
  assert(ei.valid);

  int localfd = is_optimized_socket_safe(fd) ? getInfo(getEP(fd)).localfd : -1;
  epoll_entry *unopt_entry = find_epoll_entry(epfd, fd);
  epoll_entry *opt_entry =
      (localfd != -1) ? find_epoll_entry(epfd, localfd) : NULL;
  // Assert at most one exists:
  assert((opt_entry == NULL) || (unopt_entry == NULL));

  // Only one operation is valid for case where
  // neither entry exists:
  if (!opt_entry && !unopt_entry && op != EPOLL_CTL_ADD) {
    // Forward requested operation, it will fail regardless
    int ret = __real_epoll_ctl(epfd, op, fd, event);
    assert(ret == -1 && "Expected epoll_ctl() to fail, but didn't");
    return ret;
  }

  switch (op) {
  case EPOLL_CTL_ADD: {
    // Okay, we're adding it.

    // If already added, let real epoll return error:
    if (opt_entry) {
      int ret = __real_epoll_ctl(epfd, EPOLL_CTL_ADD, localfd, event);
      assert(ret == -1 && "Expected epoll_ctl() to fail, but didn't");
      return ret;
    } else if (unopt_entry) {
      int ret = __real_epoll_ctl(epfd, EPOLL_CTL_ADD, localfd, event);
      assert(ret == -1 && "Expected epoll_ctl() to fail, but didn't");
      return ret;
    } else {
      int addfd = (localfd != -1) ? localfd : fd;
      int ret = __real_epoll_ctl(epfd, EPOLL_CTL_ADD, addfd, event);
      // Add to our epoll entries list for this epfd:
      if (ret == 0) {
        epoll_entry &new_entry = ei.entries[ei.count++];
        new_entry.fd = addfd;
        new_entry.event = *event;

        assert(ei.count <= MAX_EPOLL_ENTRIES);
      } else {
        ipclog("EPOLL_CTL_ADD failed!\n");
      }
      return ret;
    }
  }
  case EPOLL_CTL_MOD: {
    // Modify whichever we have a valid entry for
    epoll_entry *entry = unopt_entry;
    int modfd = fd;
    if (opt_entry) {
      entry = opt_entry;
      modfd = localfd;
    }
    int ret = __real_epoll_ctl(epfd, op, modfd, event);
    if (ret == 0) {
      entry->event = *event;
    }
    return ret;
  }
  case EPOLL_CTL_DEL: {
    // Delete whichever we have entry for
    epoll_entry *entry = unopt_entry;
    int delfd = fd;
    if (opt_entry) {
      entry = opt_entry;
      delfd = localfd;
    }
    int ret = __real_epoll_ctl(epfd, op, delfd, event);
    if (ret == 0) {
      // Successfully deleted entry, remove from list
      --ei.count;
      if (ei.count > 0)
        std::swap(*entry, ei.entries[ei.count]);
    }
    return ret;
  }
  default:
    ipclog("Unexpected EPOLL op: %d\n", op);
    break;
  }

  // Invalid epoll operation, forward
  int ret = __real_epoll_ctl(epfd, op, fd, event);
  assert((ret == -1) && "Expected epoll_ctl() call to fail, but didn't");
  return ret;
}
