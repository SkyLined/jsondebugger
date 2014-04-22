#include "stdafx.h"
#include "breakpoints.h"
#include "cProcessInformation.h"
#include "cThreadContext.h"
#include <list>
#include <map>

//#define LOG(x) x

#ifndef LOG
#define LOG(x)
#endif

std::map<DWORD, std::map<PBYTE, std::list<BREAKPOINT_CALLBACK>>> gdBreakpoint_afCallbacks_by_pAddress_by_dwProcessId;
std::map<DWORD, std::map<PBYTE, BYTE>> gdBreakpoint_bReplacedByte_by_pAddress_by_dwProcessId;
std::map<PBYTE, BYTE> gdNoBreakpoints;
std::map<DWORD, std::map<DWORD, PBYTE>> gdActiveBreakpoint_pAddress_by_dwThreadId_by_dwProcessId;

BYTE gbInt3 = 0xCC;

const std::map<PBYTE, BYTE> getBreakpointsForProcess(DWORD dwProcessId, BOOL bAdd = FALSE) {
  if (gdBreakpoint_bReplacedByte_by_pAddress_by_dwProcessId.count(dwProcessId) == 0 && !bAdd) {
    return gdNoBreakpoints;
  }
  return gdBreakpoint_bReplacedByte_by_pAddress_by_dwProcessId[dwProcessId];
}

BOOL hasInt3(DWORD dwProcessId, PBYTE pAddress) {
  return (
    gdBreakpoint_bReplacedByte_by_pAddress_by_dwProcessId.count(dwProcessId) == 1
    && gdBreakpoint_bReplacedByte_by_pAddress_by_dwProcessId[dwProcessId].count(pAddress) == 1
  );
}

BOOL setBreakpoint(DWORD dwProcessId, PBYTE pAddress, BREAKPOINT_CALLBACK fCallback) {
  LOG(_tprintf(_T("breakpoints: Setting BREAKPOINT in process %u at 0x%IX.\r\n"), dwProcessId, pAddress));
  if (!hasInt3(dwProcessId, pAddress)) {
    cProcessInformation* poProcessInformation = getProcessInformation(dwProcessId);
    BYTE bOriginalByte;
    if (!poProcessInformation->readByte(pAddress, bOriginalByte, FALSE)) {
      _tprintf(_T("{\"sEventName\": \"internalError\", \"sErrorMessage\": \"Cannot read code at 0x%IX in process %u: 0x%X\"};\r\n"), pAddress, dwProcessId, GetLastError());
      return FALSE;
    }
    LOG(_tprintf(_T("breakpoints:   + Original byte: 0x%02X.\r\n"), bOriginalByte));
    if (!poProcessInformation->writeByte(pAddress, gbInt3, FALSE)) {
      _tprintf(_T("{\"sEventName\": \"internalError\", \"sErrorMessage\": \"Cannot write int3 at 0x%IX in process %u: 0x%X\"};\r\n"), pAddress, dwProcessId, GetLastError());
      return FALSE;
    }
    gdBreakpoint_bReplacedByte_by_pAddress_by_dwProcessId[dwProcessId][pAddress] = bOriginalByte;
  }
  gdBreakpoint_afCallbacks_by_pAddress_by_dwProcessId[dwProcessId][pAddress].push_back(fCallback);
  LOG(_tprintf(_T("breakpoints:   + OK.\r\n")));
  return TRUE;
}

BOOL removeBreakpoint(DWORD dwProcessId, PBYTE pAddress, BREAKPOINT_CALLBACK fCallback) {
  LOG(_tprintf(_T("breakpoints: * Removing BREAKPOINT in process %u at 0x%IX.\r\n"), dwProcessId, pAddress));
  if (!hasInt3(dwProcessId, pAddress)) {
    _tprintf(_T("{\"sEventName\": \"internalError\", \"sErrorMessage\": \"Cannot remove non-existing breakpoint at 0x%IX in process %u.\"};\r\n"), pAddress, dwProcessId);
    return FALSE;
  }
  std::list<BREAKPOINT_CALLBACK> afCallbacks = gdBreakpoint_afCallbacks_by_pAddress_by_dwProcessId[dwProcessId][pAddress];
  afCallbacks.remove(fCallback);
  if (afCallbacks.empty()) {
    // Remove int3
    cProcessInformation* poProcessInformation = getProcessInformation(dwProcessId);
    BYTE bOriginalByte = gdBreakpoint_bReplacedByte_by_pAddress_by_dwProcessId[dwProcessId][pAddress];
    if (!poProcessInformation->writeByte(pAddress, bOriginalByte, FALSE)) {
      _tprintf(_T("{\"sEventName\": \"internalError\", \"sErrorMessage\": \"Cannot remove int3 at 0x%IX in process %u: 0x%X\"};\r\n"), pAddress, dwProcessId, GetLastError());
      return FALSE;
    }
    gdBreakpoint_afCallbacks_by_pAddress_by_dwProcessId[dwProcessId].erase(pAddress);
    gdBreakpoint_bReplacedByte_by_pAddress_by_dwProcessId[dwProcessId].erase(pAddress);
    if (gdBreakpoint_afCallbacks_by_pAddress_by_dwProcessId[dwProcessId].empty()) {
      LOG(_tprintf(_T("breakpoints:     (Removed last breakpoint from process %u).\r\n"), dwProcessId));
      gdBreakpoint_afCallbacks_by_pAddress_by_dwProcessId.erase(dwProcessId);
      gdBreakpoint_bReplacedByte_by_pAddress_by_dwProcessId.erase(dwProcessId);
    }
  }
  LOG(_tprintf(_T("breakpoints:   + OK.\r\n")));
  return TRUE;
}

