#include "stdafx.h"
#include "breakpoints.h"
#include "cProcessInformation.h"
#include "cThreadContext.h"
#include "getRemoteProcAddress.h"
#include "handleDebugEvent.h"
#include "JSONStringify.h"
#include "logHeapAllocations.h"

#define LOG(x) x

#ifndef LOG
#define LOG(x)
#endif

typedef struct _HEAP_ALLOC_CALL {
  DWORD dwProcessId;
  DWORD dwThreadId;
  HANDLE hHeapHandle;
  ULONG uFlags;
  SIZE_T uSize;
} HEAP_ALLOC_CALL;
typedef struct _HEAP_FREE_CALL {
  DWORD dwProcessId;
  DWORD dwThreadId;
  HANDLE hHeapHandle;
  ULONG uFlags;
  PVOID pHeapBase;
} HEAP_FREE_CALL;

std::map<DWORD, std::map<DWORD, HEAP_ALLOC_CALL*>> gdoActiveHeapAllocCall_by_dwThreadId_by_dwProcessId;
std::map<DWORD, std::map<DWORD, HEAP_FREE_CALL*>> gdoActiveHeapFreeCall_by_dwThreadId_by_dwProcessId;

BOOL handleRtlAllocateHeapReturn(DEBUG_EVENT& oDebugEvent, cThreadContext& oThreadContext, cProcessInformation& oProcessInformation) {
  if (oThreadContext.bThreadIs64Bit && !oProcessInformation.bIs64Bit) {
    _tprintf(_T("{\"sEventName\": \"internalError\", \"sErrorMessage\": \"Return from ntdll!HeapAlloc in x86 process %u, but x64 thread %u should never happen.\"};\r\n"),
      oDebugEvent.dwProcessId, oDebugEvent.dwThreadId);
    return FALSE;
  }
  if (gdoActiveHeapAllocCall_by_dwThreadId_by_dwProcessId.count(oDebugEvent.dwProcessId) == 0) {
    // No breakpoint was expected in this process at this address - something is fubar.
    _tprintf(_T("{\"sEventName\": \"internalError\", \"sErrorMessage\": \"Return from ntdll!HeapAlloc without a call.\"};\r\n"));
    return FALSE;
  }
  PBYTE pReturnValue, pInstructionPointer;
#ifdef _WIN64
  if (oThreadContext.bThreadIs64Bit) {
    pReturnValue = (PBYTE)oThreadContext.x64.Rax;
    pInstructionPointer = (PBYTE)oThreadContext.x64.Rip;
  } else
#endif
  {
    pReturnValue = (PBYTE)oThreadContext.x86.Eax;
    pInstructionPointer = (PBYTE)oThreadContext.x86.Eip;
  }
  if (gdoActiveHeapAllocCall_by_dwThreadId_by_dwProcessId[oDebugEvent.dwProcessId].count(oDebugEvent.dwThreadId) == 0) {
    LOG(_tprintf(_T("logHeapAllocation: ntdll!HeapAlloc : Breakpoint for caller 0x%IX in process %u, thread %u ignored."), 
        pInstructionPointer, oDebugEvent.dwProcessId, oDebugEvent.dwThreadId));
    // A breakpoint at this address was expected, but not in this thread, so ignore this.
    return TRUE;
  }
  HEAP_ALLOC_CALL* poActiveHeapAllocCall = gdoActiveHeapAllocCall_by_dwThreadId_by_dwProcessId[oDebugEvent.dwProcessId][oDebugEvent.dwThreadId];
  gdoActiveHeapAllocCall_by_dwThreadId_by_dwProcessId[oDebugEvent.dwProcessId].erase(oDebugEvent.dwThreadId);
  if (gdoActiveHeapAllocCall_by_dwThreadId_by_dwProcessId[oDebugEvent.dwProcessId].empty()) {
    gdoActiveHeapAllocCall_by_dwThreadId_by_dwProcessId.erase(oDebugEvent.dwProcessId);
  }
  _tprintf(_T("{\"sEventName\": \"HeapAlloc\", \"iProcessId\": %u, \"iThreadId\": %u, \"sThreadISA\": \"%s\", \"iSize\": %u, \"iAddress\": %Iu};\r\n"),
    oDebugEvent.dwProcessId, oDebugEvent.dwThreadId, oThreadContext.bThreadIs64Bit ? _T("x64") : _T("x86"), poActiveHeapAllocCall->uSize, pReturnValue);
  delete poActiveHeapAllocCall;
  if (!removeBreakpoint(oDebugEvent.dwProcessId, pInstructionPointer, handleRtlAllocateHeapReturn)) {
    _tprintf(_T("{\"sEventName\": \"internalError\", \"sErrorMessage\": \"Cannot remove breakpoint on ntdll!HeapAlloc return address.\"};\r\n"));
    return FALSE;
  }
  LOG(_tprintf(_T("logHeapAllocation: ntdll!HeapAlloc: Breakpoint for caller 0x%IX removed in process %u.\r\n"), pInstructionPointer, oDebugEvent.dwProcessId));
  return TRUE;
}
BOOL handleRtlAllocateHeapCall(DEBUG_EVENT& oDebugEvent, cThreadContext& oThreadContext, cProcessInformation& oProcessInformation) {
  if (oThreadContext.bThreadIs64Bit && !oProcessInformation.bIs64Bit) {
    LOG(_tprintf(_T("logHeapAllocation: ntdll!HeapAlloc: Call by x64 code in x86 process %u, thread %u ignored.\r\n"), oDebugEvent.dwProcessId, oDebugEvent.dwThreadId));
    // 64-bit code running in 32-bit process: this is WoW64 code and not the application, so ignore it.
    return TRUE;
  }
  if (gdoActiveHeapAllocCall_by_dwThreadId_by_dwProcessId[oDebugEvent.dwProcessId].count(oDebugEvent.dwThreadId) != 0) {
    LOG(_tprintf(_T("logHeapAllocation: ntdll!HeapAlloc: Recursive call in process %u, thread %u ignored.\r\n"), oDebugEvent.dwProcessId, oDebugEvent.dwThreadId));
    return TRUE;
  }
  HEAP_ALLOC_CALL* poActiveHeapAllocCall = new HEAP_ALLOC_CALL();
  poActiveHeapAllocCall->dwProcessId = oDebugEvent.dwProcessId;
  poActiveHeapAllocCall->dwThreadId = oDebugEvent.dwThreadId;
  PBYTE pStackPointer, pRetAddress;
#ifdef _WIN64
  if (oThreadContext.bThreadIs64Bit) {
    pStackPointer = (PBYTE)oThreadContext.x64.Rsp;
    poActiveHeapAllocCall->hHeapHandle = (HANDLE)oThreadContext.x64.Rcx;
    poActiveHeapAllocCall->uFlags = (ULONG)oThreadContext.x64.Rdx;
    poActiveHeapAllocCall->uSize = (SIZE_T)oThreadContext.x64.R8;
    if (!oProcessInformation.readBytes(pStackPointer, (PBYTE)&pRetAddress, sizeof(pRetAddress), FALSE)) {
      _tprintf(_T("{\"sEventName\": \"internalError\", \"sErrorMessage\": \"Cannot read ntdll!HeapAlloc return address from stack.\"};\r\n"));
      delete poActiveHeapAllocCall;
      return FALSE;
    }
  } else
#endif
  {
    pStackPointer = (PBYTE)oThreadContext.x86.Esp;
    DWORD* pdwBuffer = new DWORD[4];
    if (!oProcessInformation.readBytes(pStackPointer, (PBYTE)pdwBuffer, 4 * sizeof(DWORD), FALSE)) {
      _tprintf(_T("{\"sEventName\": \"internalError\", \"sErrorMessage\": \"Cannot read ntdll!HeapAlloc return address and arguments from stack.\"};\r\n"));
      delete poActiveHeapAllocCall;
      delete pdwBuffer;
      return FALSE;
    }
    pRetAddress = (PBYTE)pdwBuffer[0];
    poActiveHeapAllocCall->hHeapHandle = (HANDLE)pdwBuffer[1];
    poActiveHeapAllocCall->uFlags = (ULONG)pdwBuffer[2];
    poActiveHeapAllocCall->uSize = (SIZE_T)pdwBuffer[3];
    delete pdwBuffer;
  }
  LOG(_tprintf(_T("logHeapAllocation: ntdll!HeapAlloc: CALL by 0x%IX in process %u, thread %u: HeapAlloc(0x%IX, 0x%X, 0x%X) = ...\r\n"),
      pRetAddress, oDebugEvent.dwProcessId, oDebugEvent.dwThreadId, poActiveHeapAllocCall->hHeapHandle, poActiveHeapAllocCall->uFlags, poActiveHeapAllocCall->uSize));
  if (!setBreakpoint(oDebugEvent.dwProcessId, pRetAddress, handleRtlAllocateHeapReturn)) {
    _tprintf(_T("{\"sEventName\": \"internalError\", \"sErrorMessage\": \"Cannot set breakpoint on ntdll!HeapAlloc return address.\"};\r\n"));
    delete poActiveHeapAllocCall;
    return FALSE;
  }
  LOG(_tprintf(_T("logHeapAllocation: ntdll!HeapAlloc: Breakpoint for caller 0x%IX added in process %u.\r\n"), pRetAddress, oDebugEvent.dwProcessId));

  gdoActiveHeapAllocCall_by_dwThreadId_by_dwProcessId[oDebugEvent.dwProcessId][oDebugEvent.dwThreadId] = poActiveHeapAllocCall;
  return TRUE;
}

