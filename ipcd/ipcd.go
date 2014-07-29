package main

import "os"
import "fmt"

// Server entry point

func StartServer() {
	Context := NewContext()
	listenForClients(Context)
}

func main() {
	// TODO: Proper daemonification (per APUE book or equiv)
	LockFile := OpenLock()
	if LockFile == nil {
		fmt.Println("Unable to open lock file, ipcd already running?")
		os.Exit(1)
	}
	defer CleanupLock(LockFile)

	StartServer()
}
