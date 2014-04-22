#include "stdafx.h"
#include "handleDebugEvent.h"
#include "logHeapAllocations.h"

int _tmain(UINT iArgumentCount, _TCHAR* asArguments[]) {
  PTSTR sCommandLine = _tcschr(GetCommandLine(), _T(' '));
  UINT iCommandIndex = 1;
  BOOL bLogHeapAllocations = FALSE;
  if (sCommandLine) while (1) {
    while (sCommandLine[0] == _T(' ')) sCommandLine++;
    if (_tcsnicmp(sCommandLine, _T("-h "), 3) == 0) {
      bLogHeapAllocations = TRUE;
      sCommandLine += 3;
      iCommandIndex++;
    } else {
      break;
    }
  }
  if (iCommandIndex >= iArgumentCount) {
    _tprintf(_T("Usage:\r\n"));
    _tprintf(_T("    %s [arguments] [command line]\r\n"), asArguments[0]);
    _tprintf(_T("Arguments:\r\n"));
    _tprintf(_T("    -h        Log heap allocations.\r\n"));
    return 0;
  }
  STARTUPINFO oStartupInfo;
  ZeroMemory(&oStartupInfo, sizeof(oStartupInfo));
  oStartupInfo.cb = sizeof(oStartupInfo);
  PROCESS_INFORMATION oProcessInformation;
  ZeroMemory(&oProcessInformation, sizeof(oProcessInformation));
  if (!CreateProcess(asArguments[iCommandIndex], sCommandLine, NULL, NULL, FALSE, DEBUG_PROCESS | CREATE_NEW_CONSOLE, NULL, NULL, &oStartupInfo, &oProcessInformation)) {
    _tprintf(_T("{\"sEventName\": \"internalError\", \"sErrorMessage\": \"CreateProcess failed: 0x%X\"};\r\n"), GetLastError());
    return -1;
  }
  if (bLogHeapAllocations && !logHeapAllocationsInitialize()) {
    return -1;
  }
  for(;;) {
    DEBUG_EVENT oDebugEvent = {0};
    if (!WaitForDebugEvent(&oDebugEvent, INFINITE)) {
      _tprintf(_T("{\"sEventName\": \"internalError\", \"sErrorMessage\": \"WaitForDebugEvent failed: 0x%X\"};\r\n"), GetLastError());
      return -1;
    }
    DWORD dwContinueStatus = handleDebugEvent(oDebugEvent);
    if (!dwContinueStatus) {
      return 0;
    }
    if (!ContinueDebugEvent(oDebugEvent.dwProcessId, oDebugEvent.dwThreadId, dwContinueStatus)) {
      _tprintf(_T("{\"sEventName\": \"internalError\", \"sErrorMessage\": \"ContinueDebugEvent failed: 0x%X\"};\r\n"), GetLastError());
      return -1;
    }
  } 

  return 0;
}

