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

#include <assert.h>

bool rename_fd(int fd, int newfd) {
  // Duplicate to new fd
  int ret = __real_dup2(fd, newfd);
  if (ret == -1) {
    return false;
  }
  // Close the original fd we were given
  ret = __real_close(fd);
  if (ret != 0) {
    return false;
  }
  return true;
}
