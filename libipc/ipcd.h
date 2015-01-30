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
#include <arpa/inet.h> // For INET6_ADDRSTRLEN :/

typedef uint32_t endpoint;

const endpoint EP_INVALID = ~endpoint(0);

typedef struct {
  char addr[INET6_ADDRSTRLEN];
  int port;
} netaddr;

typedef struct {
  netaddr src;
  netaddr dst;
  uint32_t s_crc;
  uint32_t r_crc;
  // TODO: Timing info
} pairing_info;

// Initialize connection
void __ipcd_init();

// Check if usage of ipcd is enabled or not
bool ipcd_enabled();

// REGISTER
endpoint ipcd_register_socket(int fd);

// LOCALIZE
bool ipcd_localize(endpoint local, endpoint remote);

// GETLOCALFD
int ipcd_getlocalfd(endpoint local);

// UNREGISTER
bool ipcd_unregister_socket(endpoint ep);

// REREGISTER
bool ipcd_reregister_socket(endpoint ep, int fd);

// ENDPOINT_KLUDGE
endpoint ipcd_endpoint_kludge(endpoint local);

// THRESH_CRC_KLUDGE
endpoint ipcd_crc_kludge(endpoint local, uint32_t s_crc, uint32_t r_crc,
                         bool last);

// FIND_PAIR
endpoint ipcd_find_pair(endpoint local, pairing_info &pi, bool last);

// Does ipcd need the specified fd?
bool ipcd_is_protected(int fd);

#endif // _IPCD_H_
