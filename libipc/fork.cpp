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

#include "getfromlibc.h"
#include "wrapper.h"
#include "debug.h"

BEGIN_EXTERN_C
pid_t __real_fork(void) { CALL_REAL(fork); }

pid_t fork(void) {
  pid_t p = __real_fork();

  switch (p) {
  case -1:
    ipclog("Error in fork()!\n");
    break;
  case 0:
    // child
    // TODO: If parent close()'s any fd's we inherit
    // (common pattern in servers) it will attempt to unregister
    // the endpoints from the server.
    // We need a way to handle this, perhaps ref counting?
    ipclog("FORK! Parent is: %d\n", getppid());
    break;
  default:
    // parent
    ipclog("Fork! New child is: %d\n", p);
  }

  return p;
}

END_EXTERN_C
