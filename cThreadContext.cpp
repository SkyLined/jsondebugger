#include "stdafx.h"
#include "cThreadContext.h"

//#define LOG(x) x

#ifndef LOG
#define LOG(x)
#endif

cThreadContext::~cThreadContext() {
  if (hThread) CloseHandle(hThread);
  if (psName) delete psName;
}

BOOL cThreadContext::get() {
  if (!hThread) {
    hThread = OpenThread(THREAD_GET_CONTEXT | THREAD_SET_CONTEXT, FALSE, dwThreadId);
    if (!hThread) {
      _tprintf(_T("{\"sEventName\": \"internalError\", \"sErrorMessage\": \"Cannot open thread %u: 0x%X\"};\r\n"), dwThreadId, GetLastError());
      return FALSE;
    }
  }
#ifdef _WIN64
  x64.ContextFlags = CONTEXT_ALL;
  if (!GetThreadContext(hThread, &x64)) {
    _tprintf(_T("{\"sEventName\": \"internalError\", \"sErrorMessage\": \"Cannot get context of thread %u: 0x%X\"};\r\n"), dwThreadId, GetLastError());
    return FALSE;
  }
  // http://int0h.wordpress.com/2009/12/24/the-power-of-wow64/
  bProcessIs64Bit = x64.SegCs == 0x33;
  if (!bProcessIs64Bit) {
    bThreadIs64Bit = FALSE;
    x86.ContextFlags = CONTEXT_ALL;
    if (!Wow64GetThreadContext(hThread, &x86)) {
      _tprintf(_T("{\"sEventName\": \"internalError\", \"sErrorMessage\": \"Cannot get WoW64 context of thread %u: 0x%X\"};\r\n"), dwThreadId, GetLastError());
      return FALSE;
    }
  }
#else
  x86.ContextFlags = CONTEXT_ALL;
  if (!GetThreadContext(hThread, &x86)) {
    _tprintf(_T("{\"sEventName\": \"internalError\", \"sErrorMessage\": \"Cannot get context of thread %u: 0x%X\"};\r\n"), dwThreadId, GetLastError());
    return FALSE;
  }
#endif
  return TRUE;
}

BOOL cThreadContext::set() {
  if (!hThread) {
    hThread = OpenThread(THREAD_GET_CONTEXT | THREAD_SET_CONTEXT, FALSE, dwThreadId);
    if (!hThread) {
      _tprintf(_T("{\"sEventName\": \"internalError\", \"sErrorMessage\": \"Cannot open thread %u: 0x%X\"};\r\n"), dwThreadId, GetLastError());
      return FALSE;
    }
  }
#ifdef _WIN64
  BOOL bSetThreadContextResult = bThreadIs64Bit ? SetThreadContext(hThread, &x64) : Wow64SetThreadContext(hThread, &x86);
#else
  BOOL bSetThreadContextResult = SetThreadContext(hThread, &x86);
#endif
  if (!bSetThreadContextResult) {
    _tprintf(_T("{\"sEventName\": \"internalError\", \"sErrorMessage\": \"Cannot set context of thread %u: 0x%X\"};\r\n"), dwThreadId, GetLastError());
    return FALSE;
  }
  LOG(_tprintf(_T("cThreadContext: Set %s context in thread %u.\r\n"), bThreadIs64Bit ? _T("x64") : _T("x86"), dwThreadId));
  return TRUE;
}

std::basic_string<TCHAR>* cThreadContext::getThreadName() {
  return psName;
}
VOID cThreadContext::setThreadName(std::basic_string<TCHAR>* psNewName) {
  if (psName) {
    delete psName;
  }
  psName = psNewName;
}
