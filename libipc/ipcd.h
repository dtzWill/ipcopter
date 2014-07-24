//===-- ipcd.h ------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Interface for communicating with IPCD
//
//===----------------------------------------------------------------------===//

#ifndef _IPCD_H_
#define _IPCD_H_

#include <stdint.h>

typedef uint32_t endpoint;

const endpoint EP_INVALID = ~endpoint(0);

// REGISTER
endpoint ipcd_register_socket(int fd);

// LOCALIZE
bool ipcd_localize(endpoint local, endpoint remote);

// GETLOCALFD
int ipcd_getlocalfd(endpoint local);

// UNREGISTER
bool ipcd_unregister_socket(endpoint ep);

// REREGISTER
bool ipcd_reregister_socket(int fd, endpoint ep);

// ENDPOINT_KLUDGE
endpoint ipcd_endpoint_kludge(endpoint local);

// Does ipcd need the specified fd?
bool ipcd_is_protected(int fd);

#endif // _IPCD_H_
