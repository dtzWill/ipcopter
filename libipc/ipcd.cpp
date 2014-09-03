//===-- ipcd.cpp ----------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Implements IPCD interface, communicates with ipcd via control socket
//
//===----------------------------------------------------------------------===//

#include "ipcd.h"

#include "debug.h"
#include "real.h"
#include "rename_fd.h"
#include "magic_socket_nums.h"
#include "lock.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

static int ipcd_socket = 0;
static int mypid = 0;

// Must match that used by server,
// currently defined in ipcd/clients.go
const char *SOCK_PATH = "/tmp/ipcd.sock";
const char *IPCD_BIN_PATH = "/bin/ipcd";

const useconds_t SLEEP_AFTER_IPCD_START_INTERVAL = 10 * 1000; // 10ms?

SimpleLock &getConnectLock() {
  static SimpleLock ConnectLock;
  return ConnectLock;
}

bool we_are_ipcd;

void check_if_we_are_ipcd() {
  // man 3 program_invocation_name
  extern char *program_invocation_short_name;
  assert(program_invocation_short_name);

  ipclog("program_invocation_short_name: %s\n", program_invocation_short_name);

  we_are_ipcd = strcmp("ipcd", program_invocation_short_name) == 0;
}

void start_ipcd() {
  int d = daemon(0, 0);
  if (d == -1) {
    perror("Failed to daemonize");
    exit(1);
  }

  execl(IPCD_BIN_PATH, IPCD_BIN_PATH, (char *)NULL);
  perror("Failed to exec ipcd");
  exit(1);
}

void fork_ipcd() {
  switch (__real_fork()) {
  case -1:
    break;
  case 0:
    // Child
    start_ipcd();
    assert(0 && "Exec failed?");
    break;
  default:
    // Wait for ipcd to start :P
    ipclog("Starting ipcd...\n");
  }
}

void connect_to_ipcd() {
  int s, len;
  struct sockaddr_un remote;

  if ((s = __real_socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0)) == -1) {
    perror("socket");
    exit(1);
  }
  bool rename_success = rename_fd(s, MAGIC_SOCKET_FD, /* cloexec */ true);
  assert(rename_success);
  s = MAGIC_SOCKET_FD;

  remote.sun_family = AF_UNIX;
  strcpy(remote.sun_path, SOCK_PATH);
  len = strlen(remote.sun_path) + sizeof(remote.sun_family);
  if (__real_connect(s, (struct sockaddr *)&remote, len) == -1) {
    // XXX: Super kludge!
    // If we can't connect to daemon, assume it hasn't been
    // started yet and attempt to run it ourselves.
    // This code is terrible, and is likely to break
    // badly in many situations, but works for now.
    if (errno == ENOENT || errno == ECONNREFUSED) {
      fork_ipcd();

      int attempts = 0;
      const int MAX_ATTEMPTS = 10;
      while (attempts < MAX_ATTEMPTS) {
        usleep(SLEEP_AFTER_IPCD_START_INTERVAL);
        if (__real_connect(s, (struct sockaddr *)&remote, len) != -1)
          break;
        ++attempts;
      }
      if (attempts == MAX_ATTEMPTS) {
        perror("Unable to connect to ipcd after attempting to start it");
        exit(1);
      }
    } else {
      // Race with socket creation and permissions, maybe?
      ipclog("Connect failed, attempting a few more times...\n");
      int attempts = 0;
      const int MAX_ATTEMPTS = 10;
      while (attempts < MAX_ATTEMPTS) {
        usleep(SLEEP_AFTER_IPCD_START_INTERVAL);
        if (__real_connect(s, (struct sockaddr *)&remote, len) != -1)
          break;
        ++attempts;
      }
      if (attempts == MAX_ATTEMPTS) {
        perror("Connect to ipcd socket");
        ipclog("Error connecting to ipcd?\n");
        exit(1);
      }
    }
  }

  ipclog("Connected to IPCD, fd=%d\n", s);
  mypid = getpid();
  ipcd_socket = s;
}

void __ipcd_init() {
  assert(ipcd_socket == 0);
  check_if_we_are_ipcd();
  if (ipcd_enabled())
    connect_to_ipcd();
}

