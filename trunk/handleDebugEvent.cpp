#include "stdafx.h"
#include "breakpoints.h"
#include "cProcessInformation.h"
#include "cThreadContext.h"
#include "getFilePathFromHandle.h"
#include "handleDebugEvent.h"
#include "handleException.h"
#include "JSONStringify.h"
#include <map>
#include <memory>
#include <list>
#include <algorithm>

std::map<DWORD, std::list<DEBUG_EVENT_CALLBACK>> dDebugEvent_afCallbacks_by_dwCode;

BOOL addDebugEventCallback(DWORD dwDebugEventCode, DEBUG_EVENT_CALLBACK fCallback) {
  dDebugEvent_afCallbacks_by_dwCode[dwDebugEventCode].push_back(fCallback);
  return TRUE;
}

BOOL removeDebugEventCallback(DWORD dwDebugEventCode, DEBUG_EVENT_CALLBACK fCallback) {
  std::list<DEBUG_EVENT_CALLBACK> afCallbacks = dDebugEvent_afCallbacks_by_dwCode[dwDebugEventCode];
  std::list<DEBUG_EVENT_CALLBACK>::iterator iafCallback = std::find(afCallbacks.begin(), afCallbacks.end(), fCallback);
  if (iafCallback == afCallbacks.end()) {
    // TODO: Find out how to handle errors
    return FALSE;
  }
  afCallbacks.remove(fCallback);
  if (afCallbacks.empty()) {
    dDebugEvent_afCallbacks_by_dwCode.erase(dwDebugEventCode);
  }
  return TRUE;
}

BOOL fireDebugEventCallbacks(DEBUG_EVENT& oDebugEvent, cThreadContext& oThreadContext, cProcessInformation& oProcessInformation) {
  if (dDebugEvent_afCallbacks_by_dwCode.count(oDebugEvent.dwDebugEventCode) > 0) {
    std::list<DEBUG_EVENT_CALLBACK> afCallbacks = dDebugEvent_afCallbacks_by_dwCode[oDebugEvent.dwDebugEventCode];
    for (std::list<DEBUG_EVENT_CALLBACK>::iterator iafCallback = afCallbacks.begin(); iafCallback != afCallbacks.end(); iafCallback++) {
      DEBUG_EVENT_CALLBACK fCallback = *iafCallback;
      if (!fCallback(oDebugEvent, oThreadContext, oProcessInformation)) {
        return FALSE;
      }
    }
  }
  return TRUE;
}

