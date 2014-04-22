#include "stdafx.h"
#include "cProcessInformation.h"
#include "cThreadContext.h"
#include "handleExceptionCxx.h"
#include "JSONStringify.h"

// http://members.gamedev.net/sicrane/articles/exception.html

std::basic_string<TCHAR> dumpBytes(PBYTE pInput, size_t uSize) {
  std::basic_string<TCHAR> sOutput;
  TCHAR sBuffer[3];
  size_t uCharCount = 0;
  while (uSize--) {
    if (uCharCount == 0) sOutput += _T("  ");
    _stprintf_s(sBuffer, 3, _T("%02X"), *pInput++);
    sOutput += sBuffer;
    if (uSize) {
      if (++uCharCount == 0x8) {
        sOutput += _T("\r\n");
        uCharCount = 0;
      } else {
        sOutput += _T(" ");
      }
    }
  }
  return sOutput;
}

// http://members.gamedev.net/sicrane/articles/exception.html
typedef struct __PMD {
  int mdisp;
  int pdisp;
  int vdisp;
} PMD;

typedef void*__ptr32 PMFN32;

typedef char TYPE_DESCRIPTOR_NAME_CHAR;
typedef struct _TYPE_DESCRIPTOR32 {
  unsigned long hash;
  void*__ptr32 spare;
  TYPE_DESCRIPTOR_NAME_CHAR name;
} TYPE_DESCRIPTOR32, *__ptr32 PTYPE_DESCRIPTOR32;
typedef struct _TYPE_DESCRIPTOR64 {
  unsigned long hash;
  void*__ptr64 spare;
  TYPE_DESCRIPTOR_NAME_CHAR name;
} TYPE_DESCRIPTOR64, *__ptr64 PTYPE_DESCRIPTOR64;

typedef struct _CATCHABLE_TYPE32 {
  unsigned int properties;
  PTYPE_DESCRIPTOR32 pType;
  PMD thisDisplacement;
  int sizeOrOffset;
  PMFN32 copyFunction;
} CATCHABLE_TYPE32, *__ptr32 PCATCHABLE_TYPE32;
typedef PCATCHABLE_TYPE32*__ptr32 PPCATCHABLE_TYPE32;
 
typedef int CATCHABLE_TYPE_ARRAY_LENGTH32;
typedef PCATCHABLE_TYPE32 CATCHABLE_TYPE_ARRAY_ENTRY32;
typedef struct _CATCHABLE_TYPE_ARRAY32 {
  CATCHABLE_TYPE_ARRAY_LENGTH32 nCatchableTypes;
  CATCHABLE_TYPE_ARRAY_ENTRY32 arrayOfCatchableTypes;
} CATCHABLE_TYPE_ARRAY32, *__ptr32 PCATCHABLE_TYPE_ARRAY32;

typedef struct _THROW_INFO32 {
  unsigned int attributes;
  PMFN32 pmfnUnwind;
  int (__cdecl *__ptr32 pForwardCompat)(...);
  PCATCHABLE_TYPE_ARRAY32 pCatchableTypeArray;
} THROW_INFO32, *__ptr32 PTHROW_INFO32;

