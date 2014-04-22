#include "stdafx.h"
#include <map>
#include "cProcessInformation.h"
#include "cThreadContext.h"
#include "handleException.h"
#include "handleExceptionAccessViolation.h"
#include "handleExceptionCxx.h"
#include "handleExceptionInPageError.h"
#include "handleExceptionThreadName.h"

std::map<DWORD, PTSTR> gdException_sEventName_by_dwCode;
PTSTR getExceptionName(DWORD dwExceptionCode) {
  if (gdException_sEventName_by_dwCode.empty()) {
//    gdException_sEventName_by_dwCode[EXCEPTION_ACCESS_VIOLATION] =           _T("accessViolation");
    gdException_sEventName_by_dwCode[EXCEPTION_ARRAY_BOUNDS_EXCEEDED] =      _T("arrayBoundsExceeded");
    gdException_sEventName_by_dwCode[EXCEPTION_BREAKPOINT] =                 _T("breakpoint");
    gdException_sEventName_by_dwCode[EXCEPTION_DATATYPE_MISALIGNMENT] =      _T("datatypeMisalignment");
    gdException_sEventName_by_dwCode[EXCEPTION_FLT_DENORMAL_OPERAND] =       _T("floatingPointDenormalOperand");
    gdException_sEventName_by_dwCode[EXCEPTION_FLT_DIVIDE_BY_ZERO] =         _T("floatingPointDivideByZero");
    gdException_sEventName_by_dwCode[EXCEPTION_FLT_INEXACT_RESULT] =         _T("floatingPointInexactResult");
    gdException_sEventName_by_dwCode[EXCEPTION_FLT_INVALID_OPERATION] =      _T("floatingPointInvalidOperation");
    gdException_sEventName_by_dwCode[EXCEPTION_FLT_OVERFLOW] =               _T("floatingPointOverflow");
    gdException_sEventName_by_dwCode[EXCEPTION_FLT_STACK_CHECK] =            _T("floatingPointStackCheck");
    gdException_sEventName_by_dwCode[EXCEPTION_FLT_UNDERFLOW] =              _T("floatingPointUnderflow");
    gdException_sEventName_by_dwCode[EXCEPTION_ILLEGAL_INSTRUCTION] =        _T("illegalInstruction");
//    gdException_sEventName_by_dwCode[EXCEPTION_IN_PAGE_ERROR] =              _T("inPageError");
    gdException_sEventName_by_dwCode[EXCEPTION_INT_DIVIDE_BY_ZERO] =         _T("integerDivideByZero");
    gdException_sEventName_by_dwCode[EXCEPTION_INT_OVERFLOW] =               _T("integerOverflow");
    gdException_sEventName_by_dwCode[EXCEPTION_INVALID_DISPOSITION] =        _T("invalidDisposition");
    gdException_sEventName_by_dwCode[EXCEPTION_NONCONTINUABLE_EXCEPTION] =   _T("nonContinuableException");
    gdException_sEventName_by_dwCode[EXCEPTION_PRIV_INSTRUCTION] =           _T("privilegedInstruction");
    gdException_sEventName_by_dwCode[EXCEPTION_SINGLE_STEP] =                _T("singleStep");
    gdException_sEventName_by_dwCode[EXCEPTION_STACK_OVERFLOW] =             _T("stackOverflow");
    gdException_sEventName_by_dwCode[0x4000001F] =                           _T("win32x86breakpoint");
  }
  std::map<DWORD, PTSTR>::iterator oExceptionNameFound = gdException_sEventName_by_dwCode.find(dwExceptionCode);
  return oExceptionNameFound == gdException_sEventName_by_dwCode.end() ? NULL : oExceptionNameFound->second; 
}

DWORD handleException(DEBUG_EVENT& oDebugEvent, cThreadContext& oThreadContext, cProcessInformation& oProcessInformation) {
  if (
    !handleExceptionAccessViolation(oDebugEvent, oThreadContext, oProcessInformation)
    && !handleExceptionCxx(oDebugEvent, oThreadContext, oProcessInformation)
    && !handleExceptionInPageError(oDebugEvent, oThreadContext, oProcessInformation)
    && !handleExceptionThreadName(oDebugEvent, oThreadContext, oProcessInformation)
  ) {
    // Convert exception information array into a string:
    std::basic_string<TCHAR> sExceptionInformation;
    TCHAR sExceptionInformationBuffer[21]; // "18446744073709551615\0" in case of unsigned __int64
    for (size_t iIndex = 0; iIndex < oDebugEvent.u.Exception.ExceptionRecord.NumberParameters; iIndex++) {
      if (iIndex) sExceptionInformation += _T(", ");
      _stprintf_s(sExceptionInformationBuffer, sizeof(sExceptionInformationBuffer) / sizeof(TCHAR), _T("%Iu"), oDebugEvent.u.Exception.ExceptionRecord.ExceptionInformation[iIndex]);
      sExceptionInformation += sExceptionInformationBuffer;
    }
    PTSTR sExceptionName = getExceptionName(oDebugEvent.u.Exception.ExceptionRecord.ExceptionCode);
    PTSTR siChance = oDebugEvent.u.Exception.dwFirstChance ? _T("1") : _T("2");
    PTSTR sbContinuable = oDebugEvent.u.Exception.ExceptionRecord.ExceptionFlags == 0 ? _T("true") : _T("false");
    if (sExceptionName) {
      _tprintf(_T("{\"sEventName\": \"%s\", \"iProcessId\": %u, \"iThreadId\": %u, \"sThreadISA\": \"%s\", \"iChance\": %s, \"bContinuable\": %s, \"iCodeAddress\": %Iu, \"aiExceptionInformation\": [%s]};\r\n"),
          sExceptionName, oDebugEvent.dwProcessId, oDebugEvent.dwThreadId, oThreadContext.bThreadIs64Bit ? _T("x64") : _T("x86"),
          siChance, sbContinuable, oDebugEvent.u.Exception.ExceptionRecord.ExceptionAddress, sExceptionInformation.c_str());
    } else {
      _tprintf(_T("{\"sEventName\": \"exception\", \"iProcessId\": %u, \"iThreadId\": %u, \"sThreadISA\": \"%s\", \"iChance\": %s, \"bContinuable\": %s, \"iCodeAddress\": %Iu, \"iExceptionCode\": %u, \"aiExceptionInformation\": [%s]};\r\n"),
          oDebugEvent.dwProcessId, oDebugEvent.dwThreadId, oThreadContext.bThreadIs64Bit ? _T("x64") : _T("x86"),
          siChance, sbContinuable, oDebugEvent.u.Exception.ExceptionRecord.ExceptionAddress, oDebugEvent.u.Exception.ExceptionRecord.ExceptionCode, sExceptionInformation.c_str());
    }
  }
  return oDebugEvent.u.Exception.dwFirstChance ? DBG_EXCEPTION_NOT_HANDLED : NULL;
}