BOOL breakpointCausedException(DEBUG_EVENT& oDebugEvent, cThreadContext& oThreadContext, cProcessInformation& oProcessInformation) {
  if (oDebugEvent.u.Exception.ExceptionRecord.ExceptionCode == EXCEPTION_BREAKPOINT) {
    PBYTE pBreakpointAddress = (PBYTE)oDebugEvent.u.Exception.ExceptionRecord.ExceptionAddress;
    if (hasInt3(oDebugEvent.dwProcessId, pBreakpointAddress)) {
      return TRUE;
    }
  } else if (oDebugEvent.u.Exception.ExceptionRecord.ExceptionCode == EXCEPTION_SINGLE_STEP) {
    if (gdActiveBreakpoint_pAddress_by_dwThreadId_by_dwProcessId.count(oDebugEvent.dwProcessId) > 0) {
      if (gdActiveBreakpoint_pAddress_by_dwThreadId_by_dwProcessId[oDebugEvent.dwProcessId].count(oDebugEvent.dwThreadId) > 0) {
        return TRUE;
      }
    }
  }
  return FALSE;
}

DWORD handleExceptionBreakpoint(DEBUG_EVENT& oDebugEvent, cThreadContext& oThreadContext, cProcessInformation& oProcessInformation) {
  PBYTE pBreakpointAddress = (PBYTE)oDebugEvent.u.Exception.ExceptionRecord.ExceptionAddress;
  if (!hasInt3(oDebugEvent.dwProcessId, pBreakpointAddress)) {
    _tprintf(_T("{\"sEventName\": \"internalError\", \"sErrorMessage\": \"Attempt to have a breakpoint exception at 0x%IX handled as a breakpoint, ")
      _T("but no int3 should be there.\"};\r\n"), pBreakpointAddress);
    return NULL;
  }
  LOG(_tprintf(_T("breakpoints: * Handling BREAKPOINT in process %u, thread %u at 0x%IX.\r\n"), oDebugEvent.dwProcessId, oDebugEvent.dwThreadId, pBreakpointAddress));
  // Open the thread and modify its context to move the instruction pointer back to the start of the int3 and set single step flag.
  PBYTE pInstructionPointer;
#ifdef _WIN64
  if (oThreadContext.bThreadIs64Bit) {
    oThreadContext.x64.Rip--;
    oThreadContext.x64.EFlags |= 0x100;
    pInstructionPointer = (PBYTE)oThreadContext.x64.Rip;
  } else
#endif
  {
    oThreadContext.x86.Eip--;
    oThreadContext.x86.EFlags |= 0x100;
    pInstructionPointer = (PBYTE)oThreadContext.x86.Eip;
  }
  if (pInstructionPointer != pBreakpointAddress) {
    _tprintf(_T("{\"sEventName\": \"internalError\", \"sErrorMessage\": \"Instruction pointer for thread %u is 0x%IX rather than 0x%IX.\"};\r\n"),
        oDebugEvent.dwThreadId, pInstructionPointer, pBreakpointAddress);
    return NULL;
  }
  if (!oThreadContext.set()) {
    _tprintf(_T("{\"sEventName\": \"internalError\", \"sErrorMessage\": \"Cannot set context of thread %u: 0x%X\"};\r\n"), oDebugEvent.dwThreadId, GetLastError());
    return NULL;
  }
  LOG(_tprintf(_T("breakpoints:   + Instruction pointer reset to 0x%IX and single-stepping enabled in thread %u.\r\n"), pInstructionPointer, oDebugEvent.dwThreadId));
  BYTE bOriginalByte = gdBreakpoint_bReplacedByte_by_pAddress_by_dwProcessId[oDebugEvent.dwProcessId][pBreakpointAddress];
  if (!oProcessInformation.writeByte(pBreakpointAddress, bOriginalByte, FALSE)) {
    _tprintf(_T("{\"sEventName\": \"internalError\", \"sErrorMessage\": \"Cannot write memory at 0x%IX in process %u: 0x%X\"};\r\n"), 
        pBreakpointAddress, oDebugEvent.dwProcessId, GetLastError());
    return NULL;
  }
  gdActiveBreakpoint_pAddress_by_dwThreadId_by_dwProcessId[oDebugEvent.dwProcessId][oDebugEvent.dwThreadId] = pBreakpointAddress;

  std::list<BREAKPOINT_CALLBACK> afCallbacks = gdBreakpoint_afCallbacks_by_pAddress_by_dwProcessId[oDebugEvent.dwProcessId][pBreakpointAddress];
  if (afCallbacks.size() > 0) {
    LOG(_tprintf(_T("breakpoints:   + Executing callbacks...\r\n")));
    for (std::list<BREAKPOINT_CALLBACK>::iterator itfCallback = afCallbacks.begin(); itfCallback != afCallbacks.end(); itfCallback++) {
      BREAKPOINT_CALLBACK fCallback = *itfCallback;
      if (fCallback && !fCallback(oDebugEvent, oThreadContext, oProcessInformation)) {
        return NULL;
      }
    }
  }
  
  LOG(_tprintf(_T("breakpoints:   + OK.\r\n")));
  return DBG_EXCEPTION_HANDLED;
}

