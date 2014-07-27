//===-- fork.cpp ----------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Intercept calls to fork()
//
//===----------------------------------------------------------------------===//

#include <unistd.h>

#include "ipcopt.h"

#include "debug.h"
#include "getfromlibc.h"
#include "system.h"
#include "wrapper.h"

BEGIN_EXTERN_C
pid_t __real_fork(void) { CALL_REAL(fork); }

pid_t __internal_fork(void) {
  // Re-registered all fd's before
  // passing to children.
  // This is bad, but makes it easy to avoid
  // racing child's reregistration against our closing them.
  register_inherited_fds();
  pid_t p = __real_fork();

  switch (p) {
  case -1:
    ipclog("Error in fork()!\n");
    break;
  case 0:
    // child
    ipclog("FORK! Parent is: %d\n", getppid());
    break;
  default:
    // parent
    ipclog("Fork! New child is: %d\n", p);
  }

  return p;
}

END_EXTERN_C
