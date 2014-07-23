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
// XXX: Shouldn't need to include this for _real_*
#include "socket_inline.h"

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

const char *PRELOAD_PATH = "/etc/ld.so.preload";
const char *PRELOAD_TMP = "/tmp/preload.cfg";

void connect_to_ipcd() {
  int s, len;
  struct sockaddr_un remote;

  if ((s = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
    perror("socket");
    exit(1);
  }

  remote.sun_family = AF_UNIX;
  strcpy(remote.sun_path, SOCK_PATH);
  len = strlen(remote.sun_path) + sizeof(remote.sun_family);
  if (connect(s, (struct sockaddr *)&remote, len) == -1) {
    // XXX: Super kludge!
    // If we can't connect to daemon, assume it hasn't been
    // started yet and attempt to run it ourselves.
    // This code is terrible, and is likely to break
    // badly in many situations, but works for now.
    if (errno == ENOENT || errno == ECONNREFUSED) {
      rename(PRELOAD_PATH, PRELOAD_TMP);
      switch (fork()) {
      case -1:
        break;
      case 0:
        // Child
        execl(IPCD_BIN_PATH, IPCD_BIN_PATH, (char *)NULL);
        perror("Failed to exec ipcd");
        assert(0 && "Exec failed?");
        break;
      default:
        // Wait for ipcd to start :P
        ipclog("Starting ipcd...\n");
        sleep(1);
      }
      rename(PRELOAD_TMP, PRELOAD_PATH);
      if (connect(s, (struct sockaddr *)&remote, len) == -1) {
        perror("connect-after-fork");
        exit(1);
      }
    } else {
      perror("connect");
      ipclog("Error connecting to ipcd?\n");
      exit(1);
    }
  }

  ipclog("Connected to IPCD, fd=%d\n", s);
  mypid = getpid();
  ipcd_socket = s;
}

void __attribute__((constructor)) ipcd_init() {
  assert(ipcd_socket == 0);
  connect_to_ipcd();
}

void __attribute__((destructor)) ipcd_dtor() {
  if (ipcd_socket == 0) {
    ipclog("Exiting without establishing connection to ipcd...\n");
    return;
  }
  assert(ipcd_socket != 0);
  assert(mypid != 0);

  char buf[100];
  int len = sprintf(buf, "REMOVEALL %d\n", mypid);
  assert(len > 5);
  int err = __real_write(ipcd_socket, buf, len);
  if (err < 0) {
    perror("write");
    exit(1);
  }
  err = __real_read(ipcd_socket, buf, 50);
  assert(err > 5);

  const char *match = "200 REMOVED ";
  size_t matchlen = strlen(match);
  if (size_t(err) < matchlen) {
    perror("read");
    exit(1);
  }
  bool success = (strncmp(buf, "200 REMOVED ", matchlen) == 0);
  if (!success) {
    ipclog("Failure, response=%s\n", buf);
  }
  assert(success && "Unable to unregister all fd's");
}

static volatile int connect_lock = 0;
void ipcd_lock() {
  while (__sync_lock_test_and_set(&connect_lock, 1)) {
    // wait
  };
}

void ipcd_unlock() { __sync_lock_release(&connect_lock); }

struct IPCLock {
  IPCLock() { ipcd_lock(); }
  ~IPCLock() { ipcd_unlock(); }
};

void connect_if_needed() {
  if (mypid == getpid())
    return;

  ipclog("Reconnecting to ipcd in child...\n");
  close(ipcd_socket);
  connect_to_ipcd();
}

endpoint ipcd_register_socket(int fd) {
  IPCLock L;
  connect_if_needed();

  char buf[100];
  int len = sprintf(buf, "REGISTER %d %d\n", getpid(), fd);
  assert(len > 5);

  assert(ipcd_socket);
  // ipclog("REGISTER %d -->\n", fd);
  int err = __real_write(ipcd_socket, buf, len);
  if (err < 0) {
    perror("write");
    exit(1);
  }

  // ipclog("REGISTER %d <--\n", fd);
  err = __real_read(ipcd_socket, buf, 50);
  assert(err > 5);

  buf[err] = 0;
  int id;
  int n = sscanf(buf, "200 ID %d\n", &id);
  assert(n == 1);

  // ipclog("Registered and got endpoint id=%d\n", id);

  return id;
}

bool ipcd_localize(endpoint local, endpoint remote) {
  IPCLock L;
  connect_if_needed();

  char buf[100];
  int len = sprintf(buf, "LOCALIZE %d %d\n", local, remote);
  assert(len > 5);
  int err = __real_write(ipcd_socket, buf, len);
  if (err < 0) {
    perror("write");
    exit(1);
  }
  err = __real_read(ipcd_socket, buf, 50);
  assert(err > 5);

  return strncmp(buf, "200 OK\n", err) == 0;
}

// GETLOCALFD
int ipcd_getlocalfd(endpoint local) {
  IPCLock L;
  connect_if_needed();

  char buf[100];
  int len = sprintf(buf, "GETLOCALFD %d\n", local);
  assert(len > 5);
  int err = __real_write(ipcd_socket, buf, len);
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

    int ret = recvmsg(ipcd_socket, &msg, 0);
    if (ret <= 0) {
      perror("recvmsg");
      exit(1);
    }

    // TODO: Understand and fix mismatch between
    // CONTROLLEN and msg.msg_controllen after recvmsg()!
    memcpy(&fd, CMSG_DATA(cmsg), sizeof(int));
    ipclog("received local fd %d for endpoint %d\n", fd, local);
  }

  err = __real_read(ipcd_socket, buf, 50);
  assert(err > 5);

  bool success = strncmp(buf, "200 OK\n", err) == 0;
  assert(success);

  return fd;
}

// UNREGISTER
bool ipcd_unregister_socket(endpoint ep) {
  IPCLock L;
  connect_if_needed();

  char buf[100];
  int len = sprintf(buf, "UNREGISTER %d\n", ep);
  assert(len > 5);
  int err = __real_write(ipcd_socket, buf, len);
  if (err < 0) {
    perror("write");
    exit(1);
  }
  err = __real_read(ipcd_socket, buf, 50);
  assert(err > 5);

  return strncmp(buf, "200 OK\n", err) == 0;
}

endpoint ipcd_endpoint_kludge(endpoint local) {
  IPCLock L;
  connect_if_needed();

  char buf[100];
  int len = sprintf(buf, "ENDPOINT_KLUDGE %d\n", local);
  assert(len > 5);
  int err = __real_write(ipcd_socket, buf, len);
  if (err < 0) {
    perror("write");
    exit(1);
  }
  err = __real_read(ipcd_socket, buf, 50);
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