void __attribute__((destructor)) ipcd_dtor() {
  if (ipcd_socket == 0) {
    ipclog("Exiting without establishing connection to ipcd...\n");
    return;
  }
  assert(ipcd_socket != 0);
  assert(mypid != 0);

  // TODO: REMOVEALL is goodness, but we don't presently
  // correctly associate PID's with endpoints they own in ipcd.
  // Partially due to the mechanism for REREGISTER being
  // performed 'eagerly' before the child PID is known,
  // and partially due to a need to update ipcd's datastructures.

  // This needs to be done eventually anyway, to provide
  // access control restrictions on who can unregister,
  // optimize, etc. a given endpoint.

  // For now, explicitly unregister all endpoints we know we own.
  // XXX: This is done in ipcopt.cpp since this code
  // doesn't have access to our state table, just ipcd.

  // Return instead of REMOVEALL to avoid
  // duplicate UNREGISTER.
  return;

  char buf[100];
  int len = sprintf(buf, "REMOVEALL %d\n", mypid);
  assert(len > 5);
  int err = __real_send(ipcd_socket, buf, len, MSG_NOSIGNAL);
  if (err < 0) {
    ipclog("Error sending ipcd REMOVEALL command in dtor: %s\n",
           strerror(errno));
    return;
  }
  err = __real_recv(ipcd_socket, buf, 50, MSG_NOSIGNAL);
  assert(err > 5);

  const char *match = "200 REMOVED ";
  size_t matchlen = strlen(match);
  if (size_t(err) < matchlen) {
    ipclog("Error receiving response from ipcd in dtor: %s\n", strerror(errno));
    return;
  }
  bool success = (strncmp(buf, "200 REMOVED ", matchlen) == 0);
  if (!success) {
    ipclog("Failure, response=%s\n", buf);
  }
  if (success)
    ipclog("Successfully unregistered all fd's\n");
  else
    ipclog("Failed to remove all fd's\n");
}

void connect_if_needed() {
  if (mypid == getpid())
    return;

  ipclog("Reconnecting to ipcd in child...\n");
  __real_close(ipcd_socket);
  connect_to_ipcd();
}

endpoint ipcd_register_socket(int fd) {
  ScopedLock L(getConnectLock());
  connect_if_needed();

  char buf[100];
  int len = sprintf(buf, "REGISTER %d %d\n", getpid(), fd);
  assert(len > 5);

  assert(ipcd_socket);
  // ipclog("REGISTER %d -->\n", fd);
  int err = __real_send(ipcd_socket, buf, len, MSG_NOSIGNAL);
  if (err < 0) {
    perror("write");
    exit(1);
  }

  // ipclog("REGISTER %d <--\n", fd);
  err = __real_recv(ipcd_socket, buf, 50, MSG_NOSIGNAL);
  assert(err > 5);

  buf[err] = 0;
  int id;
  int n = sscanf(buf, "200 ID %d\n", &id);
  assert(n == 1);

  // ipclog("Registered and got endpoint id=%d\n", id);

  return id;
}

bool ipcd_localize(endpoint local, endpoint remote) {
  ScopedLock L(getConnectLock());
  connect_if_needed();

  char buf[100];
  int len = sprintf(buf, "LOCALIZE %d %d\n", local, remote);
  assert(len > 5);
  int err = __real_send(ipcd_socket, buf, len, MSG_NOSIGNAL);
  if (err < 0) {
    perror("write");
    exit(1);
  }
  err = __real_recv(ipcd_socket, buf, 50, MSG_NOSIGNAL);
  assert(err > 5);

  return strncmp(buf, "200 OK\n", err) == 0;
}

