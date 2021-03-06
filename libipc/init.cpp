//===-- init.h ------------------------------------------------------------===//
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
// Ensure proper initialization.
//
//===----------------------------------------------------------------------===//

#include "init.h"

#include "debug.h"
#include "ipcd.h"
#include "ipcopt.h"
#include "lock.h"

SimpleLock & getInitLock() {
  static SimpleLock InitLock;
  return InitLock;
}

void __ipc_init() {
  // Initialization time!

  // First, let's ensure we have our logger:
  ipclog("Init\n");

  // Next, let's initialize the libipc state:
  __ipcopt_init();

  // Finally, attempt connection to ipcd:
  __ipcd_init();
}

void __ensure_init_started() {
  // Attempt to take initialization lock:
  // if we can, proceed with initialization.
  // if we can't, initialization is already in-progress.
  // Since only constructors should call this,
  // this is likely due to triggering a constructor
  // while performing initialization.
  if (!getInitLock().TryLock())
    return;

  __ipc_init();
}
