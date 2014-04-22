#include "stdafx.h"
#include "cProcessInformation.h"
#include "cThreadContext.h"
#include "handleExceptionThreadName.h"
#include "JSONStringify.h"

#pragma pack(push,8)
  typedef struct _THREADNAME_INFO32 {
     DWORD dwType;          // Must be 0x1000.
     BYTE *__ptr32 szName;  // Pointer to name (in user addr space).
     DWORD dwThreadID;      // Thread ID (-1=caller thread).
     DWORD dwFlags;         // Reserved for future use, must be zero.
  } THREADNAME_INFO32;
  #ifdef _WIN64
    typedef struct _THREADNAME_INFO64 {
       DWORD dwType;          // Must be 0x1000.
       BYTE *__ptr64 szName;  // Pointer to name (in user addr space).
       DWORD dwThreadID;      // Thread ID (-1=caller thread).
       DWORD dwFlags;         // Reserved for future use, must be zero.
    } THREADNAME_INFO64;
  #endif
#pragma pack(pop)

BOOL handleExceptionThreadName32(DEBUG_EVENT& oDebugEvent, cThreadContext& oThreadContext, cProcessInformation& oProcessInformation) {
  if (oDebugEvent.u.Exception.ExceptionRecord.ExceptionCode == 0x406D1388 && oDebugEvent.u.Exception.ExceptionRecord.NumberParameters == 4) {
    THREADNAME_INFO32& oThreadNameInfo = *(THREADNAME_INFO32*)oDebugEvent.u.Exception.ExceptionRecord.ExceptionInformation;
    if (oThreadNameInfo.dwType == 0x1000) {
      PTSTR siChance = oDebugEvent.u.Exception.dwFirstChance ? _T("1") : _T("2");
      PTSTR sbContinuable = oDebugEvent.u.Exception.ExceptionRecord.ExceptionFlags == 0 ? _T("true") : _T("false");
      oThreadContext.setThreadName(oProcessInformation.readString((PBYTE)oThreadNameInfo.szName, oDebugEvent.u.DebugString.fUnicode ? sizeof(WCHAR) : sizeof(CHAR), FALSE));
      _tprintf(_T("{\"sEventName\": \"threadName\", \"iProcessId\": %u, \"iThreadId\": %u, \"sThreadISA\": \"%s\", \"iChance\": %s, \"bContinuable\": %s, \"iNamedThreadId\": %u, \"sThreadName\": %s};\r\n"),
          oDebugEvent.dwProcessId, oDebugEvent.dwThreadId, oThreadContext.bThreadIs64Bit ? _T("x64") : _T("x86"),
          siChance, sbContinuable, oThreadNameInfo.dwThreadID, JSONStringify(*oThreadContext.getThreadName()).c_str());
    }
  }
  return FALSE;
}
#ifdef _WIN64
  BOOL handleExceptionThreadName64(DEBUG_EVENT& oDebugEvent, cThreadContext& oThreadContext, cProcessInformation& oProcessInformation) {
    if (oDebugEvent.u.Exception.ExceptionRecord.ExceptionCode == 0x406D1388 && oDebugEvent.u.Exception.ExceptionRecord.NumberParameters == 4) {
      THREADNAME_INFO64& oThreadNameInfo = *(THREADNAME_INFO64*)oDebugEvent.u.Exception.ExceptionRecord.ExceptionInformation;
      if (oThreadNameInfo.dwType == 0x1000) {
        PTSTR siChance = oDebugEvent.u.Exception.dwFirstChance ? _T("1") : _T("2");
        PTSTR sbContinuable = oDebugEvent.u.Exception.ExceptionRecord.ExceptionFlags == 0 ? _T("true") : _T("false");
        oThreadContext.setThreadName(oProcessInformation.readString(oThreadNameInfo.szName, oDebugEvent.u.DebugString.fUnicode ? sizeof(WCHAR) : sizeof(CHAR), FALSE));
        _tprintf(_T("{\"sEventName\": \"threadName\", \"iProcessId\": %u, \"iThreadId\": %u, \"sThreadISA\": \"%s\", \"iChance\": %s, \"bContinuable\": %s, \"iNamedThreadId\": %u, \"sThreadName\": %s};\r\n"),
            oDebugEvent.dwProcessId, oDebugEvent.dwThreadId, oThreadContext.bThreadIs64Bit ? _T("x64") : _T("x86"),
            siChance, sbContinuable, oThreadNameInfo.dwThreadID, JSONStringify(*oThreadContext.getThreadName()).c_str());
        return TRUE;
      }
    }
    return FALSE;
  }
#endif

BOOL handleExceptionThreadName(DEBUG_EVENT& oDebugEvent, cThreadContext& oThreadContext, cProcessInformation& oProcessInformation) {
  #ifdef _WIN64
    return oThreadContext.bThreadIs64Bit ?
        handleExceptionThreadName64(oDebugEvent, oThreadContext, oProcessInformation)
        : handleExceptionThreadName32(oDebugEvent, oThreadContext, oProcessInformation);
  #else
    return handleExceptionThreadName32(oDebugEvent, oThreadContext, oProcessInformation);
  #endif
};