#include "stdafx.h"
#include <string>
#pragma once

class cThreadContext {
public:
  DWORD dwThreadId = NULL;
#ifdef _WIN64
  CONTEXT x64;
  WOW64_CONTEXT x86;
  BOOLEAN bThreadIs64Bit = TRUE;
  BOOLEAN bProcessIs64Bit = TRUE;
#else
  CONTEXT x86;
  const BOOLEAN bThreadIs64Bit = FALSE;
  const BOOLEAN bProcessIs64Bit = FALSE;
#endif
  cThreadContext(DWORD _dwThreadId): dwThreadId(_dwThreadId) {};
  BOOL get();
  BOOL set();
  std::basic_string<TCHAR>* getThreadName();
  VOID setThreadName(std::basic_string<TCHAR>*);
  ~cThreadContext();
private:
  cThreadContext() {};
  HANDLE hThread = NULL;
  std::basic_string<TCHAR>* psName = NULL;
};