BOOL handleExceptionCxx64(DEBUG_EVENT& oDebugEvent, cThreadContext& oThreadContext, cProcessInformation& oProcessInformation) {
  if (oDebugEvent.u.Exception.ExceptionRecord.ExceptionCode == 0xE06D7363 && 
      (oDebugEvent.u.Exception.ExceptionRecord.NumberParameters == 3 || oDebugEvent.u.Exception.ExceptionRecord.NumberParameters == 4) &&
      oDebugEvent.u.Exception.ExceptionRecord.ExceptionInformation[0] == 0x19930520) {
    std::basic_string<TCHAR> sObjectNames;
    size_t uOffset = oDebugEvent.u.Exception.ExceptionRecord.NumberParameters == 4 ? oDebugEvent.u.Exception.ExceptionRecord.ExceptionInformation[3] : 0;
    //_tprintf(_T("uOffset: 0x%IX\r\n"), uOffset);
    THROW_INFO32 oThrowInfo;
    PBYTE pRemoteThrowInfo = (PBYTE)oDebugEvent.u.Exception.ExceptionRecord.ExceptionInformation[2];
    if (!oProcessInformation.readBytes(pRemoteThrowInfo, (PBYTE)&oThrowInfo, sizeof(oThrowInfo), FALSE)) {
      //_tprintf(_T("* Cannot read oThrowInfo (%u bytes at 0x%IX): 0x%X\r\n"), sizeof(oThrowInfo), pRemoteThrowInfo, GetLastError());
    } else {
      //PBYTE pBase = (PBYTE)&oThrowInfo;
      //_tprintf(_T("oThrowInfo @+0x%IX {\r\n/*\r\n%s\r\n*/\r\n"), pRemoteThrowInfo - uOffset, dumpBytes((PBYTE)&oThrowInfo, sizeof(oThrowInfo)).c_str());
      //_tprintf(_T("  attributes: 0x%IX (%u bytes @+0x%IX)\r\n"), oThrowInfo.attributes, sizeof(oThrowInfo.attributes), ((PBYTE)&oThrowInfo.attributes) - pBase);
      //_tprintf(_T("  pmfnUnwind: 0x%IX (%u bytes @+0x%IX)\r\n"), oThrowInfo.pmfnUnwind, sizeof(oThrowInfo.pmfnUnwind), ((PBYTE)&oThrowInfo.pmfnUnwind) - pBase);
      //_tprintf(_T("  pForwardCompat: 0x%IX (%u bytes @+0x%IX)\r\n"), oThrowInfo.pForwardCompat, sizeof(oThrowInfo.pForwardCompat), ((PBYTE)&oThrowInfo.pForwardCompat) - pBase);
      //_tprintf(_T("  pCatchableTypeArray: 0x%IX (%u bytes @+0x%IX)\r\n}\r\n"), oThrowInfo.pCatchableTypeArray, sizeof(oThrowInfo.pCatchableTypeArray), ((PBYTE)&oThrowInfo.pCatchableTypeArray) - pBase);
      CATCHABLE_TYPE_ARRAY_LENGTH32 uCatchableTypeArrayLength;
      PBYTE pRemoteCatchableTypeArray = ((PBYTE)oThrowInfo.pCatchableTypeArray) + uOffset;
      if (!oProcessInformation.readBytes(pRemoteCatchableTypeArray, (PBYTE)&uCatchableTypeArrayLength, sizeof(uCatchableTypeArrayLength), FALSE)) {
        //_tprintf(_T("* Cannot read uCatchableTypeArrayLength (%u bytes at 0x%IX): 0x%X\r\n"), 
        //    sizeof(uCatchableTypeArrayLength), pRemoteCatchableTypeArray, GetLastError());
      } else {
        //_tprintf(_T("oCatchableTypeArray @+0x%IX {\r\n/*\r\n%s\r\n*/\r\n"), pRemoteCatchableTypeArray - uOffset, dumpBytes((PBYTE)&uCatchableTypeArrayLength, sizeof(uCatchableTypeArrayLength)).c_str());
        //_tprintf(_T("  nCatchableTypes: 0x%X (%u bytes @+0x0)\r\n"), uCatchableTypeArrayLength, sizeof(uCatchableTypeArrayLength));
        pRemoteCatchableTypeArray += sizeof(uCatchableTypeArrayLength);
        size_t uCatchableTypeArraySize = uCatchableTypeArrayLength * sizeof(CATCHABLE_TYPE_ARRAY_ENTRY32);
        PBYTE apCatchableTypes = new BYTE[uCatchableTypeArraySize];
        if (!oProcessInformation.readBytes(pRemoteCatchableTypeArray, apCatchableTypes, uCatchableTypeArraySize, FALSE)) {
          //_tprintf(_T("* Cannot read aoCatchableTypeArray (%u bytes at 0x%IX): 0x%X\r\n"), 
          //    uCatchableTypeArraySize, pRemoteCatchableTypeArray, GetLastError());
        } else {
          //_tprintf(_T("/*\r\n%s\r\n*/\r\n"), dumpBytes(apCatchableTypes, uCatchableTypeArraySize).c_str());
          //_tprintf(_T("  arrayOfCatchableTypes: (%u bytes @+0x4) [\r\n"), uCatchableTypeArraySize);
          for (size_t iIndex = 0; (int)iIndex < uCatchableTypeArrayLength; iIndex++) {
            CATCHABLE_TYPE32 oCatchableType;
            PBYTE ppRemoteCatchableType = apCatchableTypes + iIndex * sizeof(PCATCHABLE_TYPE32);
            PBYTE pRemoteCatchableType = (PBYTE)(*(PCATCHABLE_TYPE32*)ppRemoteCatchableType) + uOffset;
            if (!oProcessInformation.readBytes(pRemoteCatchableType, (PBYTE)&oCatchableType, sizeof(oCatchableType), FALSE)) {
              //_tprintf(_T("* Cannot read oCatchableType (%u bytes at 0x%IX): 0x%X\r\n"), 
              //    sizeof(oCatchableType), pRemoteCatchableType, GetLastError());
            } else {
              //PBYTE pBase = (PBYTE)&oCatchableType;
              //_tprintf(_T("    &oCatchableType @+0x%IX {\r\n/*\r\n%s\r\n*/\r\n"), pRemoteCatchableType - uOffset, dumpBytes((PBYTE)&oCatchableType, sizeof(oCatchableType)).c_str());
              //_tprintf(_T("      properties: 0x%X (%u bytes @+0x%X)\r\n"), oCatchableType.properties, sizeof(oCatchableType.properties), ((PBYTE)&oCatchableType.properties) - pBase);
              //_tprintf(_T("      pType: 0x%IX (%u bytes @+0x%X)\r\n"), oCatchableType.pType, sizeof(oCatchableType.pType), ((PBYTE)&oCatchableType.pType) - pBase);
              TYPE_DESCRIPTOR64 oTypeDescriptor;
              PBYTE pRemoteTypeDescriptor = ((PBYTE)oCatchableType.pType) + uOffset;
              if (!oProcessInformation.readBytes(pRemoteTypeDescriptor, (PBYTE)&oTypeDescriptor, sizeof(oTypeDescriptor), FALSE)) {
                //_tprintf(_T("* Cannot read oTypeDescriptor (%u bytes at 0x%IX): 0x%X\r\n"), 
                //    sizeof(oTypeDescriptor), pRemoteTypeDescriptor, GetLastError());
              } else {
                PBYTE pRemoteTypeDescriptorName = pRemoteTypeDescriptor + (((PBYTE)&oTypeDescriptor.name) - ((PBYTE)&oTypeDescriptor));
                std::basic_string<TCHAR>* psObjectName = oProcessInformation.readString(pRemoteTypeDescriptorName, sizeof(TYPE_DESCRIPTOR_NAME_CHAR), FALSE);
                PBYTE pBase = (PBYTE)&oTypeDescriptor;
                //_tprintf(_T("      oTypeDescriptor @ 0x%IX {\r\n/*\r\n%s\r\n*/\r\n"), pRemoteTypeDescriptor - uOffset, dumpBytes((PBYTE)&oTypeDescriptor, sizeof(oTypeDescriptor)).c_str());
                //_tprintf(_T("        hash: 0x%X ((%u bytes @+0x%X)\r\n"), oTypeDescriptor.hash, sizeof(oTypeDescriptor.hash), ((PBYTE)&oTypeDescriptor.hash) - pBase);
                //_tprintf(_T("        spare: 0x%X ((%u bytes @+0x%X)\r\n"), oTypeDescriptor.spare, sizeof(oTypeDescriptor.spare), ((PBYTE)&oTypeDescriptor.spare) - pBase);
                //_tprintf(_T("        name: %s (@+0x%X)\r\n"), JSONStringify(sObjectName).c_str(), pRemoteTypeDescriptorName - pRemoteTypeDescriptor);
                //_tprintf(_T("      }\r\n"));
                if (!sObjectNames.empty()) sObjectNames += _T(", ");
                sObjectNames += JSONStringify(*psObjectName);
                delete psObjectName;
              }
              //_tprintf(_T("      ...\r\n"));
              //_tprintf(_T("    },\r\n"));
            }
          }
          //_tprintf(_T("  ]\r\n}\r\n"));
        }
      }
    }
    PTSTR siChance = oDebugEvent.u.Exception.dwFirstChance ? _T("1") : _T("2");
    PTSTR sbContinuable = oDebugEvent.u.Exception.ExceptionRecord.ExceptionFlags == 0 ? _T("true") : _T("false");
    _tprintf(_T("{\"sEventName\": \"exceptionC++\", \"iProcessId\": %u, \"iThreadId\": %u, \"sThreadISA\": \"%s\", \"iChance\": %s, \"bContinuable\": %s, \"iCodeAddress\": %Iu, \"sObjectNames\": [%s]};\r\n"),
        oDebugEvent.dwProcessId, oDebugEvent.dwThreadId, oThreadContext.bThreadIs64Bit ? _T("x64") : _T("x86"),
        siChance, sbContinuable, oDebugEvent.u.Exception.ExceptionRecord.ExceptionAddress, sObjectNames.c_str());
    return TRUE;
  }    
  return FALSE;
}

