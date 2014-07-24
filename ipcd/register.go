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

type EndPointInfo struct {
	EP         EndPoint
	Info       *LocalInfo
	KludgePair *EndPointInfo
	RefCount   int
}

type IPCContext struct {
	IDMap  map[EndPoint]int
	EPMap  map[int]*EndPointInfo
	Lock   sync.Mutex
	FreeID int
	// Used for Endpoint sync kludge
	WaitingEPI  *EndPointInfo
	WaitingTime time.Time
}

func NewContext() *IPCContext {
	C := &IPCContext{}
	C.IDMap = make(map[EndPoint]int)
	C.EPMap = make(map[int]*EndPointInfo)
	return C
}

func (C *IPCContext) register(PID, FD int) (int, error) {
	C.Lock.Lock()
	defer C.Lock.Unlock()

	EPI := EndPointInfo{EndPoint{PID, FD}, nil, nil /* kludge pair */, 1 /* refcnt */}
	if _, exist := C.IDMap[EPI.EP]; exist {
		return 0, errors.New("Duplicate registration!")
	}

	ID := C.FreeID
	// Simple allocation scheme.
	// TODO: Handle overflow and ID re-use
	C.FreeID++

	C.IDMap[EPI.EP] = ID
	C.EPMap[ID] = &EPI

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
	delete(C.IDMap, EPI.EP)
	delete(C.EPMap, ID)

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
		delete(C.IDMap, EPI.EP)

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
		return C.IDMap[EPI.KludgePair.EP], nil
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
		return C.IDMap[Waiting.EP], nil
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