BOOL handleRtlFreeHeapReturn(DEBUG_EVENT& oDebugEvent, cThreadContext& oThreadContext, cProcessInformation& oProcessInformation) {
  if (oThreadContext.bThreadIs64Bit && !oProcessInformation.bIs64Bit) {
    _tprintf(_T("{\"sEventName\": \"internalError\", \"sErrorMessage\": \"Return from ntdll!HeapFree in x86 process %u, but x64 thread %u should never happen.\"};\r\n"), 
        oDebugEvent.dwProcessId, oDebugEvent.dwThreadId);
    return FALSE;
  }
  if (gdoActiveHeapFreeCall_by_dwThreadId_by_dwProcessId.count(oDebugEvent.dwProcessId) == 0) {
    // No breakpoint was expected in this process at this address - something is fubar.
    _tprintf(_T("{\"sEventName\": \"internalError\", \"sErrorMessage\": \"Return from ntdll!HeapFree without a call.\"};\r\n"));
    return FALSE;
  }
  BOOLEAN bReturnValue;
  PBYTE pInstructionPointer;
#ifdef _WIN64
  if (oThreadContext.bThreadIs64Bit) {
    bReturnValue = (BOOLEAN)oThreadContext.x64.Rax;
    pInstructionPointer = (PBYTE)oThreadContext.x64.Rip;
  } else
#endif
  {
    bReturnValue = (BOOLEAN)oThreadContext.x86.Eax;
    pInstructionPointer = (PBYTE)oThreadContext.x86.Eip;
  }
  if (gdoActiveHeapFreeCall_by_dwThreadId_by_dwProcessId[oDebugEvent.dwProcessId].count(oDebugEvent.dwThreadId) == 0) {
    LOG(_tprintf(_T("logHeapAllocation: ntdll!HeapFree: Breakpoint for caller 0x%IX in process %u, thread %u ignored."), 
        pInstructionPointer, oDebugEvent.dwProcessId, oDebugEvent.dwThreadId));
    // A breakpoint at this address was expected, but not in this thread, so ignore this.
    return TRUE;
  }
  HEAP_FREE_CALL* poActiveHeapFreeCall = gdoActiveHeapFreeCall_by_dwThreadId_by_dwProcessId[oDebugEvent.dwProcessId][oDebugEvent.dwThreadId];
  gdoActiveHeapFreeCall_by_dwThreadId_by_dwProcessId[oDebugEvent.dwProcessId].erase(oDebugEvent.dwThreadId);
  if (gdoActiveHeapFreeCall_by_dwThreadId_by_dwProcessId[oDebugEvent.dwProcessId].empty()) {
    gdoActiveHeapFreeCall_by_dwThreadId_by_dwProcessId.erase(oDebugEvent.dwProcessId);
  }
  _tprintf(_T("{\"sEventName\": \"HeapFree\", \"iProcessId\": %u, \"iThreadId\": %u, \"sThreadISA\": \"%s\", \"iAddress\": %Iu, \"bResult\": %s};\r\n"),
    oDebugEvent.dwProcessId, oDebugEvent.dwThreadId, oThreadContext.bThreadIs64Bit ? _T("x64") : _T("x86"), poActiveHeapFreeCall->pHeapBase, bReturnValue ? _T("true") : _T("false"));
  delete poActiveHeapFreeCall;
  if (!removeBreakpoint(oDebugEvent.dwProcessId, pInstructionPointer, handleRtlFreeHeapReturn)) {
    _tprintf(_T("{\"sEventName\": \"internalError\", \"sErrorMessage\": \"Cannot remove breakpoint on ntdll!HeapFree return address.\"};\r\n"));
    return FALSE;
  }
  LOG(_tprintf(_T("logHeapAllocation: ntdll!HeapFree: Breakpoint for caller 0x%IX removed in process %u.\r\n"), pInstructionPointer, oDebugEvent.dwProcessId));
  return TRUE;
}
BOOL handleRtlFreeHeapCall(DEBUG_EVENT& oDebugEvent, cThreadContext& oThreadContext, cProcessInformation& oProcessInformation) {
  if (oThreadContext.bThreadIs64Bit && !oProcessInformation.bIs64Bit) {
    LOG(_tprintf(_T("logHeapAllocation: ntdll!HeapFree: Call by x64 code in x86 process %u, thread %u ignored.\r\n"), oDebugEvent.dwProcessId, oDebugEvent.dwThreadId));
    // 64-bit code running in 32-bit process: this is WoW64 code and not the application, so ignore it.
    return TRUE;
  }
  if (gdoActiveHeapFreeCall_by_dwThreadId_by_dwProcessId[oDebugEvent.dwProcessId].count(oDebugEvent.dwThreadId) != 0) {
    LOG(_tprintf(_T("logHeapAllocation: ntdll!HeapFree: Recursive call in process %u, thread %u ignored.\r\n"), oDebugEvent.dwProcessId, oDebugEvent.dwThreadId));
    return TRUE;
  }
  HEAP_FREE_CALL* poActiveHeapFreeCall = new HEAP_FREE_CALL();
  poActiveHeapFreeCall->dwProcessId = oDebugEvent.dwProcessId;
  poActiveHeapFreeCall->dwThreadId = oDebugEvent.dwThreadId;
  PBYTE pStackPointer;
  PBYTE pRetAddress;
#ifdef _WIN64
  if (oThreadContext.bThreadIs64Bit) {
    poActiveHeapFreeCall->hHeapHandle = (PVOID)oThreadContext.x64.Rcx;
    poActiveHeapFreeCall->uFlags = (ULONG)oThreadContext.x64.Rdx;
    poActiveHeapFreeCall->pHeapBase = (PVOID)oThreadContext.x64.R8;
    pStackPointer = (PBYTE)oThreadContext.x64.Rsp;
    if (!oProcessInformation.readBytes(pStackPointer, (PBYTE)&pRetAddress, sizeof(pRetAddress), FALSE)) {
      _tprintf(_T("{\"sEventName\": \"internalError\", \"sErrorMessage\": \"Cannot read ntdll!HeapFree return address from stack.\"};\r\n"));
      delete poActiveHeapFreeCall;
      return FALSE;
    }
  } else
#endif
  {
    pStackPointer = (PBYTE)oThreadContext.x86.Esp;
    DWORD* pdwBuffer = new DWORD[4];
    if (!oProcessInformation.readBytes(pStackPointer, (PBYTE)pdwBuffer, 4 * sizeof(DWORD), FALSE)) {
      _tprintf(_T("{\"sEventName\": \"internalError\", \"sErrorMessage\": \"Cannot read ntdll!HeapFree return address and arguments from stack.\"};\r\n"));
      delete pdwBuffer;
      delete poActiveHeapFreeCall;
      return FALSE;
    }
    pRetAddress = (PBYTE)pdwBuffer[0];
    poActiveHeapFreeCall->hHeapHandle = (HANDLE)pdwBuffer[1];
    poActiveHeapFreeCall->uFlags = (ULONG)pdwBuffer[2];
    poActiveHeapFreeCall->pHeapBase = (PVOID)pdwBuffer[3];
    delete pdwBuffer;
  }
  LOG(_tprintf(_T("logHeapAllocation: ntdll!HeapFree: CALL by 0x%IX in process %u, thread %u: HeapFree(0x%IX, 0x%X, 0x%IX) = ...\r\n"),
      pRetAddress, oDebugEvent.dwProcessId, oDebugEvent.dwThreadId, poActiveHeapFreeCall->hHeapHandle, poActiveHeapFreeCall->uFlags, poActiveHeapFreeCall->pHeapBase));
  
  if (!setBreakpoint(oDebugEvent.dwProcessId, pRetAddress, handleRtlFreeHeapReturn)) {
    _tprintf(_T("{\"sEventName\": \"internalError\", \"sErrorMessage\": \"Cannot set breakpoint on ntdll!HeapFree return address.\"};\r\n"));
    delete poActiveHeapFreeCall;
    return FALSE;
  }
  LOG(_tprintf(_T("logHeapAllocation: ntdll!HeapFree: Breakpoint for caller 0x%IX added in process.\r\n"), pRetAddress, oDebugEvent.dwProcessId));
  gdoActiveHeapFreeCall_by_dwThreadId_by_dwProcessId[oDebugEvent.dwProcessId][oDebugEvent.dwThreadId] = poActiveHeapFreeCall;
  return TRUE;
}

