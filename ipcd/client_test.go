package main

// Test basic functionality as clients

import (
	"bufio"
	"fmt"
	"net"
	"os"
	"os/exec"
	"strings"
	"testing"
	"time"
)

// Run server daemon, assumes no server running already.
func StartServerProcess() *os.Process {
	cmd := exec.Command("./ipcd")
	err := cmd.Start()
	if err != nil {
		panic(err)
	}
	time.Sleep(time.Second / 15)
	return cmd.Process
}

func Stop(P *os.Process) {
	err := P.Kill()
	if err != nil {
		panic("Unable to kill ipcd??")
	}
	P.Wait()
}

// Perform indicated request, return server response string.
// Fails test if anythin goes wrong.
func DoReq(req string, t *testing.T) string {
	c, err := net.Dial("unix", SOCKET_PATH)
	if err != nil {
		t.Fatal(err)
	}
	defer c.Close()

	c.Write([]byte(req))

	r := bufio.NewReader(c)
	line, err := r.ReadBytes('\n')
	if err != nil {
		t.Fatal(err)
	}

	return strings.TrimSuffix(string(line), "\n")
}

// Perform requets and verify response from server matches 'exp'
// Fail the test if this is not the case.
func CheckReq(req string, exp string, t *testing.T) {
	resp := DoReq(req, t)
	if resp != exp {
		t.Fatalf("Unexpected response string '%s', expected '%s'", resp, exp)
	}
}

// Verify basic REGISTER support.
// Makes assumptions about endpoint id assignment behavior.
func TestRegister(t *testing.T) {
	P := StartServerProcess()
	defer Stop(P)

	CheckReq("REGISTER 1 10\n", "200 ID 0", t)
}

// Verify server rejects obviously bogus command:
func TestBadCommand(t *testing.T) {
	P := StartServerProcess()
	defer Stop(P)

	CheckReq("INVALID 1 10\n", "300 Unrecognized command", t)
}

// Verify multiple registrations from different clients work as expected.
func TestMultipleRegister(t *testing.T) {
	P := StartServerProcess()
	defer Stop(P)

	CheckReq("REGISTER 1 10\n", "200 ID 0", t)
	CheckReq("REGISTER 1 15\n", "200 ID 1", t)
	CheckReq("REGISTER 2 1\n", "200 ID 2", t)
}

// Ensure we can register multiple endpoints from same
// connection to the server.
func TestMultipleRegisterSameConn(t *testing.T) {
	P := StartServerProcess()
	defer Stop(P)

	c, err := net.Dial("unix", SOCKET_PATH)
	defer c.Close()
	if err != nil {
		t.Fatal(err)
	}
	c.Write([]byte("REGISTER 1 10\n"))
	r := bufio.NewReader(c)
	line, err := r.ReadBytes('\n')
	if err != nil {
		t.Fatal(err)
	}

	resp := strings.TrimSuffix(string(line), "\n")
	if resp != "200 ID 0" {
		t.Fatalf("Unexpected response: %s\n", resp)
	}

	c.Write([]byte("REGISTER 1 15\n"))
	line, err = r.ReadBytes('\n')
	if err != nil {
		t.Fatal(err)
	}
	resp = strings.TrimSuffix(string(line), "\n")
	if resp != "200 ID 1" {
		t.Fatalf("Unexpected response: %s\n", resp)
	}
}

// Verify basic LOCALIZE functionality.
func TestLocalize(t *testing.T) {
	P := StartServerProcess()
	defer Stop(P)

	CheckReq("REGISTER 1 10\n", "200 ID 0", t)
	CheckReq("REGISTER 1 5\n", "200 ID 1", t)
	CheckReq("LOCALIZE 0 1\n", "200 OK", t)
}

// Ensure server is okay with redundant LOCALIZE messages.
func TestLocalizeRedundant(t *testing.T) {
	P := StartServerProcess()
	defer Stop(P)

	CheckReq("REGISTER 1 10\n", "200 ID 0", t)
	CheckReq("REGISTER 1 5\n", "200 ID 1", t)
	CheckReq("LOCALIZE 0 1\n", "200 OK", t)
	CheckReq("LOCALIZE 0 1\n", "200 OK", t)
	CheckReq("LOCALIZE 0 1\n", "200 OK", t)
	CheckReq("LOCALIZE 1 0\n", "200 OK", t)
}

