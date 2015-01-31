package main

import (
	"errors"
	"fmt"
	"os"
	"sync"
	"syscall"
	"time"
)

type LocalizedEP struct {
	EP      *EndPointInfo
	LocalFD *os.File
}

type LocalInfo struct {
	A, B LocalizedEP
}

type EndPoint struct {
	PID int
	FD  int
}

type NetAddr struct {
	IP   string
	Port int
}

type EndPointInfo struct {
	EP         EndPoint
	Info       *LocalInfo
	KludgePair *EndPointInfo
	S_CRC      int
	R_CRC      int
	Src        NetAddr
	Dst        NetAddr
	IsAccept   bool
	Start      time.Time
	End        time.Time
	RefCount   int
	ID         int
}

type IPCContext struct {
	EPMap  map[int]*EndPointInfo
	Lock   sync.Mutex
	FreeID int
	// Used for Endpoint sync kludge
	WaitingEPI  *EndPointInfo
	WaitingTime time.Time
}

func InvalidAddr() NetAddr {
	return NetAddr{"", -1}
}

func (N *NetAddr) isValid() bool {
	return N.Port != -1
}

func CheckTimeDelta(D time.Duration) bool {
	Epsilon := time.Duration(200 * time.Microsecond)

	return D <= Epsilon && D >= -Epsilon
}

func NewContext() *IPCContext {
	C := &IPCContext{}
	C.EPMap = make(map[int]*EndPointInfo)
	return C
}

func (C *IPCContext) register(PID, FD int) (int, error) {
	C.Lock.Lock()
	defer C.Lock.Unlock()

	ID := C.FreeID

	EPI := EndPointInfo{EndPoint{PID, FD}, nil,
		nil,           /* kludge pair */
		0,             /* S_CRC */
		0,             /* R_CRC */
		InvalidAddr(), /* Src */
		InvalidAddr(), /* Dst*/
		false,         /* IsAccept */
		time.Time{},   /* Start */
		time.Time{},   /* End */
		1,             /* refcnt */
		ID}

	C.EPMap[ID] = &EPI

	// Find next free ID
	used := true
	for used {
		C.FreeID++
		_, used = C.EPMap[C.FreeID]
	}

	return ID, nil
}

func (C *IPCContext) localize(LID, RID int) error {
	C.Lock.Lock()
	defer C.Lock.Unlock()

	LEP, exist := C.EPMap[LID]
	if !exist {
		return errors.New("Invalid Local ID")
	}
	REP, exist := C.EPMap[RID]
	if !exist {
		return errors.New("Invalid Remote ID")
	}

	if LEP.Info != REP.Info {
		return errors.New("Attempt to localize already localized FD?")
	}
	if LEP.Info != nil {
		// These have already been localized, with same endpoints.  All is well.
		return nil
	}

	// Okay, spawn connected pair of sockets for these
	LFD, RFD, err := Socketpair(syscall.SOCK_STREAM)
	if err != nil {
		return err
	}

	LEP_A, LEP_B := LocalizedEP{LEP, LFD}, LocalizedEP{REP, RFD}
	if RID < LID {
		LEP_B, LEP_A = LEP_A, LEP_B
	}

	LI := &LocalInfo{LEP_A, LEP_B}
	LEP.Info = LI
	REP.Info = LI

	return nil
}

func (C *IPCContext) getLocalFD(ID int) (*os.File, error) {
	C.Lock.Lock()
	defer C.Lock.Unlock()

	EP, exist := C.EPMap[ID]
	if !exist {
		return nil, errors.New("Invalid ID")
	}

	if EP.Info == nil {
		return nil, errors.New("Requested local FD for non-localized Endpoint")
	}

	if EP.Info.A.EP == EP {
		return EP.Info.A.LocalFD, nil
	}
	if EP.Info.B.EP == EP {
		return EP.Info.B.LocalFD, nil
	}

	return nil, errors.New("LocalInfo mismatch: Endpoint not found??")
}

