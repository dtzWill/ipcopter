//===-- debug.cpp ---------------------------------------------------------===//
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
#include <stdio.h>
#include <stdlib.h>
#include <sys/errno.h>
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


  const char *log_dir = "/tmp/ipcd";
  char buf[100];

  int err = mkdir(log_dir, 0777);
  if (err == -1) {
    // It's okay if it already exists, expected even.
    if (errno != EEXIST) {
      fprintf(stderr, "Error creating log directory\n");
      abort();
    }
  }
  // Okay, directory exists.
  // Make an attempt at giving it permissive perms
  // Ignore return value for now...
  (void)chmod(log_dir, 0777);
  //if (chmod(log_dir, 0777) == -1)
  //  fprintf(stderr, "Error setting logdir permissions, continuing...\n");

  sprintf(buf, "%s/%d.log", log_dir, newmypid);

  int logfd = open(buf, O_CLOEXEC | O_WRONLY | O_APPEND | O_CREAT, 0666);
  if (logfd == -1) {
    fprintf(stderr, "Error opening log file!");
    abort();
  }
  // Best-effort to make log accessible to everyone...
  if (fchmod(logfd, 0666) == -1)
    fprintf(stderr, "Error setting log permissions, continuing...\n");
  bool success = rename_fd(logfd, MAGIC_LOGGING_FD, /* cloexec */ true);
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
  // Try to print same error to stderr
  fprintf(stderr, "Assertion '%s' failed at %s:%d in %s\n", assertion, file,
          line, function);
  abort();
}
