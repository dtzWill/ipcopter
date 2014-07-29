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

int __internal_epoll_create(int size) {
  // ipclog("epoll_create(size=%d)\n", size);
  return __real_epoll_create(size);
}
int __internal_epoll_create1(int flags) {
  // ipclog("epoll_create1(flags=%d)\n", flags);
  return __real_epoll_create1(flags);
}
int __internal_epoll_pwait(int epfd, struct epoll_event *events, int maxevents,
                           int timeout, const sigset_t *sigmask) {
  // ipclog("epoll_pwait(epfd=%d,...)\n", epfd);
  int ret = __real_epoll_pwait(epfd, events, maxevents, timeout, sigmask);

  // If no fd's are ready, or an error occurred, we're done.
  if (ret == -1 || ret == 0)
    return ret;

  assert(ret > 0);
  return ret;

  // TODO: Without tracking details of what fd's we replaced, we can't guarantee
  // that only one fd has the localfd returned by epoll.
  // For now, ignore, as it would be straightforward
  // and probably cheaper than this scan to track
  // information for epfd's.


  //for (int i = 0; i < ret; ++i) {
  //  int ready_fd = events[i].data.fd;
  //  // AFAIK 'fd' here is only usually the fd causing the event.
  //  if (!inbounds_fd(ready_fd))
  //    continue;

  //  }

  //}
  //for (int lfd = 0; lfd < TABLE_SIZE; ++lfd) {
  //  if (is_local(lfd)) {

  //  }
  //}

}
int __internal_epoll_ctl(int epfd, int op, int fd, struct epoll_event *event) {
  // ipclog("epoll_ctl(epfd=%d, op=%d, fd=%d, ...)\n", epfd, op, fd);

  // Intercept uses of optimized fd's, replace with local ones.
  // TODO: How to handle epoll of an fd optimized after it's added?
  if (is_optimized_socket_safe(fd)) {
    int localfd = getInfo(getEP(fd)).localfd;
    if (false) {
      ipclog("epoll_ctl(epfd=%d, op=%d) on optimized fd %d (localfd %d)!\n",
             epfd, op, fd, localfd);
    }
    switch (op) {
    case EPOLL_CTL_DEL: {
      // Remove both the fd and the localfd
      int ret1 = __real_epoll_ctl(epfd, op, fd, event);
      int ret2 = __real_epoll_ctl(epfd, op, localfd, event);
      // If successfully removed original, use its return value.
      if (ret1 != -1) {
        return ret1;
      }
      // Otherwise use return value of optimized fd.
      return ret2;
    }
    case EPOLL_CTL_MOD: {
      // Ensure non-optimized fd isn't in the epoll set:
      int ret = __real_epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
      if (ret == 0) {
        // Unoptimized fd is present, user is attempting to modify entry
        // for it.  We already removed the entry for unopt, so add
        // a new entry with localfd and new event to associate with it.
        return __real_epoll_ctl(epfd, EPOLL_CTL_ADD, localfd, event);
      } else {
        // Either user is attempting to modify without adding
        // or we already replaced the unoptimized fd with localfd.
        // Either way, do original operation with localfd instead:
        return __real_epoll_ctl(epfd, op, localfd, event);
      }
    }
    case EPOLL_CTL_ADD:
      // Add localfd instead
      return __real_epoll_ctl(epfd, op, localfd, event);
    }
    assert(0 && "Invalid epoll operation!\n");
  }

  // Not optimized, forward requested operation.
  return __real_epoll_ctl(epfd, op, fd, event);
}
