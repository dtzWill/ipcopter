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
#include "real.h"

int __internal_epoll_create(int size) {
  ipclog("epoll_create(size=%d)\n", size);
  return __real_epoll_create(size);
}
int __internal_epoll_create1(int flags) {
  ipclog("epoll_create1(flags=%d)\n", flags);
  return __real_epoll_create1(flags);
}
int __internal_epoll_pwait(int epfd, struct epoll_event *events, int maxevents,
                           int timeout, const sigset_t *sigmask) {
  ipclog("epoll_pwait(epfd=%d,...)\n", epfd);
  return __real_epoll_pwait(epfd, events, maxevents, timeout, sigmask);
}
int __internal_epoll_ctl(int epfd, int op, int fd, struct epoll_event *event) {
  ipclog("epoll_ctl(epfd=%d, op=%d, fd=%d, ...)\n", epfd, op, fd);
  return __real_epoll_ctl(epfd, op, fd, event);
}