// Helper that obtains localized FD for the specified endpoint ID.
// Fails the test if anything goes wrong.
func GetLocalFDFor(ID int, t *testing.T) int {
	c, err := net.Dial("unix", SOCKET_PATH)
	defer c.Close()
	if err != nil {
		t.Fatal(err)
	}
	c.Write([]byte(fmt.Sprintf("GETLOCALFD %d\n", ID)))

	unixConn, ok := c.(*net.UnixConn)
	if !ok {
		t.Fatalf("unexpected type; expected UnixConn, got %T", unixConn)
	}

	U := New(unixConn)
	defer U.Close()

	fd, err := U.ReadFD()
	if err != nil {
		t.Fatal(err)
	}

	return fd
}

// Start-to-finish functionality check:
// * Register two endpoints
// * Notify server they're local and should be optimized
// * Request localized FD's for the optimized endpoints
// * Verify what we write in one of these comes out the other one
func TestLocalizeFDs(t *testing.T) {
	P := StartServerProcess()
	defer Stop(P)

	CheckReq("REGISTER 1 10\n", "200 ID 0", t)
	CheckReq("REGISTER 1 5\n", "200 ID 1", t)
	CheckReq("LOCALIZE 0 1\n", "200 OK", t)

	// Get localized FD's
	fd1 := GetLocalFDFor(0, t)
	fd2 := GetLocalFDFor(1, t)

	// Test connectivity!
	F1 := os.NewFile(uintptr(fd1), "f1")
	F2 := os.NewFile(uintptr(fd2), "f2")

	F1.Write([]byte("Testing\n"))
	r := bufio.NewReader(F2)
	line, err := r.ReadBytes('\n')
	if err != nil {
		t.Fatal(err)
	}
	if string(line) != "Testing\n" {
		t.Fatal("Failed to communicate over localized fd's\n")
	}
}

// Verify basic UNREGISTER support works,
// as well as ensure error is returned if client
// attempts to unregister endpoint twice or doesn't exist.
func TestUnregister(t *testing.T) {
	P := StartServerProcess()
	defer Stop(P)

	CheckReq("REGISTER 1 10\n", "200 ID 0", t)
	CheckReq("REGISTER 1 5\n", "200 ID 1", t)
	CheckReq("REGISTER 1 9\n", "200 ID 2", t)
	// Valid
	CheckReq("UNREGISTER 1\n", "200 OK", t)
	CheckReq("UNREGISTER 0\n", "200 OK", t)
	CheckReq("UNREGISTER 2\n", "200 OK", t)
	// Invalid
	CheckReq("UNREGISTER 1\n", "303 Invalid Endpoint ID '1'", t)
	CheckReq("UNREGISTER 3\n", "303 Invalid Endpoint ID '3'", t)

}

// Verify REMOVEALL command successfully
// unregisters all endpoints for specified pid.
func TestRemoveAll(t *testing.T) {
	P := StartServerProcess()
	defer Stop(P)

	CheckReq("REGISTER 1 10\n", "200 ID 0", t)
	CheckReq("REGISTER 1 5\n", "200 ID 1", t)
	CheckReq("REGISTER 1 9\n", "200 ID 2", t)
	CheckReq("REGISTER 2 1\n", "200 ID 3", t)

	CheckReq("REMOVEALL 1\n", "200 REMOVED 3", t)
	CheckReq("REMOVEALL 3\n", "200 REMOVED 0", t)
	CheckReq("REMOVEALL 2\n", "200 REMOVED 1", t)

}