BOOL handleExceptionCxx32(DEBUG_EVENT& oDebugEvent, cThreadContext& oThreadContext, cProcessInformation& oProcessInformation) {
  if (oDebugEvent.u.Exception.ExceptionRecord.ExceptionCode == 0xE06D7363 && 
      oDebugEvent.u.Exception.ExceptionRecord.NumberParameters == 3 &&
      oDebugEvent.u.Exception.ExceptionRecord.ExceptionInformation[0] == 0x19930520) {
    std::basic_string<TCHAR> sObjectNames;
    size_t uOffset = 0; // Only used in the x64 version
    THROW_INFO32 oThrowInfo;
    PBYTE pRemoteThrowInfo = ((PBYTE)oDebugEvent.u.Exception.ExceptionRecord.ExceptionInformation[2]) + uOffset;
    if (!oProcessInformation.readBytes(pRemoteThrowInfo, (PBYTE)&oThrowInfo, sizeof(oThrowInfo), FALSE)) {
      //_tprintf(_T("* Cannot read oThrowInfo (%u bytes at 0x%IX): 0x%X\r\n"), sizeof(oThrowInfo), pRemoteThrowInfo, GetLastError());
    } else {
      //PBYTE pBase = (PBYTE)&oThrowInfo;
      //_tprintf(_T("oThrowInfo @ 0x%X {\r\n/*\r\n%s\r\n*/\r\n"), pRemoteThrowInfo, dumpBytes((PBYTE)&oThrowInfo, sizeof(oThrowInfo)).c_str());
      //_tprintf(_T("  attributes: 0x%X (%u bytes @+0x%X)\r\n"), oThrowInfo.attributes, sizeof(oThrowInfo.attributes), ((PBYTE)&oThrowInfo.attributes) - pBase);
      //_tprintf(_T("  pmfnUnwind: 0x%X (%u bytes @+0x%X)\r\n"), oThrowInfo.pmfnUnwind, sizeof(oThrowInfo.pmfnUnwind), ((PBYTE)&oThrowInfo.pmfnUnwind) - pBase);
      //_tprintf(_T("  pForwardCompat: 0x%X (%u bytes @+0x%X)\r\n"), oThrowInfo.pForwardCompat, sizeof(oThrowInfo.pForwardCompat), ((PBYTE)&oThrowInfo.pForwardCompat) - pBase);
      //_tprintf(_T("  pCatchableTypeArray: 0x%X (%u bytes @+0x%X)\r\n}\r\n"), oThrowInfo.pCatchableTypeArray, sizeof(oThrowInfo.pCatchableTypeArray), ((PBYTE)&oThrowInfo.pCatchableTypeArray) - pBase);
      CATCHABLE_TYPE_ARRAY_LENGTH32 uCatchableTypeArrayLength;
      PBYTE pRemoteCatchableTypeArray = ((PBYTE)oThrowInfo.pCatchableTypeArray) + uOffset;
      if (!oProcessInformation.readBytes(pRemoteCatchableTypeArray, (PBYTE)&uCatchableTypeArrayLength, sizeof(uCatchableTypeArrayLength), FALSE)) {
        //_tprintf(_T("* Cannot read uCatchableTypeArrayLength (%u bytes at 0x%X): 0x%X\r\n"), 
        //    sizeof(uCatchableTypeArrayLength), pRemoteCatchableTypeArray, GetLastError());
      } else {
        //_tprintf(_T("oCatchableTypeArray @ 0x%X {\r\n/*\r\n%s\r\n*/\r\n"), pRemoteCatchableTypeArray, dumpBytes((PBYTE)&uCatchableTypeArrayLength, sizeof(uCatchableTypeArrayLength)).c_str());
        //_tprintf(_T("  nCatchableTypes: 0x%X (%u bytes @+0x0)\r\n"), uCatchableTypeArrayLength, sizeof(uCatchableTypeArrayLength));
        pRemoteCatchableTypeArray += sizeof(uCatchableTypeArrayLength);
        size_t uCatchableTypeArraySize = uCatchableTypeArrayLength * sizeof(CATCHABLE_TYPE_ARRAY_ENTRY32);
        PBYTE apCatchableTypes = new BYTE[uCatchableTypeArraySize];
        if (!oProcessInformation.readBytes(pRemoteCatchableTypeArray, apCatchableTypes, uCatchableTypeArraySize, FALSE)) {
          //_tprintf(_T("* Cannot read aoCatchableTypeArray (%u bytes at 0x%X): 0x%X\r\n"), 
          //    uCatchableTypeArraySize, pRemoteCatchableTypeArray, GetLastError());
        } else {
          //_tprintf(_T("/*\r\n%s\r\n*/\r\n"), dumpBytes(apCatchableTypes, uCatchableTypeArraySize).c_str());
          //_tprintf(_T("  arrayOfCatchableTypes: (%u bytes @+0x4) [\r\n"), uCatchableTypeArraySize);
          for (size_t iIndex = 0; (int)iIndex < uCatchableTypeArrayLength; iIndex++) {
            CATCHABLE_TYPE32 oCatchableType;
            PBYTE ppRemoteCatchableType = apCatchableTypes + iIndex * sizeof(PCATCHABLE_TYPE32);
            PBYTE pRemoteCatchableType = (PBYTE)(*(PCATCHABLE_TYPE32*)ppRemoteCatchableType) + uOffset;
            if (!oProcessInformation.readBytes(pRemoteCatchableType, (PBYTE)&oCatchableType, sizeof(oCatchableType), FALSE)) {
              //_tprintf(_T("* Cannot read oCatchableType (%u bytes at 0x%X): 0x%X\r\n"), 
              //    sizeof(oCatchableType), pRemoteCatchableType, GetLastError());
            } else {
              //PBYTE pBase = (PBYTE)&oCatchableType;
              //_tprintf(_T("    &oCatchableType @ 0x%X {\r\n/*\r\n%s\r\n*/\r\n"), pRemoteCatchableType, dumpBytes((PBYTE)&oCatchableType, sizeof(oCatchableType)).c_str());
              //_tprintf(_T("      properties: 0x%X (%u bytes @+0x%X)\r\n"), oCatchableType.properties, sizeof(oCatchableType.properties), ((PBYTE)&oCatchableType.properties) - pBase);
              //_tprintf(_T("      pType: 0x%X (%u bytes @+0x%X)\r\n"), oCatchableType.pType, sizeof(oCatchableType.pType), ((PBYTE)&oCatchableType.pType) - pBase);
              TYPE_DESCRIPTOR32 oTypeDescriptor;
              PBYTE pRemoteTypeDescriptor = ((PBYTE)oCatchableType.pType) + uOffset;
              if (!oProcessInformation.readBytes(pRemoteTypeDescriptor, (PBYTE)&oTypeDescriptor, sizeof(oTypeDescriptor), FALSE)) {
                //_tprintf(_T("* Cannot read oTypeDescriptor (%u bytes at 0x%X): 0x%X\r\n"), 
                //    sizeof(oTypeDescriptor), pRemoteTypeDescriptor, GetLastError());
              } else {
                PBYTE pRemoteTypeDescriptorName = pRemoteTypeDescriptor + (((PBYTE)&oTypeDescriptor.name) - ((PBYTE)&oTypeDescriptor));
                std::basic_string<TCHAR>* psObjectName = oProcessInformation.readString(pRemoteTypeDescriptorName, sizeof(TYPE_DESCRIPTOR_NAME_CHAR), FALSE);
                //PBYTE pBase = (PBYTE)&oTypeDescriptor;
                //_tprintf(_T("      oTypeDescriptor @ 0x%X {\r\n/*\r\n%s\r\n*/\r\n"), pRemoteTypeDescriptor, dumpBytes((PBYTE)&oTypeDescriptor, sizeof(oTypeDescriptor)).c_str());
                //_tprintf(_T("        hash: 0x%X ((%u bytes @+0x%X)\r\n"), oTypeDescriptor.hash, sizeof(oTypeDescriptor.hash), ((PBYTE)&oTypeDescriptor.hash) - pBase);
                //_tprintf(_T("        spare: 0x%X ((%u bytes @+0x%X)\r\n"), oTypeDescriptor.spare, sizeof(oTypeDescriptor.spare), ((PBYTE)&oTypeDescriptor.spare) - pBase);
                //_tprintf(_T("        name: %s (@0x%X)\r\n"), JSONStringify(sObjectName).c_str(), pRemoteTypeDescriptorName);
                //_tprintf(_T("      }\r\n"));
                if (!sObjectNames.empty()) sObjectNames += _T(", ");
                sObjectNames += JSONStringify(*psObjectName);
                delete psObjectName;
              }
              //_tprintf(_T("      ...\r\n"));
              //_tprintf(_T("    },\r\n"));
            }
          }
          //_tprintf(_T("  ]\r\n}\r\n"));
        }
      }
    }
    PTSTR siChance = oDebugEvent.u.Exception.dwFirstChance ? _T("1") : _T("2");
    PTSTR sbContinuable = oDebugEvent.u.Exception.ExceptionRecord.ExceptionFlags == 0 ? _T("true") : _T("false");
    _tprintf(_T("{\"sEventName\": \"exceptionC++\", \"iProcessId\": %u, \"iThreadId\": %u, \"sThreadISA\": \"%s\", \"iChance\": %s, \"bContinuable\": %s, \"iCodeAddress\": %Iu, \"asObjectNames\": [%s]};\r\n"),
        oDebugEvent.dwProcessId, oDebugEvent.dwThreadId, oThreadContext.bThreadIs64Bit ? _T("x64") : _T("x86"),
        siChance, sbContinuable, oDebugEvent.u.Exception.ExceptionRecord.ExceptionAddress, sObjectNames.c_str());
    return TRUE;
  }    
  return FALSE;
}

BOOL handleExceptionCxx(DEBUG_EVENT& oDebugEvent, cThreadContext& oThreadContext, cProcessInformation& oProcessInformation) {
  #ifdef _WIN64
    return oThreadContext.bThreadIs64Bit ?
        handleExceptionCxx64(oDebugEvent, oThreadContext, oProcessInformation)
        : handleExceptionCxx32(oDebugEvent, oThreadContext, oProcessInformation);
  #else
    return handleExceptionCxx32(oDebugEvent, oThreadContext, oProcessInformation);
  #endif
};