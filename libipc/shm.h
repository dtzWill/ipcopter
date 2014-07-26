//===-- shm.h ---------------------------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
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