func TestEndpointKludge(t *testing.T) {
	P := StartServerProcess()
	defer Stop(P)

	CheckReq("REGISTER 1 10\n", "200 ID 0", t)
	CheckReq("REGISTER 1 15\n", "200 ID 1", t)
	CheckReq("REGISTER 1 20\n", "200 ID 2", t)
	CheckReq("REGISTER 2 10\n", "200 ID 3", t)
	CheckReq("REGISTER 2 15\n", "200 ID 4", t)

	// I want NOPAIR to be different than 200 (201?),
	// but current code makes that more trouble than it's
	// worth.
	CheckReq("ENDPOINT_KLUDGE 0\n", "200 NOPAIR", t)
	CheckReq("ENDPOINT_KLUDGE 0\n", "200 NOPAIR", t)
	CheckReq("ENDPOINT_KLUDGE 0\n", "200 NOPAIR", t)
	CheckReq("ENDPOINT_KLUDGE 1\n", "200 PAIR 0", t)
	CheckReq("ENDPOINT_KLUDGE 1\n", "200 PAIR 0", t)
	CheckReq("ENDPOINT_KLUDGE 0\n", "200 PAIR 1", t)

	CheckReq("LOCALIZE 0 1\n", "200 OK", t)
}

func TestReregister(t *testing.T) {
	P := StartServerProcess()
	defer Stop(P)

	CheckReq("REGISTER 1 10\n", "200 ID 0", t)
	CheckReq("REGISTER 1 15\n", "200 ID 1", t)
	CheckReq("REGISTER 1 20\n", "200 ID 2", t)

	// oops we "fork()'d"
	CheckReq("REREGISTER 0 2 10\n", "200 OK", t)
	CheckReq("REREGISTER 3 2 15\n", "303 Invalid Endpoint ID '3'", t)

	// Single ref
	CheckReq("UNREGISTER 1\n", "200 OK", t)

	// Two refs, reduce to one
	CheckReq("UNREGISTER 0\n", "200 OK", t)
	// Reduce to none
	CheckReq("UNREGISTER 0\n", "200 OK", t)
	// Error
	CheckReq("UNREGISTER 0\n", "303 Invalid Endpoint ID '0'", t)
}

func TestLongLivedEndpointIDReuse(t *testing.T) {
	P := StartServerProcess()
	defer Stop(P)

	for i := 0; i < 100; i++ {
		CheckReq("REGISTER 1 10\n", "200 ID 0", t)
		CheckReq("UNREGISTER 0\n", "200 OK", t)
	}
}

func TestCRCKludge(t *testing.T) {
	P := StartServerProcess()
	defer Stop(P)

	CheckReq("REGISTER 1 10\n", "200 ID 0", t)
	CheckReq("REGISTER 1 15\n", "200 ID 1", t)
	CheckReq("REGISTER 1 20\n", "200 ID 2", t)
	CheckReq("REGISTER 2 10\n", "200 ID 3", t)
	CheckReq("REGISTER 2 15\n", "200 ID 4", t)

	CheckReq("THRESH_CRC_KLUDGE 0 1234 4455 0\n", "200 NOPAIR", t)
	CheckReq("THRESH_CRC_KLUDGE 0 1234 4455 0\n", "200 NOPAIR", t)
	CheckReq("THRESH_CRC_KLUDGE 0 1234 4455 0\n", "200 NOPAIR", t)
	CheckReq("THRESH_CRC_KLUDGE 1 4455 1234 0\n", "200 PAIR 0", t)
	CheckReq("THRESH_CRC_KLUDGE 0 1234 4455 0\n", "200 PAIR 1", t)

	CheckReq("LOCALIZE 0 1\n", "200 OK", t)

	// TODO: Test various error cases

	// TODO: How to handle this?
	CheckReq("THRESH_CRC_KLUDGE 2 1234 4455 0\n", "200 NOPAIR", t)
}

