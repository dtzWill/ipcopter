package main

import (
	"errors"
	"fmt"
	"os"
	"sync"
	"syscall"
)

type LocalizedEP struct {
	EP      *EndPoint
	LocalFD *os.File
}

type LocalInfo struct {
	A, B LocalizedEP
}

type EndPoint struct {
	PID        int
	FD         int
	Info       *LocalInfo
	KludgePair *EndPoint
}

type IPCContext struct {
	IDMap  map[EndPoint]int
	EPMap  map[int]*EndPoint
	Lock   sync.Mutex
	FreeID int
	// Used for EP sync kludge
	WaitingEP *EndPoint
}

func NewContext() *IPCContext {
	C := &IPCContext{}
	C.IDMap = make(map[EndPoint]int)
	C.EPMap = make(map[int]*EndPoint)
	return C
}

// EP's contain extra information
// that should be refactored out,
// but until then strip it when
// looking it up by-value in IDMap
func bareEP(EP *EndPoint) EndPoint {
	return EndPoint{EP.PID, EP.FD, nil, nil}
}

func (C *IPCContext) register(PID, FD int) (int, error) {
	C.Lock.Lock()
	defer C.Lock.Unlock()

	EP := EndPoint{PID, FD, nil, nil /* kludge pair */}
	if _, exist := C.IDMap[EP]; exist {
		return 0, errors.New("Duplicate registration!")
	}

	ID := C.FreeID
	// Simple allocation scheme.
	// TODO: Handle overflow and ID re-use
	C.FreeID++

	C.IDMap[EP] = ID
	C.EPMap[ID] = &EP

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

	EP, exist := C.EPMap[ID]
	if !exist {
		return errors.New(fmt.Sprintf("Invalid Endpoint ID '%d'", ID))
	}

	// Remove enties from map
	delete(C.IDMap, bareEP(EP))
	delete(C.EPMap, ID)

	// TODO: "Un-localize" endpoint?
	if EP.Info != nil {
		// TODO: Close as part of handing to endpoints?
		EP.Info.A.LocalFD.Close()
		EP.Info.B.LocalFD.Close()
	}

	return nil
}

func (C *IPCContext) removeall(PID int) int {
	C.Lock.Lock()
	defer C.Lock.Unlock()

	count := 0

	RemoveIDs := []int{}
	RemoveEPs := []*EndPoint{}

	for k, v := range C.EPMap {
		if v.PID == PID {
			RemoveIDs = append(RemoveIDs, k)
			RemoveEPs = append(RemoveEPs, v)
			count++
		}
	}

	for _, ID := range RemoveIDs {
		delete(C.EPMap, ID)
	}
	for _, EP := range RemoveEPs {
		if EP.Info != nil {
			// TODO: Close as part of handing to endpoints?
			EP.Info.A.LocalFD.Close()
			EP.Info.B.LocalFD.Close()
		}
		delete(C.IDMap, bareEP(EP))
	}

	return count
}

func (C *IPCContext) pairkludge(ID int) (int, error) {
	C.Lock.Lock()
	defer C.Lock.Unlock()

	EP, exist := C.EPMap[ID]
	if !exist {
		return ID, errors.New(fmt.Sprintf("Invalid Endpoint ID '%d'", ID))
	}

	// If already kludge-paired this, return its kludge-pal
	if EP.KludgePair != nil {
		return C.IDMap[bareEP(EP.KludgePair)], nil
	}

	// Otherwise, is there a pair candidate waiting?
	Waiting := C.WaitingEP
	if Waiting != nil && Waiting != EP {
		EP.KludgePair = Waiting
		Waiting.KludgePair = EP
		C.WaitingEP = nil
		return C.IDMap[bareEP(Waiting)], nil
	}

	// Nope, well track this in case someone
	// comes looking for this unpaired endpoint:

	C.WaitingEP = EP

	return ID, nil
}
