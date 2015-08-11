//===-- magic_socket_nums.h -------------------------------------*- C++ -*-===//
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
// Definitions for magic socket numbers reserved for ipcd use.
//
//===----------------------------------------------------------------------===//

#ifndef _MAGIC_SOCKET_NUMS_H_
#define _MAGIC_SOCKET_NUMS_H_

const int MAGIC_SOCKET_FD = 999;

const int MAGIC_LOGGING_FD = 998;

const int MAGIC_SHM_FD = 997;

#endif // _MAGIC_SOCKET_NUMS_H_
