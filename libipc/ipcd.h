//===-- ipcd.h ------------------------------------------------------------===//
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
  uint32_t s_crc;
  uint32_t r_crc;
} pairing_info;

typedef struct {
  bool is_accept;
  struct timespec connect_start;
  struct timespec connect_end;
  netaddr src;
  netaddr dst;
} endpoint_info;

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

bool ipcd_endpoint_info(endpoint local, endpoint_info &ei);

#endif // _IPCD_H_
