package main

import (
	"os"
	"syscall"
)

const LOCKFILE = "/tmp/ipcd.pid"
const LOCKMODE = (syscall.S_IRUSR | syscall.S_IWUSR | syscall.S_IRGRP | syscall.S_IROTH)

func Lock(F *os.File) error {
	return syscall.Flock(int(F.Fd()), syscall.LOCK_EX|syscall.LOCK_NB)
}

func Unlock(F *os.File) error {
	return syscall.Flock(int(F.Fd()), syscall.LOCK_UN)
}

func OpenLock() *os.File {
	F, err := os.OpenFile(LOCKFILE, os.O_RDWR|os.O_CREATE, LOCKMODE)
	if err != nil {
		// Unable to open lock file at all, definitely didn't work.
		return nil
	}

	if err := Lock(F); err != nil {
		return nil
	}

	return F
}

func CleanupLock(F *os.File) {
	defer F.Close()

	if err := Unlock(F); err != nil {
		panic("Unable to unlock file")
	}
	// .. unlink?
}
