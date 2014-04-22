#include "stdafx.h"
#include "cProcessInformation.h"
#include "cThreadContext.h"
#include <map>

typedef BOOL(*BREAKPOINT_CALLBACK)(DEBUG_EVENT& oDebugEvent, cThreadContext& oThreadContext, cProcessInformation& oProcessInformation);

BOOL hasBreakpoint(DWORD dwProcessId, PBYTE pAddress);
BOOL setBreakpoint(DWORD dwProcessId, PBYTE pAddress, BREAKPOINT_CALLBACK fCallback = NULL);
BOOL removeBreakpoint(DWORD dwProcessId, PBYTE pAddress, BREAKPOINT_CALLBACK fCallback = NULL);
BOOL breakpointCausedException(DEBUG_EVENT& oDebugEvent, cThreadContext& oThreadContext, cProcessInformation& oProcessInformation);
DWORD breakpointHandleException(DEBUG_EVENT& oDebugEvent, cThreadContext& oThreadContext, cProcessInformation& oProcessInformation);