func TestCRCKludgeMiss(t *testing.T) {
	P := StartServerProcess()
	defer Stop(P)

	CheckReq("REGISTER 1 10\n", "200 ID 0", t)
	CheckReq("REGISTER 1 15\n", "200 ID 1", t)
	CheckReq("REGISTER 1 20\n", "200 ID 2", t)
	CheckReq("REGISTER 2 10\n", "200 ID 3", t)
	CheckReq("REGISTER 2 15\n", "200 ID 4", t)

	// Try to find a pair for 0 a few times...
	CheckReq("THRESH_CRC_KLUDGE 0 1234 4455 0\n", "200 NOPAIR", t)
	CheckReq("THRESH_CRC_KLUDGE 0 1234 4455 0\n", "200 NOPAIR", t)
	// Notice the 'LastTry' flag is set here:
	CheckReq("THRESH_CRC_KLUDGE 0 1234 4455 1\n", "200 NOPAIR", t)
	// Client then gives up, and never contacts us again.
	// Immediately afterwards, the other endpoint starts
	// asking for its endpoint.
	// If we do the match, one endpoint will try to use
	// the original socket and the other will use the local one.
	// Don't do this!
	CheckReq("THRESH_CRC_KLUDGE 1 4455 1234 0\n", "200 NOPAIR", t)
}

func TestFindPair(t *testing.T) {
	P := StartServerProcess()
	defer Stop(P)

	CheckReq("REGISTER 1 10\n", "200 ID 0", t)
	CheckReq("REGISTER 1 15\n", "200 ID 1", t)
	CheckReq("REGISTER 1 20\n", "200 ID 2", t)
	CheckReq("REGISTER 2 10\n", "200 ID 3", t)
	CheckReq("REGISTER 2 15\n", "200 ID 4", t)

	CheckReq("FIND_PAIR 0 192.168.0.2 80 192.168.0.3 30 1234 4455 1 0 0 0 0 0\n", "200 NOPAIR", t)
	CheckReq("FIND_PAIR 0 192.168.0.2 80 192.168.0.3 30 1234 4455 1 0 0 0 0 0\n", "200 NOPAIR", t)
	CheckReq("FIND_PAIR 1 192.168.0.3 30 192.168.0.2 80 4455 1234 0 0 0 0 0 0\n", "200 PAIR 0", t)
	CheckReq("FIND_PAIR 0 192.168.0.2 80 192.168.0.3 30 1234 4455 1 0 0 0 0 0\n", "200 PAIR 1", t)

	CheckReq("LOCALIZE 0 1\n", "200 OK", t)

	// TODO: How to handle this?
	// 0 and 1 are paired, 2 comes along trying to pair
	// using same information as 0 provided.
	CheckReq("FIND_PAIR 2 192.168.0.2 80 192.168.0.3 30 1234 4455 1 0 0 0 0 0\n", "200 NOPAIR", t)
}

func TestFindPairMiss(t *testing.T) {
	P := StartServerProcess()
	defer Stop(P)

	CheckReq("REGISTER 1 10\n", "200 ID 0", t)
	CheckReq("REGISTER 1 15\n", "200 ID 1", t)
	CheckReq("REGISTER 1 20\n", "200 ID 2", t)
	CheckReq("REGISTER 2 10\n", "200 ID 3", t)
	CheckReq("REGISTER 2 15\n", "200 ID 4", t)

	// Try to find a pair for 0 a few times...
	CheckReq("FIND_PAIR 0 192.168.0.2 80 192.168.0.3 30 1234 4455 1 0 0 0 0 0\n", "200 NOPAIR", t)
	CheckReq("FIND_PAIR 0 192.168.0.2 80 192.168.0.3 30 1234 4455 1 0 0 0 0 0\n", "200 NOPAIR", t)
	// Notice the 'LastTry' flag is set here:
	CheckReq("FIND_PAIR 0 192.168.0.2 80 192.168.0.3 30 1234 4455 1 1 0 0 0 0\n", "200 NOPAIR", t)
	// Client then gives up, and never contacts us again.
	// Immediately afterwards, the other endpoint starts
	// asking for its endpoint.
	// If we do the match, one endpoint will try to use
	// the original socket and the other will use the local one.
	// Don't do this!
	CheckReq("FIND_PAIR 1 192.168.0.3 30 192.168.0.2 80 4455 1234 0 0 0 0 0 0\n", "200 NOPAIR", t)
}
