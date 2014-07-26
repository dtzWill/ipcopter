//===-- debug.h -----------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Simple debugging macros
//
//===----------------------------------------------------------------------===//
#ifndef _DEBUG_LOG_
#define _DEBUG_LOG_

#include <unistd.h>
#include <stdio.h>

FILE *getlogfp();

#define ipclog(str, ...)                                                       \
  do {                                                                         \
    FILE *fp = getlogfp();                                                     \
    fprintf(fp, "[IPC] <%d> " str, getpid(), ##__VA_ARGS__);                   \
    fflush(fp);                                                                \
  } while (0)

#endif // _DEBUG_LOG_