func (C *IPCContext) unregister(ID int) error {
	C.Lock.Lock()
	defer C.Lock.Unlock()

	EPI, exist := C.EPMap[ID]
	if !exist {
		return errors.New(fmt.Sprintf("Invalid Endpoint ID '%d'", ID))
	}

	EPI.RefCount--
	if EPI.RefCount > 0 {
		// More references, leave it registered
		return nil
	}

	// Remove enties from map
	delete(C.EPMap, ID)

	if ID < C.FreeID {
		C.FreeID = ID
	}

	// TODO: "Un-localize" endpoint?
	if EPI.Info != nil {
		// TODO: Close as part of handing to endpoints?
		EPI.Info.A.LocalFD.Close()
		EPI.Info.B.LocalFD.Close()
	}

	if C.WaitingEPI == EPI {
		C.WaitingEPI = nil
	}

	return nil
}

func (C *IPCContext) removeall(PID int) int {
	C.Lock.Lock()
	defer C.Lock.Unlock()

	count := 0

	RemoveIDs := []int{}
	RemoveEPIs := []*EndPointInfo{}

	// TODO: reregistration means an endpoint could
	// have multiple owning processes, handle this!
	// This all really needs a do-over! O:)
	for k, v := range C.EPMap {
		if v.EP.PID == PID {
			RemoveIDs = append(RemoveIDs, k)
			RemoveEPIs = append(RemoveEPIs, v)
			count++
		}
	}

	for _, ID := range RemoveIDs {
		delete(C.EPMap, ID)
	}
	for _, EPI := range RemoveEPIs {
		if EPI.Info != nil {
			// TODO: Close as part of handing to endpoints?
			EPI.Info.A.LocalFD.Close()
			EPI.Info.B.LocalFD.Close()
		}

		if C.WaitingEPI == EPI {
			C.WaitingEPI = nil
		}
	}

	return count
}

func (C *IPCContext) pairkludge(ID int) (int, error) {
	C.Lock.Lock()
	defer C.Lock.Unlock()

	EPI, exist := C.EPMap[ID]
	if !exist {
		return ID, errors.New(fmt.Sprintf("Invalid Endpoint ID '%d'", ID))
	}

	// If already kludge-paired this, return its kludge-pal
	if EPI.KludgePair != nil {
		return EPI.KludgePair.ID, nil
	}

	// Otherwise, is there a pair candidate waiting?
	Waiting := C.WaitingEPI

	if Waiting != nil {
		if time.Since(C.WaitingTime) >= 100*time.Millisecond {
			C.WaitingEPI = nil
			Waiting = nil
		}
	}

	if Waiting != nil && Waiting != EPI {
		EPI.KludgePair = Waiting
		Waiting.KludgePair = EPI
		C.WaitingEPI = nil
		return Waiting.ID, nil
	}

	// Nope, well track this in case someone
	// comes looking for this unpaired endpoint:

	C.WaitingEPI = EPI
	C.WaitingTime = time.Now()

	return ID, nil
}

func (C *IPCContext) reregister(ID, PID, FD int) error {
	C.Lock.Lock()
	defer C.Lock.Unlock()

	EPI, exist := C.EPMap[ID]
	if !exist {
		return errors.New(fmt.Sprintf("Invalid Endpoint ID '%d'", ID))
	}

	EPI.RefCount++

	return nil
}

func (C *IPCContext) crc_match(ID, S_CRC, R_CRC int, LastTry bool) (int, error) {
	C.Lock.Lock()
	defer C.Lock.Unlock()

	EPI, exist := C.EPMap[ID]
	if !exist {
		return ID, errors.New(fmt.Sprintf("Invalid Endpoint ID '%d'", ID))
	}

	// If already kludge-paired this, return its kludge-pal
	if EPI.KludgePair != nil {
		return EPI.KludgePair.ID, nil
	}

	// TODO: Zero is a valid CRC value!
	if EPI.S_CRC != 0 || EPI.R_CRC != 0 {
		if EPI.S_CRC != S_CRC && EPI.R_CRC != R_CRC {
			return ID, errors.New("CRC match attempted with changed values")
		}
	}

	EPI.S_CRC = S_CRC
	EPI.R_CRC = R_CRC

	MatchID := -1
	for k, v := range C.EPMap {
		if k == ID {
			continue
		}
		if v.KludgePair != nil {
			continue
		}
		if v.S_CRC == R_CRC && v.R_CRC == S_CRC {
			MatchID = k
			break
		}
	}
	// NOPAIR
	if MatchID == -1 {
		// If this is the last time the program
		// will attempt to find its communication pair,
		// remove the CRC information to prevent pairing.
		if LastTry {
			EPI.S_CRC = 0
			EPI.R_CRC = 0
		}
		return ID, nil
	}

	Match := C.EPMap[MatchID]
	EPI.KludgePair = Match
	Match.KludgePair = EPI

	return MatchID, nil
}

