//===-- system.h ------------------------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Declarations of intercepted calls for system actions like fork/exec
//
//===----------------------------------------------------------------------===//

#ifndef _SYSTEM_H_
#define _SYSTEM_H_

#include <assert.h>
#include <stdarg.h>
#include <unistd.h>

#include "wrapper.h"

BEGIN_EXTERN_C
int __internal_execv(const char *path, char *const argv[]);
int __internal_execve(const char *path, char *const argv[], char *const envp[]);
int __internal_execvp(const char *file, char *const argv[]);
int __internal_execvpe(const char *file, char *const argv[],
                       char *const envp[]);
pid_t __internal_fork(void);

// Macro for execl* arg gathering:
#define BUILD_ARGV(arg)                                                        \
  const char *_argv[100];                                                      \
  int argc = 0;                                                                \
  va_list ap;                                                                  \
  va_start(ap, arg);                                                           \
  _argv[0] = arg;                                                              \
  while (_argv[argc]) {                                                        \
    argc++;                                                                    \
    assert(argc < 100);                                                        \
    _argv[argc] = va_arg(ap, const char *);                                    \
  }                                                                            \
  va_end(ap);                                                                  \
  char *const *argv = (char *const *)_argv;

END_EXTERN_C

#endif // _SYSTEM_H_
