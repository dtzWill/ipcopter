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

#include <unistd.h>

#include "wrapper.h"

BEGIN_EXTERN_C
int __internal_execve(const char *path, char *const argv[],
                      char *const envp[]);
pid_t __internal_fork(void);
END_EXTERN_C

#endif // _SYSTEM_H_