func (C *IPCContext) find_pair(ID int, Src, Dst NetAddr, S_CRC, R_CRC int, IsAccept, LastTry bool, Start, End time.Time) (int, error) {
	C.Lock.Lock()
	defer C.Lock.Unlock()

	EPI, exist := C.EPMap[ID]
	if !exist {
		return ID, errors.New(fmt.Sprintf("Invalid Endpoint ID '%d'", ID))
	}

	// If already kludge-paired this, return its kludge-pal
	if EPI.KludgePair != nil {
		return EPI.KludgePair.ID, nil
	}

	if EPI.Src.isValid() || EPI.Dst.isValid() {
		if EPI.S_CRC != S_CRC && EPI.R_CRC != R_CRC {
			return ID, errors.New("pairing attempted with changed CRC values")
		}
		if EPI.Src != Src || EPI.Dst != Dst {
			return ID, errors.New("pairing attempted with changed address")
		}
		if EPI.IsAccept != IsAccept {
			return ID, errors.New("pairing attempted with changed is_accept")
		}
	}

	EPI.S_CRC = S_CRC
	EPI.R_CRC = R_CRC
	EPI.Src = Src
	EPI.Dst = Dst
	EPI.IsAccept = IsAccept
	EPI.Start = Start
	EPI.End = End

	MatchID := -1
	for k, v := range C.EPMap {
		if k == ID {
			continue
		}
		if v.KludgePair != nil {
			continue
		}
		if v.S_CRC == R_CRC && v.R_CRC == S_CRC &&
			v.Src == Dst && v.Dst == Src {
			// One side should have accepted, other shouldn't.
			if v.IsAccept != !IsAccept {
				return ID, errors.New("match found but is_accept mismatch??")
			}

			Client, Server := v, EPI
			if !IsAccept {
				Client, Server = EPI, v
			}
			if Client.Start.After(Server.End) {
				// Accept returned before connect() finished, definitely not valid
				fmt.Println("Client connected after server accepted?!")
				return ID, nil
			}
			ConnectToAcceptReturn := Server.End.Sub(Client.Start)
			AcceptReturnToConnectReturn := Client.End.Sub(Server.End)
			if false {
				fmt.Printf("Connect-Start To Accept Return: %s\n", ConnectToAcceptReturn)
				fmt.Printf("Accept Return to Connect Return: %s\n", AcceptReturnToConnectReturn)
				fmt.Printf("Connect Duration: %s\n", Client.End.Sub(Client.Start))
				fmt.Printf("Accept Duration: %s\n", Server.End.Sub(Server.Start))
			}
			if CheckTimeDelta(AcceptReturnToConnectReturn) {
				MatchID = k
			} else {
				// times weren't close enough, bail
				fmt.Println("Times not close enough!!")
				return ID, nil
			}
			break
		}
	}
	// NOPAIR
	if MatchID == -1 {
		// If this is the last time the program
		// will attempt to find its communication pair,
		// remove the CRC information to prevent pairing.
		if LastTry {
			EPI.S_CRC = 0
			EPI.R_CRC = 0
			EPI.Src = InvalidAddr()
			EPI.Dst = InvalidAddr()
		}
		return ID, nil
	}

	Match := C.EPMap[MatchID]
	EPI.KludgePair = Match
	Match.KludgePair = EPI

	return MatchID, nil
}
