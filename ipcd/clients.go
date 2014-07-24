package main

// Listen for clients, interpret requests
// and execute specified command
// in our optimization context.

import (
	"bufio"
	"fmt"
	"log"
	"net"
	"os"
	"strconv"
	"strings"
)

const SOCKET_PATH = "/tmp/ipcd.sock"

type ClientReq struct {
	I int
}

const (
	REQ_ERR_UNRECOGNIZED_CMD  = iota
	REQ_ERR_INVALID_PARAMETER = iota
	REQ_ERR_INSUFFICIENT_ARGS = iota
	REQ_ERR_UNKNOWN           = iota
)

type ReqError struct {
	Type int
	Msg  string
}

func (R *ReqError) Error() string {
	return fmt.Sprintf("RequestError(Type=%d, Msg=%s)", R.Type, R.Msg)
}

func (R *ReqError) Response() string {
	ErrCode := 300 + R.Type
	return fmt.Sprintf("%d %s", ErrCode, R.Msg)
}

func listenForClients(C *IPCContext) {
	os.Remove(SOCKET_PATH)
	ln, err := net.Listen("unix", SOCKET_PATH)
	if err != nil {
		panic(err)
	}

	err = os.Chmod(SOCKET_PATH, os.ModePerm)
	if err != nil {
		panic(err)
	}

	for {
		conn, err := ln.Accept()
		if err != nil {
			log.Printf("Error in accept: %s\n", err.Error())
			continue
		}
		go handleConnection(C, conn)
	}
}

func handleConnection(Context *IPCContext, C net.Conn) {
	// TODO: Use something more structured like protobuf, etc
	b := bufio.NewReader(C)
	defer C.Close()

	for {
		line, err := b.ReadBytes('\n')
		if err != nil { // EOF, or worse
			break
		}
		lineString := strings.TrimSuffix(string(line), "\n")
		msg, reqerr := processRequestLine(Context, C, lineString)
		if reqerr != nil {
			resp := reqerr.Response()
			// fmt.Printf("<-- Success '%s'\n", resp)
			C.Write([]byte(resp + "\n"))
		} else {
			C.Write([]byte(fmt.Sprintf("200 %s\n", msg)))
		}
	}
}

func InsufficientArgsErr() *ReqError {
	return &ReqError{REQ_ERR_INSUFFICIENT_ARGS, "Insufficient arguments given"}
}
func InvalidParameterErr(msg string) *ReqError {
	return &ReqError{REQ_ERR_INVALID_PARAMETER, msg}
}
func UnknownErr(msg string) *ReqError {
	return &ReqError{REQ_ERR_UNKNOWN, msg}
}

func processRequestLine(Ctxt *IPCContext, C net.Conn, line string) (Resp string, RErr *ReqError) {
	Resp = "OK"
	spaceDelimTokens := strings.Split(line, " ")
	if len(spaceDelimTokens) < 2 {
		RErr = InsufficientArgsErr()
		return
	}
	command := spaceDelimTokens[0]
	// fmt.Printf("processRequest: '%s'\n", line)
	switch command {
	case "REGISTER":
		// REGISTER PID FD
		if len(spaceDelimTokens) < 3 {
			RErr = InsufficientArgsErr()
			return
		}

		PID, err := strconv.Atoi(spaceDelimTokens[1])
		if err != nil {
			RErr = InvalidParameterErr(err.Error())
			return
		}
		FD, err := strconv.Atoi(spaceDelimTokens[2])
		if err != nil {
			RErr = InvalidParameterErr(err.Error())
			return
		}

		ID, err := Ctxt.register(PID, FD)
		if err != nil {
			RErr = UnknownErr(err.Error())
			return
		}
		return fmt.Sprintf("ID %d", ID), nil
	case "LOCALIZE":
		// LOCALIZE LOCALID REMOTEID
		if len(spaceDelimTokens) < 3 {
			RErr = InsufficientArgsErr()
			return
		}

		LID, err := strconv.Atoi(spaceDelimTokens[1])
		if err != nil {
			RErr = InvalidParameterErr(err.Error())
			return
		}
		RID, err := strconv.Atoi(spaceDelimTokens[2])
		if err != nil {
			RErr = InvalidParameterErr(err.Error())
			return
		}

		err = Ctxt.localize(LID, RID)
		if err != nil {
			RErr = UnknownErr(err.Error())
			return
		}
	case "GETLOCALFD":
		LID, err := strconv.Atoi(spaceDelimTokens[1])
		if err != nil {
			RErr = InvalidParameterErr(err.Error())
			return
		}

		FD, err := Ctxt.getLocalFD(LID)
		if err != nil {
			RErr = UnknownErr(err.Error())
			return
		}

		U, err := NewFromConn(C)
		if err != nil {
			RErr = UnknownErr(err.Error())
			return
		}
		err = U.WriteFD(int(FD.Fd()))
		if err != nil {
			RErr = UnknownErr(err.Error())
			return
		}
	case "UNREGISTER":
		// UNREGISTER <endpoint>
		EP, err := strconv.Atoi(spaceDelimTokens[1])
		if err != nil {
			RErr = InvalidParameterErr(err.Error())
			return
		}

		err = Ctxt.unregister(EP)
		if err != nil {
			RErr = UnknownErr(err.Error())
			return
		}
	case "REMOVEALL":
		// REMOVEALL <pid>
		PID, err := strconv.Atoi(spaceDelimTokens[1])
		if err != nil {
			RErr = InvalidParameterErr(err.Error())
			return
		}

		removed := Ctxt.removeall(PID)
		return fmt.Sprintf("REMOVED %d", removed), nil
	case "ENDPOINT_KLUDGE":
		// ENDPOINT_KLUDGE <endpoint id>
		EP, err := strconv.Atoi(spaceDelimTokens[1])
		if err != nil {
			RErr = InvalidParameterErr(err.Error())
			return
		}

		Pair, err := Ctxt.pairkludge(EP)
		if err != nil {
			RErr = UnknownErr(err.Error())
			return
		}
		if Pair == EP {
			return "NOPAIR", nil
		}
		return fmt.Sprintf("PAIR %d", Pair), nil
	case "REREGISTER":
		// REREGISTER EP PID FD
		// TODO: Actually do something with PID/FD.
		// (Esp useful when start checking caller's creds!)
		// TODO: Consider requiring sender specifies the pid/fd of the original for verification.
		if len(spaceDelimTokens) < 4 {
			RErr = InsufficientArgsErr()
			return
		}
		EP, err := strconv.Atoi(spaceDelimTokens[1])
		if err != nil {
			RErr = InvalidParameterErr(err.Error())
			return
		}
		PID, err := strconv.Atoi(spaceDelimTokens[1])
		if err != nil {
			RErr = InvalidParameterErr(err.Error())
			return
		}
		FD, err := strconv.Atoi(spaceDelimTokens[1])
		if err != nil {
			RErr = InvalidParameterErr(err.Error())
			return
		}

		err = Ctxt.reregister(EP, PID, FD)
		if err != nil {
			RErr = UnknownErr(err.Error())
			return
		}
	default:
		RErr = &ReqError{REQ_ERR_UNRECOGNIZED_CMD, "Unrecognized command"}
		return
	}

	RErr = nil
	return
}
