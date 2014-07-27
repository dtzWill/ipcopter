//===-- epoll.h -------------------------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// epoll handling
//
//===----------------------------------------------------------------------===//

#ifndef _EPOLL_H_
#define _EPOLL_H_

#include <sys/epoll.h>

int __internal_epoll_create(int size);
int __internal_epoll_create1(int flags);
int __internal_epoll_pwait(int epfd, struct epoll_event *events, int maxevents,
                           int timeout, const sigset_t *sigmask);
int __internal_epoll_ctl(int epfd, int op, int fd, struct epoll_event *event);

#endif // _EPOLL_H_