DWORD handleExceptionSingleStep(DEBUG_EVENT& oDebugEvent, cThreadContext& oThreadContext, cProcessInformation& oProcessInformation) {
  if (
    gdActiveBreakpoint_pAddress_by_dwThreadId_by_dwProcessId.count(oDebugEvent.dwProcessId) == 0
    || gdActiveBreakpoint_pAddress_by_dwThreadId_by_dwProcessId[oDebugEvent.dwProcessId].count(oDebugEvent.dwThreadId) == 0
  ) {
    _tprintf(_T("{\"sEventName\": \"internalError\", \"sErrorMessage\": \"Attempt to have a single-step exception at 0x%IX in process %u, thread %u handled by the ")
        _T("breakpoint exception handler, but there is no active breakpoint in that thread.\"};\r\n"), oDebugEvent.u.Exception.ExceptionRecord.ExceptionAddress,
        oDebugEvent.dwProcessId, oDebugEvent.dwThreadId);
    return NULL;
  }
  PBYTE pBreakpointAddress = gdActiveBreakpoint_pAddress_by_dwThreadId_by_dwProcessId[oDebugEvent.dwProcessId][oDebugEvent.dwThreadId];
  LOG(_tprintf(_T("breakpoints: * Handling SINGLE STEP in process %u, thread %u at 0x%IX.\r\n"), 
      oDebugEvent.dwProcessId, oDebugEvent.dwThreadId, oDebugEvent.u.Exception.ExceptionRecord.ExceptionAddress));
  // This single step event was triggered after a breakpoint, in case it needs to be put back. Remove it from out map.
  gdActiveBreakpoint_pAddress_by_dwThreadId_by_dwProcessId[oDebugEvent.dwProcessId].erase(oDebugEvent.dwThreadId);
  if (gdActiveBreakpoint_pAddress_by_dwThreadId_by_dwProcessId[oDebugEvent.dwProcessId].empty()) {
    LOG(_tprintf(_T("breakpoints:     (No more active breakpoints in process %u).\r\n"), oDebugEvent.dwProcessId));
    gdActiveBreakpoint_pAddress_by_dwThreadId_by_dwProcessId.erase(oDebugEvent.dwProcessId);
  }
  // See if the breakpoint needs to be put back
  if (
    gdBreakpoint_afCallbacks_by_pAddress_by_dwProcessId.count(oDebugEvent.dwProcessId) == 0
    || gdBreakpoint_afCallbacks_by_pAddress_by_dwProcessId[oDebugEvent.dwProcessId].count(pBreakpointAddress) == 0
  ) {
    LOG(_tprintf(_T("breakpoints:   - No new breakpoint is needed in process %u at 0x%IX.\r\n"), oDebugEvent.dwProcessId, pBreakpointAddress));
  } else {
    // This breakpoint is still active - put it back
    if (!oProcessInformation.writeByte(pBreakpointAddress, gbInt3, FALSE)) {
      _tprintf(_T("{\"sEventName\": \"internalError\", \"sErrorMessage\": \"Cannot write memory at 0x%IX in process %u: 0x%X\"};\r\n"), 
          pBreakpointAddress, oDebugEvent.dwProcessId, GetLastError());
      return NULL;
    }
    LOG(_tprintf(_T("breakpoints:   + Breakpoint reset in process %u at 0x%IX.\r\n"), oDebugEvent.dwProcessId, pBreakpointAddress));
  }
  LOG(_tprintf(_T("breakpoints:   + OK.\r\n")));
  return DBG_EXCEPTION_HANDLED;
}

DWORD breakpointHandleException(DEBUG_EVENT& oDebugEvent, cThreadContext& oThreadContext, cProcessInformation& oProcessInformation) {
  if (oDebugEvent.u.Exception.ExceptionRecord.ExceptionCode == EXCEPTION_BREAKPOINT) {
    return handleExceptionBreakpoint(oDebugEvent, oThreadContext, oProcessInformation);
  }
  if (oDebugEvent.u.Exception.ExceptionRecord.ExceptionCode == EXCEPTION_SINGLE_STEP) {
    return handleExceptionSingleStep(oDebugEvent, oThreadContext, oProcessInformation);
  }
  _tprintf(_T("{\"sEventName\": \"internalError\", \"sErrorMessage\": \"Attempt to have an exception with code 0x%X handled by breakpoint exception handler.\"};\r\n"), 
      oDebugEvent.u.Exception.ExceptionRecord.ExceptionCode);
  return NULL;
}
