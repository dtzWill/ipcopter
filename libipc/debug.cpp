//===-- debug.cpp ---------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Debug logging implementation.
//
//===----------------------------------------------------------------------===//

#include "debug.h"

#include <assert.h>
#include <stdlib.h>
#include <unistd.h>

static int mypid = 0;
static FILE *logfp = NULL;

FILE *getlogfp() {

  int newmypid = getpid();
  if (mypid == newmypid) {
    return logfp;
  }

#if 0
  // Simply log to stderr unless it's not a tty
  if (isatty(2)) {
    assert(fileno(stderr) == 2);
    return stderr;
  }
#endif

  // TODO: Definitely not thread-safe O:)

  if (logfp) {
    fclose(logfp);
    logfp = NULL;
  }

  const char *pid_template = "/tmp/ipcd.%d.log";
  char buf[100];

  sprintf(buf, pid_template, newmypid);

  logfp = fopen(buf, "w");
  assert(logfp);

  mypid = newmypid;

  return logfp;
}

extern "C" void __assert_fail(const char *assertion, const char *file,
                              unsigned int line, const char *function) {
  ipclog("Assertion '%s' failed at %s:%d in %s\n", assertion, file, line,
         function);
  abort();
}