UINT giProcessCount = 0;
DWORD handleDebugEvent(DEBUG_EVENT& oDebugEvent) {
  std::unique_ptr<cThreadContext> poThreadContext(new cThreadContext(oDebugEvent.dwThreadId));
  if (!poThreadContext->get()) {
    return NULL;
  }
  cProcessInformation* poProcessInformation;
  if (oDebugEvent.dwDebugEventCode == CREATE_PROCESS_DEBUG_EVENT) {
    // createProcess /////////////////////////////////////////////////////////////////////////////
    std::basic_string<TCHAR>* psImageFilePath = getFilePathFromHandle(oDebugEvent.u.CreateProcessInfo.hFile);
    if (!psImageFilePath) {
      _tprintf(_T("{\"sEventName\": \"internalError\", \"sErrorMessage\": \"Cannot get file name for process image at 0x%IX in process %u.\"};\r\n"), 
          oDebugEvent.u.CreateProcessInfo.lpBaseOfImage, oDebugEvent.dwProcessId);
      return NULL;
    }
    poProcessInformation = new cProcessInformation(oDebugEvent.dwProcessId, (PBYTE)oDebugEvent.u.CreateProcessInfo.lpBaseOfImage, psImageFilePath);
    if (!poProcessInformation->get()) {
      return NULL;
    }
    _tprintf(_T("{\"sEventName\": \"createProcess\", \"iProcessId\": %u, \"iThreadId\": %u, \"sProcessISA\": \"%s\", \"sThreadISA\": \"%s\", \"iImageBase\": %Iu, \"iThreadStartAddress\": %Iu, \"sFilePath\": %s};\r\n"),
        oDebugEvent.dwProcessId, oDebugEvent.dwThreadId, poProcessInformation->bIs64Bit ? _T("x64") : _T("x86"), poThreadContext->bThreadIs64Bit ? _T("x64") : _T("x86"),
        oDebugEvent.u.CreateProcessInfo.lpBaseOfImage, oDebugEvent.u.CreateProcessInfo.lpStartAddress, JSONStringify(*psImageFilePath).c_str());
    if (!fireDebugEventCallbacks(oDebugEvent, *poThreadContext, *poProcessInformation)) {
      return NULL;
    }
    giProcessCount++;
    return DBG_CONTINUE;
  }
  poProcessInformation = getProcessInformation(oDebugEvent.dwProcessId);
  if (!poProcessInformation) {
    _tprintf(_T("{\"sEventName\": \"internalError\", \"sErrorMessage\": \"Process %u is unknown\"};\r\n"), oDebugEvent.dwProcessId);
    return NULL;
  }
  switch(oDebugEvent.dwDebugEventCode) {
    // exitProcess ///////////////////////////////////////////////////////////////////////////////
    case EXIT_PROCESS_DEBUG_EVENT: {
      _tprintf(_T("{\"sEventName\": \"exitProcess\", \"iProcessId\": %u, \"iThreadId\": %u, \"sThreadISA\": \"%s\", \"iExitCode\": %u};\r\n"), 
          oDebugEvent.dwProcessId, oDebugEvent.dwThreadId, poThreadContext->bThreadIs64Bit ? _T("x64") : _T("x86"),
          oDebugEvent.u.ExitProcess.dwExitCode);
      if (!fireDebugEventCallbacks(oDebugEvent, *poThreadContext, *poProcessInformation)) {
        return NULL;
      }
      delete poProcessInformation;
      giProcessCount--;
      // If there are no more processes, stop debugging.
      return giProcessCount ? DBG_CONTINUE : NULL;
    }
    // createThread //////////////////////////////////////////////////////////////////////////////
    case CREATE_THREAD_DEBUG_EVENT: {
      _tprintf(_T("{\"sEventName\": \"createThread\", \"iProcessId\": %u, \"iThreadId\": %u, \"sThreadISA\": \"%s\", \"iThreadStartAddress\": %Iu};\r\n"), 
          oDebugEvent.dwProcessId, oDebugEvent.dwThreadId, poThreadContext->bThreadIs64Bit ? _T("x64") : _T("x86"), poThreadContext->bThreadIs64Bit ? _T("x64") : _T("x86"),
          oDebugEvent.u.CreateThread.lpStartAddress);
      if (!fireDebugEventCallbacks(oDebugEvent, *poThreadContext, *poProcessInformation)) {
        return NULL;
      }
      return DBG_CONTINUE;
    }
    // exitThread ////////////////////////////////////////////////////////////////////////////////
    case EXIT_THREAD_DEBUG_EVENT: {
      _tprintf(_T("{\"sEventName\": \"exitThread\", \"iProcessId\": %u, \"iThreadId\": %u, \"sThreadISA\": \"%s\", \"iExitCode\": %u};\r\n"), 
          oDebugEvent.dwProcessId, oDebugEvent.dwThreadId, poThreadContext->bThreadIs64Bit ? _T("x64") : _T("x86"),
          oDebugEvent.u.ExitThread.dwExitCode);
      if (!fireDebugEventCallbacks(oDebugEvent, *poThreadContext, *poProcessInformation)) {
        return NULL;
      }
      return DBG_CONTINUE;
    }
    // loadDll ///////////////////////////////////////////////////////////////////////////////////
    case LOAD_DLL_DEBUG_EVENT: {
      std::basic_string<TCHAR>* psDllFilePath = getFilePathFromHandle(oDebugEvent.u.LoadDll.hFile);
      if (!psDllFilePath) {
        _tprintf(_T("{\"sEventName\": \"internalError\", \"sErrorMessage\": \"Cannot get file name for module image at 0x%IX in process %u.\"};\r\n"),
            oDebugEvent.u.LoadDll.lpBaseOfDll, oDebugEvent.dwProcessId);
        return NULL;
      }
      poProcessInformation->addDll((PBYTE)oDebugEvent.u.LoadDll.lpBaseOfDll, psDllFilePath);
      CloseHandle(oDebugEvent.u.LoadDll.hFile);
      _tprintf(_T("{\"sEventName\": \"loadDll\", \"iProcessId\": %u, \"iThreadId\": %u, \"sThreadISA\": \"%s\", \"iImageBase\": %Iu, \"sFilePath\": %s};\r\n"), 
          oDebugEvent.dwProcessId, oDebugEvent.dwThreadId, poThreadContext->bThreadIs64Bit ? _T("x64") : _T("x86"),
          oDebugEvent.u.LoadDll.lpBaseOfDll, JSONStringify(*psDllFilePath).c_str());
      if (!fireDebugEventCallbacks(oDebugEvent, *poThreadContext, *poProcessInformation)) {
        return NULL;
      }
      return DBG_CONTINUE;
    }
    // unloadDll /////////////////////////////////////////////////////////////////////////////////
    case UNLOAD_DLL_DEBUG_EVENT: {
      // There are some unexplained unload events at the start of a new process, which reference modules for which no load events where ever fired.
      // To work around this, any modules that get unloaded but were never loaded are ignored.
      if (poProcessInformation->removeDll((PBYTE)oDebugEvent.u.UnloadDll.lpBaseOfDll)) {
        _tprintf(_T("{\"sEventName\": \"unloadDll\", \"iProcessId\": %u, \"iThreadId\": %u, \"sThreadISA\": \"%s\", \"iImageBase\": %Iu};\r\n"),
            oDebugEvent.dwProcessId, oDebugEvent.dwThreadId, poThreadContext->bThreadIs64Bit ? _T("x64") : _T("x86"),
            oDebugEvent.u.UnloadDll.lpBaseOfDll);
        if (!fireDebugEventCallbacks(oDebugEvent, *poThreadContext, *poProcessInformation)) {
          return NULL;
        }
      }
      return DBG_CONTINUE;
    }
    // outputDebugString /////////////////////////////////////////////////////////////////////////
    case OUTPUT_DEBUG_STRING_EVENT: {
      std::basic_string<TCHAR>* psDebugString = poProcessInformation->readString(
          (PBYTE)oDebugEvent.u.DebugString.lpDebugStringData, oDebugEvent.u.DebugString.nDebugStringLength, oDebugEvent.u.DebugString.fUnicode ? sizeof(WCHAR) : sizeof(CHAR), FALSE);
      _tprintf(_T("{\"sEventName\": \"outputDebugString\", \"iProcessId\": %u, \"iThreadId\": %u, \"sThreadISA\": \"%s\", \"sString\": %s};\r\n"),
          oDebugEvent.dwProcessId, oDebugEvent.dwThreadId, poThreadContext->bThreadIs64Bit ? _T("x64") : _T("x86"),
          JSONStringify(psDebugString).c_str());
      if (psDebugString) {
        delete psDebugString;
      }
      if (!fireDebugEventCallbacks(oDebugEvent, *poThreadContext, *poProcessInformation)) {
        return NULL;
      }
      return DBG_CONTINUE;
    }
    // exception /////////////////////////////////////////////////////////////////////////////////
    case EXCEPTION_DEBUG_EVENT: {
      if (breakpointCausedException(oDebugEvent, *poThreadContext, *poProcessInformation)) {
        DWORD dwResult = breakpointHandleException(oDebugEvent, *poThreadContext, *poProcessInformation);
        return dwResult;
      } else {
        DWORD dwResult = handleException(oDebugEvent, *poThreadContext, *poProcessInformation);
        if (!dwResult || !fireDebugEventCallbacks(oDebugEvent, *poThreadContext, *poProcessInformation)) {
          return NULL;
        }
        return dwResult;
      }
    }
    default: {
      _tprintf(_T("{\"sEventName\": \"internalError\", \"sErrorMessage\": \"Debug event 0x%X is unknown\"};\r\n"), oDebugEvent.dwDebugEventCode);
      return NULL;
    }
  }
}
