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
#include "system.h"
#include "wrapper.h"

EXTERN_C int __real_execve(const char *path, char *const argv[],
                           char *const envp[]) {
  CALL_REAL(execve, path, argv, envp);
}

#define EXEC_WRAPPER(name, path, ...)                                          \
  ipclog(#name " called (path=%s)!\n", path);                                  \
  shm_state_save();                                                            \
  typedef typeof(name) *FPTY;                                                  \
  static FPTY FP = (FPTY)get_libc_func(#name);                                 \
  int ret = FP(path, __VA_ARGS__);                                             \
  shm_state_destroy();                                                         \
  return ret;

int __internal_execv(const char *path, char *const argv[]) {
  EXEC_WRAPPER(execv, path, argv);
}
int __internal_execve(const char *path, char *const argv[],
                      char *const envp[]) {
  EXEC_WRAPPER(execve, path, argv, envp);
}
int __internal_execvp(const char *file, char *const argv[]) {
  EXEC_WRAPPER(execvp, file, argv);
}
int __internal_execvpe(const char *file, char *const argv[],
                       char *const envp[]) {
  EXEC_WRAPPER(execvpe, file, argv, envp);
}
