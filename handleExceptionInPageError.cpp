#include "stdafx.h"
#include "cProcessInformation.h"
#include "cThreadContext.h"
#include "getAddressDescription.h"
#include "handleExceptionInPageError.h"
#include "JSONStringify.h"

BOOL handleExceptionInPageError(DEBUG_EVENT& oDebugEvent, cThreadContext& oThreadContext, cProcessInformation& oProcessInformation) {
  if (oDebugEvent.u.Exception.ExceptionRecord.ExceptionCode == EXCEPTION_IN_PAGE_ERROR) {
    PTSTR siChance = oDebugEvent.u.Exception.dwFirstChance ? _T("1") : _T("2");
    PTSTR sbContinuable = oDebugEvent.u.Exception.ExceptionRecord.ExceptionFlags == 0 ? _T("true") : _T("false");
    ULONG_PTR uAccessType = oDebugEvent.u.Exception.ExceptionRecord.ExceptionInformation[0];
    _tprintf(_T("{\"sEventName\": \"inPageError\", \"iProcessId\": %u, \"iThreadId\": %u, \"sThreadISA\": \"%s\", \"iChance\": %s, \"bContinuable\": %s, \"iCodeAddress\": %Iu, \"iEventAddress\": %Iu,  \"sType\": \"%s\", \"iNtStatus\": %Iu};\r\n"),
        oDebugEvent.dwProcessId, oDebugEvent.dwThreadId, oThreadContext.bThreadIs64Bit ? _T("x64") : _T("x86"),
        siChance, sbContinuable, oDebugEvent.u.Exception.ExceptionRecord.ExceptionAddress, oDebugEvent.u.Exception.ExceptionRecord.ExceptionInformation[1],
        uAccessType == 0 ? _T("read") : uAccessType == 1 ? _T("write") : _T("execute"),
        oDebugEvent.u.Exception.ExceptionRecord.ExceptionInformation[2]);
    return TRUE;
  }
  return FALSE;
}