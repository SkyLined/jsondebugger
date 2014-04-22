#include "stdafx.h"
#include "cProcessInformation.h"
#include "cThreadContext.h"
#include "getAddressDescription.h"
#include "handleExceptionAccessViolation.h"
#include "JSONStringify.h"

BOOL handleExceptionAccessViolation(DEBUG_EVENT& oDebugEvent, cThreadContext& oThreadContext, cProcessInformation& oProcessInformation) {
  if (oDebugEvent.u.Exception.ExceptionRecord.ExceptionCode == EXCEPTION_ACCESS_VIOLATION) {
    PTSTR siChance = oDebugEvent.u.Exception.dwFirstChance ? _T("1") : _T("2");
    PTSTR sbContinuable = oDebugEvent.u.Exception.ExceptionRecord.ExceptionFlags == 0 ? _T("true") : _T("false");
    ULONG_PTR uAccessType = oDebugEvent.u.Exception.ExceptionRecord.ExceptionInformation[0];
    _tprintf(_T("{\"sEventName\": \"accessViolation\", \"iProcessId\": %u, \"iThreadId\": %u, \"sThreadISA\": \"%s\", \"iChance\": %s, \"bContinuable\": %s, \"iCodeAddress\": %Iu, \"iEventAddress\": %Iu, \"sType\": \"%s\"};\r\n"),
        oDebugEvent.dwProcessId, oDebugEvent.dwThreadId, oThreadContext.bThreadIs64Bit ? _T("x64") : _T("x86"),
        siChance, sbContinuable, oDebugEvent.u.Exception.ExceptionRecord.ExceptionAddress, oDebugEvent.u.Exception.ExceptionRecord.ExceptionInformation[1],
        uAccessType == 0 ? _T("read") : uAccessType == 1 ? _T("write") : _T("execute"));
    return TRUE;
  }
  return FALSE;
}