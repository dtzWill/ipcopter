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

#include "magic_socket_nums.h"
#include "real.h"
#include "rename_fd.h"
#include "wrapper.h"

#include <assert.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/stat.h>
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

  int logfd = open(buf, O_CLOEXEC | O_WRONLY | O_APPEND | O_CREAT, 0777);
  if (logfd == -1) {
    fprintf(stderr, "Error opening log file!");
    abort();
  }
  bool success = rename_fd(logfd, MAGIC_LOGGING_FD);
  if (!success) {
    fprintf(stderr, "Error duplicating logging fd!\n");
    abort();
  }

  logfp = fdopen(MAGIC_LOGGING_FD, "ae");
  // If unable to open log, attempt to print to stderr and bail
  if (!logfp) {
    fprintf(stderr, "Error opening log file!");
    abort();
  }
  fprintf(logfp, "Log file opened, pid=%d, oldpid=%d\n", newmypid, mypid);


  mypid = newmypid;

  return logfp;
}

extern "C" void __assert_fail(const char *assertion, const char *file,
                              unsigned int line, const char *function) {
  ipclog("Assertion '%s' failed at %s:%d in %s\n", assertion, file, line,
         function);
  abort();
}
