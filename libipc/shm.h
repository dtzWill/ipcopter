//===-- shm.h ---------------------------------------------------*- C++ -*-===//
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
// Save/restore via shared memory to survive exec*()
//
//===----------------------------------------------------------------------===//

#ifndef _SHM_H_
#define _SHM_H_

void shm_state_destroy();
void shm_state_restore();
void shm_state_save();

#endif // _SHM_H_
