#include "stdafx.h"
#include "cProcessInformation.h"
#include "cThreadContext.h"

BOOL handleExceptionAccessViolation(DEBUG_EVENT& oDebugEvent, cThreadContext& oThreadContext, cProcessInformation& oProcessInformation);