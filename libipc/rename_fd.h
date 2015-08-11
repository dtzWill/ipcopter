//===-- rename_fd.h ---------------------------------------------*- C++ -*-===//
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
// Helper for moving a file descriptor to somewhere
// that code is unlikely to attempt to attempt to use
// as a target of dup2(), etc.
//
//===----------------------------------------------------------------------===//

#ifndef _RENAME_FD_H_
#define _RENAME_FD_H_

// Attempt to duplicate fd into newfd and close fd,
// effectively 'renaming' fd to newfd.
// newfd will be closed if previously open.
// 'cloexec' indicates whether newfd should
// be set to CLOSE-ON-EXEC or not.
// Returns true on success.
bool rename_fd(int fd, int newfd, bool cloexec);

#endif // _RENAME_FD_H_
