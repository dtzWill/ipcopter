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

func (E *EndPointInfo) matchesWithoutCRC(R *EndPointInfo) bool {
	// Not enough information to say.
	if !E.Src.isValid() || !R.Src.isValid() {
		return false
	}
	// If already has a pair, also nothing to be done here
	if E.KludgePair != nil || R.KludgePair != nil {
		return false
	}

	if E.IsAccept == R.IsAccept {
		return false
	}
	if E.Src != R.Dst || E.Dst != R.Src {
		return false
	}

	// Use "IsAccept" to designate client/server
	Client, Server := E, R
	if !E.IsAccept {
		Client, Server = R, E
	}

	// Hmm, might not actually matter which is which
	// looking at the code below.  Oh well O:)

	if Client.Start.After(Server.End) {
		// Accept returned before connect() finished, definitely not valid
		return false
	}
	if Server.Start.After(Client.End) {
		// connect() finished before accept was called, also not valid.
		return false
	}

	// TODO: Check time delta? Or is this enough?

	return true
}
func (E *EndPointInfo) matches(R *EndPointInfo) bool {
	return E.matchesWithoutCRC(R) &&
		E.S_CRC == R.R_CRC && E.R_CRC == R.S_CRC
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

func (C *IPCContext) endpoint_info(ID int, Src, Dst NetAddr, Start, End time.Time, IsAccept bool) error {
	C.Lock.Lock()
	defer C.Lock.Unlock()

	// TODO: Access same clock ourselves to verify updates?

	EPI, exist := C.EPMap[ID]
	if !exist {
		return errors.New(fmt.Sprintf("Invalid Endpoint ID '%d'", ID))
	}

	if EPI.KludgePair != nil {
		return errors.New("Cannot update info for paired endpoint")
	}

	if EPI.Src.isValid() || EPI.Dst.isValid() {
		if EPI.Src != Src || EPI.Dst != Dst {
			return errors.New("cannot change address")
		}
		if EPI.IsAccept != IsAccept {
			return errors.New("cannot change is_accept")
		}
		if EPI.Start != Start || EPI.End != EPI.End {
			return errors.New("cannot change timings")
		}
	}

	EPI.Src = Src
	EPI.Dst = Dst
	EPI.Start = Start
	EPI.End = End
	EPI.IsAccept = IsAccept

	return nil
}

func (C *IPCContext) find_pair(ID, S_CRC, R_CRC int, LastTry bool) (int, error) {
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

	// XXX: Zero is a valid CRC value!!
	// If we already received CRC information, ensure it's the same
	if EPI.S_CRC != 0 || EPI.R_CRC != 0 {
		if EPI.S_CRC != S_CRC && EPI.R_CRC != R_CRC {
			return ID, errors.New("pairing attempted with changed CRC values")
		}
	}

	if !EPI.Src.isValid() || !EPI.Dst.isValid() {
		return ID, errors.New("pairing without endpoint information")
	}

	EPI.S_CRC = S_CRC
	EPI.R_CRC = R_CRC

	// Find matches ignoring timing information and crc
	Matches := []int{}

	for k, v := range C.EPMap {
		if k == ID {
			continue
		}
		if EPI.matchesWithoutCRC(v) {
			Matches = append(Matches, k)
		}
	}

	if len(Matches) > 1 {
		return ID, errors.New("too many potential matches")
	}
	if len(Matches) > 0 {
		MatchID := Matches[0]
		Match := C.EPMap[MatchID]
		// Try to find endpoints that could
		// be matched with our potential match.
		Dups := []int{}
		for k, v := range C.EPMap {
			if k == ID {
				continue
			}
			if Match.matchesWithoutCRC(v) {
				Dups = append(Dups, k)
			}
		}

		if len(Dups) > 0 {
			return ID, errors.New("potential dup detected")
		}

		// No dups! Let's check CRC:
		if EPI.matches(Match) {
			EPI.KludgePair = Match
			Match.KludgePair = EPI
			return MatchID, nil
		}
	}
	// NOPAIR
	// If this is the last time the program
	// will attempt to find its communication pair,
	// remove the CRC information to prevent pairing.
	if LastTry {
		EPI.S_CRC = 0
		EPI.R_CRC = 0
	}
	return ID, nil
}