// GETLOCALFD
int ipcd_getlocalfd(endpoint local) {
  ScopedLock L(getConnectLock());
  connect_if_needed();

  char buf[100];
  int len = sprintf(buf, "GETLOCALFD %d\n", local);
  assert(len > 5);
  int err = __real_send(ipcd_socket, buf, len, MSG_NOSIGNAL);
  if (err < 0) {
    perror("write");
    exit(1);
  }

  int fd;
  {
    // Lots of magic
    const size_t CONTROLLEN = CMSG_LEN(sizeof(int));
    struct iovec iov[1];
    struct msghdr msg;
    struct cmsghdr *cmsg;
    char cmsg_buf[CONTROLLEN];
    cmsg = (struct cmsghdr *)cmsg_buf;

    memset(&msg, 0, sizeof(msg));

    iov[0].iov_base = buf;
    iov[0].iov_len = sizeof(buf);
    msg.msg_iov = iov;
    msg.msg_iovlen = 1;
    msg.msg_control = cmsg;
    msg.msg_controllen = CONTROLLEN;

    int ret = __real_recvmsg(ipcd_socket, &msg, MSG_NOSIGNAL);
    if (ret <= 0) {
      perror("recvmsg");
      exit(1);
    }

    // TODO: Understand and fix mismatch between
    // CONTROLLEN and msg.msg_controllen after recvmsg()!
    memcpy(&fd, CMSG_DATA(cmsg), sizeof(int));
    ipclog("received local fd %d for endpoint %d\n", fd, local);
  }

  err = __real_recv(ipcd_socket, buf, 50, MSG_NOSIGNAL);
  assert(err > 5);

  bool success = strncmp(buf, "200 OK\n", err) == 0;
  assert(success);

  return fd;
}

// UNREGISTER
bool ipcd_unregister_socket(endpoint ep) {
  ScopedLock L(getConnectLock());
  connect_if_needed();

  char buf[100];
  int len = sprintf(buf, "UNREGISTER %d\n", ep);
  assert(len > 5);
  int err = __real_send(ipcd_socket, buf, len, MSG_NOSIGNAL);
  if (err < 0) {
    perror("write");
    exit(1);
  }
  err = __real_recv(ipcd_socket, buf, 50, MSG_NOSIGNAL);
  assert(err > 5);

  return strncmp(buf, "200 OK\n", err) == 0;
}

// REREGISTER
bool ipcd_reregister_socket(endpoint ep, int fd) {
  ScopedLock L(getConnectLock());
  connect_if_needed();

  char buf[100];
  int len = sprintf(buf, "REREGISTER %d %d %d\n", ep, getpid(), fd);
  assert(len > 5);
  int err = __real_send(ipcd_socket, buf, len, MSG_NOSIGNAL);
  if (err < 0) {
    perror("write");
    exit(1);
  }
  err = __real_recv(ipcd_socket, buf, 50, MSG_NOSIGNAL);
  assert(err > 5);

  return strncmp(buf, "200 OK\n", err) == 0;
}

endpoint ipcd_endpoint_kludge(endpoint local) {
  ScopedLock L(getConnectLock());
  connect_if_needed();

  char buf[100];
  int len = sprintf(buf, "ENDPOINT_KLUDGE %d\n", local);
  assert(len > 5);
  int err = __real_send(ipcd_socket, buf, len, MSG_NOSIGNAL);
  if (err < 0) {
    perror("write");
    exit(1);
  }
  err = __real_recv(ipcd_socket, buf, 50, MSG_NOSIGNAL);
  assert(err > 5);

  buf[err] = 0;
  int id;
  ipclog("endpoint_kludge(%d) = %s\n", local, buf);
  int n = sscanf(buf, "200 PAIR %d\n", &id);
  if (n == 1) {
    return id;
  }

  return EP_INVALID;
}

endpoint ipcd_crc_kludge(endpoint local, uint32_t s_crc, uint32_t r_crc) {
  ScopedLock L(getConnectLock());
  connect_if_needed();

  char buf[100];
  int len = sprintf(buf, "THRESH_CRC_KLUDGE %d %d %d\n", local, s_crc, r_crc);
  assert(len > 5);
  int err = __real_send(ipcd_socket, buf, len, MSG_NOSIGNAL);
  if (err < 0) {
    perror("write");
    exit(1);
  }
  err = __real_recv(ipcd_socket, buf, 50, MSG_NOSIGNAL);
  assert(err > 5);

  buf[err] = 0;
  int id;
  ipclog("crc_kludge(%d, %d, %d) = %s\n", local, s_crc, r_crc, buf);
  int n = sscanf(buf, "200 PAIR %d\n", &id);
  if (n == 1) {
    return id;
  }

  return EP_INVALID;
}

bool ipcd_is_protected(int fd) {
  return fd == ipcd_socket;
}

bool ipcd_enabled() {
  static bool disabled = getenv("IPCD_DISABLE") || we_are_ipcd;
  return !disabled;
}
