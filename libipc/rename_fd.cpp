//===-- rename_fd.cpp -----------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Helper for moving a file descriptor to somewhere
// that code is unlikely to attempt to attempt to use
// as a target of dup2(), etc.
//
//===----------------------------------------------------------------------===//

#include "rename_fd.h"

#include "real.h"
#include "init.h"

#include <assert.h>
#include <fcntl.h>

bool rename_fd(int fd, int newfd, bool cloexec) {
  // Duplicate to new fd
  int cmd = cloexec ? F_DUPFD_CLOEXEC : F_DUPFD;
  int ret = __real_fcntl_int(fd, cmd, newfd);
  if (ret == -1) {
    return false;
  }
  assert(ret == newfd);
  // Close the original fd we were given
  ret = __real_close(fd);
  if (ret != 0) {
    return false;
  }
  return true;
}
