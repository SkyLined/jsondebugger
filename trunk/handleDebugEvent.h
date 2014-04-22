#include "stdafx.h"
#include "cProcessInformation.h"
#include "cThreadContext.h"
#include <map>
#include <string>

typedef BOOL(*DEBUG_EVENT_CALLBACK)(DEBUG_EVENT& oDebugEvent, cThreadContext& oThreadContext, cProcessInformation& oProcessInformation);
BOOL addDebugEventCallback(DWORD dwDebugEventCode, DEBUG_EVENT_CALLBACK fCallback);
BOOL removeDebugEventCallback(DWORD dwDebugEventCode, DEBUG_EVENT_CALLBACK fCallback);
DWORD handleDebugEvent(DEBUG_EVENT& oDebugEvent);