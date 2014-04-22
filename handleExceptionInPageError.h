#include "stdafx.h"
#include "cProcessInformation.h"
#include "cThreadContext.h"

BOOL handleExceptionInPageError(DEBUG_EVENT& oDebugEvent, cThreadContext& oThreadContext, cProcessInformation& oProcessInformation);