BOOL loadDllCallback(DEBUG_EVENT& oDebugEvent, cThreadContext& oThreadContext, cProcessInformation& oProcessInformation) {
/*  if (oThreadContext.bThreadIs64Bit && !oProcessInformation.bIs64Bit) {
    LOG(_tprintf(_T("logHeapAllocation: LoadLibrary: Call by x64 code in x86 process %u, thread %u ignored.\r\n"), oDebugEvent.dwProcessId, oDebugEvent.dwThreadId));
    // 64-bit code running in 32-bit process: this is WoW64 code and not the application, so ignore it.
    return TRUE;
  }*/
  LOG(_tprintf(_T("logHeapAllocation: process %u (%s), thread %u (%s) => process %s.\r\n"), 
    oDebugEvent.dwProcessId, oProcessInformation.bIs64Bit ? _T("x64") : _T("x86"), 
    oDebugEvent.dwThreadId, oThreadContext.bThreadIs64Bit ? _T("x64") : _T("x86"),
    oThreadContext.bProcessIs64Bit ? _T("x64") : _T("x86")
  ));
  std::basic_string<TCHAR>* psDllFilePath = oProcessInformation.getDllFilePath((PBYTE)oDebugEvent.u.LoadDll.lpBaseOfDll);
  if (!psDllFilePath) {
    _tprintf(_T("{\"sEventName\": \"internalError\", \"sErrorMessage\": \"Cannot get module file name for modulke at 0x%IX.\"};\r\n"), oDebugEvent.u.LoadDll.lpBaseOfDll);
    return FALSE;
  }
  std::basic_string<TCHAR> sNtdll = _T("ntdll.dll");
  std::basic_string<TCHAR>::reverse_iterator ritsNtdll = sNtdll.rbegin();
  std::basic_string<TCHAR>::reverse_iterator ritsDllFilePath = psDllFilePath->rbegin();
  while (ritsNtdll != sNtdll.rend() && ritsDllFilePath != psDllFilePath->rend() && _totlower(*ritsDllFilePath) == *ritsNtdll) {
    ritsNtdll++;
    ritsDllFilePath++;
  };
  if (ritsNtdll != sNtdll.rend()) {
    // DLL path does not end in "ntdll.dll"
    return TRUE;
  }
  if (ritsDllFilePath != psDllFilePath->rend() && *ritsDllFilePath != _T('\\')) {
    // DLL path is not "ntdll.dll" and does not end in "\\ntdll.dll"
    return TRUE;
  }
  // This is ntdll.dll.
  PBYTE pfHeapAlloc = getRemoteProcAddress(oProcessInformation, (PBYTE)oDebugEvent.u.LoadDll.lpBaseOfDll, _T("RtlAllocateHeap"));
  if (!pfHeapAlloc) {
    _tprintf(_T("{\"sEventName\": \"internalError\", \"sErrorMessage\": \"Cannot get address of ntdll!HeapAlloc.\"};\r\n"));
    return FALSE;
  }
  if (!setBreakpoint(oDebugEvent.dwProcessId, pfHeapAlloc, &handleRtlAllocateHeapCall)) {
    _tprintf(_T("{\"sEventName\": \"internalError\", \"sErrorMessage\": \"Cannot set breakpoint on ntdll!HeapAlloc: 0x%X\"};\r\n"), GetLastError());
    return FALSE;
  }
  LOG(_tprintf(_T("logHeapAllocation: ntdll!HeapAlloc: Breakpoint at 0x%IX added in process %u.\r\n"), pfHeapAlloc, oDebugEvent.dwProcessId));
  PBYTE pfHeapFree = getRemoteProcAddress(oProcessInformation, (PBYTE)oDebugEvent.u.LoadDll.lpBaseOfDll, _T("RtlFreeHeap"));
  if (!pfHeapFree) {
    _tprintf(_T("{\"sEventName\": \"internalError\", \"sErrorMessage\": \"Cannot get address of ntdll!HeapFree.\"};\r\n"));
    return FALSE;
  }
  if (!setBreakpoint(oDebugEvent.dwProcessId, pfHeapFree, &handleRtlFreeHeapCall)) {
    _tprintf(_T("{\"sEventName\": \"internalError\", \"sErrorMessage\": \"Cannot set breakpoint on ntdll!HeapFree: 0x%X\"};\r\n"), GetLastError());
    return FALSE;
  }
  LOG(_tprintf(_T("logHeapAllocation: ntdll!HeapFree: Breakpoint at 0x%IX added in process %u.\r\n"), pfHeapFree, oDebugEvent.dwProcessId));
  return TRUE;
}

BOOL logHeapAllocationsInitialize() {
  return addDebugEventCallback(LOAD_DLL_DEBUG_EVENT, loadDllCallback);
}
