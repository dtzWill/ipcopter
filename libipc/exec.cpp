//===-- exec.cpp ----------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Intercept calls to exec*()
//
//===----------------------------------------------------------------------===//

#include "real.h"

#include "debug.h"
#include "getfromlibc.h"
#include "shm.h"
#include "wrapper.h"

EXTERN_C int __real_execve(const char *path, char *const argv[],
                           char *const envp[]) {
  CALL_REAL(execve, path, argv, envp);
}


int execve(const char *path, char *const argv[], char *const envp[]) {
  ipclog("execve called (path=%s)!\n", path);

  shm_state_save();

  return __real_execve(path, argv, envp);
}

