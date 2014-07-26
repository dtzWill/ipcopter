//===-- shm.cpp -----------------------------------------------------------===//
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

#include "shm.h"

#include "ipcreg_internal.h"
#include "magic_socket_nums.h"
#include "real.h"
#include "rename_fd.h"

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>

static char buf[100];
const char *getShmName() {
  sprintf(buf, "/ipcd.%d\n", getpid());
  return buf;
}

int get_shm(int flags) {
  return shm_open(getShmName(), flags, 0600);
}

void shm_state_save() {
  // Create shared memory segment
  int fd = get_shm(O_RDWR | O_CREAT | O_EXCL);
  assert(fd != -1);

  bool success = rename_fd(fd, MAGIC_SHM_FD, /* cloexec */ false);
  assert(success && "Failed to rename SHM fd!");

  // Do *not* close on exec! :)
  int flags = __real_fcntl(MAGIC_SHM_FD, F_GETFD, /* kludge */ 0);
  assert(flags >= 0);
  flags &= ~FD_CLOEXEC;
  int ret = __real_fcntl_int(MAGIC_SHM_FD, F_SETFD, flags);
  assert(ret != -1);

  // Size the memory segment:
  ret = ftruncate(MAGIC_SHM_FD, sizeof(libipc_state));
  assert(ret != -1);

  void *stateptr = mmap(NULL, sizeof(libipc_state), PROT_READ | PROT_WRITE,
                  MAP_SHARED, MAGIC_SHM_FD, 0);
  assert(stateptr != MAP_FAILED);

  // Copy state to shared memory segment
  *(libipc_state*)stateptr = state;

  // Unmap, but don't unlink the shared memory
  ret = munmap(stateptr, sizeof(libipc_state));
  assert(ret != -1);

  ipclog("State saved!\n");
}

bool is_valid_fd(int fd) {
  return __real_fcntl(fd, F_GETFD, /* kludge */ 0) >= 0;
}

void shm_state_restore() {
  // If we have memory to restore, it'll be in our magic FD!
  if (!is_valid_fd(MAGIC_SHM_FD))
    return;

  ipclog("Inherited ipc state FD, starting state restoration...\n");

  void *stateptr = mmap(NULL, sizeof(libipc_state), PROT_READ | PROT_WRITE,
                        MAP_SHARED, MAGIC_SHM_FD, 0);
  // If our FD is still open, the memory better still be there!
  assert(stateptr != MAP_FAILED);

  // Copy state to shared memory segment
  // Copy state from shared memory segment
  state = *(libipc_state*)stateptr;

  int ret = munmap(stateptr, sizeof(libipc_state));
  assert(ret != -1);

  // Done with the shared segment, thank you!
  ret = __real_close(MAGIC_SHM_FD);
  assert(ret != -1);
  ret = shm_unlink(getShmName());
  assert(ret != -1);

  ipclog("State restored!\n");
}

void shm_state_destroy() {
  // If we have memory to restore, it'll be in our magic FD!
  assert(is_valid_fd(MAGIC_SHM_FD));

  int ret = __real_close(MAGIC_SHM_FD);
  assert(ret == 0);

  ret = shm_unlink(getShmName());
  assert(ret != -1);

  ipclog("Destroyed saved state!\n");
}
