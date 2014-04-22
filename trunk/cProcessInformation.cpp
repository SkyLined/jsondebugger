#include "stdafx.h"
#include "cProcessInformation.h"
#include "JSONStringify.h"
#include <map>

//#define LOG(x) x

#ifndef LOG
#define LOG(x)
#endif

std::map<DWORD, cProcessInformation*> gdpoProcessInformation_by_dwId;

cProcessInformation::cProcessInformation(DWORD _dwProcessId, PBYTE _pBaseAddress, std::basic_string<TCHAR>* _psImageFilePath):
    dwProcessId(_dwProcessId), pBaseAddress(_pBaseAddress), psImageFilePath(_psImageFilePath) {
  LOG(_tprintf(_T("cProcessInformation: new process %u.\r\n"), _dwProcessId));
  gdpoProcessInformation_by_dwId[_dwProcessId] = this;
};

cProcessInformation::~cProcessInformation() {
  LOG(_tprintf(_T("cProcessInformation: delete process %u.\r\n"), dwProcessId));
  if (hProcess) CloseHandle(hProcess);
  if (psImageFilePath) delete psImageFilePath;
  for (
    std::map<PBYTE, std::basic_string<TCHAR>*>::iterator itdDll_psFilePath_by_pBaseAddress = dDll_psFilePath_by_pBaseAddress.begin();
    itdDll_psFilePath_by_pBaseAddress != dDll_psFilePath_by_pBaseAddress.end();
    itdDll_psFilePath_by_pBaseAddress++
  ) {
    delete itdDll_psFilePath_by_pBaseAddress->second;
  }
}

BOOL cProcessInformation::get() {
  if (hProcess) CloseHandle(hProcess);
  hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_VM_OPERATION, FALSE, dwProcessId);
  if (!hProcess) {
    _tprintf(_T("{\"sEventName\": \"internalError\", \"sErrorMessage\": \"Cannot open process %u: 0x%X\"};\r\n"), dwProcessId, GetLastError());
    return FALSE;
  }
  BOOL bIsWoW64;
  if (!IsWow64Process(hProcess, &bIsWoW64)) {
    _tprintf(_T("{\"sEventName\": \"internalError\", \"sErrorMessage\": \"Cannot determine ISA for process %u: 0x%X\"};\r\n"), dwProcessId, GetLastError());
    return FALSE;
  }
  bIs64Bit = !bIsWoW64;
  return TRUE;
}

BOOL cProcessInformation::addDll(PBYTE pAddress, std::basic_string<TCHAR>* psDllFilePath) {
  if (dDll_psFilePath_by_pBaseAddress.count(pAddress) == 1) {
    _tprintf(_T("{\"sEventName\": \"internalError\", \"sErrorMessage\": \"Loaded two DLLs at the same address 0x%IX in process %u: \\\"%s\\\" and \\\"%s\\\"\"};\r\n"), 
        pAddress, dwProcessId, JSONStringifyNoQuotes(*(dDll_psFilePath_by_pBaseAddress[pAddress])).c_str(), JSONStringifyNoQuotes(*psDllFilePath).c_str());
    return FALSE;
  }
  dDll_psFilePath_by_pBaseAddress[pAddress] = psDllFilePath;
  return TRUE;
}


std::basic_string<TCHAR>* cProcessInformation::getDllFilePath(PBYTE pAddress) {
  if (dDll_psFilePath_by_pBaseAddress.count(pAddress) == 1) {
    return dDll_psFilePath_by_pBaseAddress[pAddress];
  }
  return NULL;
}

BOOL cProcessInformation::removeDll(PBYTE pAddress) {
  if (dDll_psFilePath_by_pBaseAddress.count(pAddress) == 1) {
    delete dDll_psFilePath_by_pBaseAddress[pAddress];
    dDll_psFilePath_by_pBaseAddress.erase(pAddress);
    return TRUE;
  }
  return FALSE;
}

BOOL cProcessInformation::readBytes(PBYTE pAddress, PBYTE pBuffer, size_t uCount, BOOL bOutputError) {
  if (!hProcess) {
    _tprintf(_T("{\"sEventName\": \"internalError\", \"sErrorMessage\": \"Cannot read from memory without initializing process %u.\"};\r\n"), dwProcessId);
    return FALSE;
  }
  if (!ReadProcessMemory(hProcess, pAddress, pBuffer, uCount, NULL)) {
    if (bOutputError) {
      _tprintf(_T("{\"sEventName\": \"internalError\", \"sErrorMessage\": \"Cannot read from memory at 0x%IX+0x%IX in process %u: 0x%X\"};\r\n"), pAddress, uCount, dwProcessId, GetLastError());
    }
    return FALSE;
  }
  return TRUE;
}
BOOL cProcessInformation::readByte(PBYTE pAddress, BYTE& xResult, BOOL bOutputError) {
  return readBytes(pAddress, (PBYTE)&xResult, sizeof(xResult), bOutputError);
}
BOOL cProcessInformation::readWord(PWORD pAddress, WORD& xResult, BOOL bOutputError) {
  return readBytes((PBYTE)pAddress, (PBYTE)&xResult, sizeof(xResult), bOutputError);
}
BOOL cProcessInformation::readDWord(PDWORD pAddress, DWORD& xResult, BOOL bOutputError) {
  return readBytes((PBYTE)pAddress, (PBYTE)&xResult, sizeof(xResult), bOutputError);
}
BOOL cProcessInformation::readDWord64(PDWORD64 pAddress, DWORD64& xResult, BOOL bOutputError) {
  return readBytes((PBYTE)pAddress, (PBYTE)&xResult, sizeof(xResult), bOutputError);
}
std::basic_string<TCHAR>* cProcessInformation::readString(PBYTE pAddress, size_t uLength, size_t uCharSize, BOOL bOutputError) {
  if (!hProcess) {
    _tprintf(_T("{\"sEventName\": \"internalError\", \"sErrorMessage\": \"Cannot read from memory without initializing process %u.\"};\r\n"), dwProcessId);
    return FALSE;
  }
  size_t uBufferSize = (uLength + 1) * uCharSize;
  LPBYTE pbBuffer = new BYTE[uBufferSize];
  if (!ReadProcessMemory(hProcess, pAddress, pbBuffer, uLength * uCharSize, NULL)) {
    if (bOutputError) {
      _tprintf(_T("{\"sEventName\": \"internalError\", \"sErrorMessage\": \"Cannot read from memory at 0x%IX+0x%IX in process %u: 0x%X\"};\r\n"), pAddress, uLength * uCharSize, dwProcessId, GetLastError());
    }
    return NULL;
  }
  if (uCharSize == 1) {
    ((LPSTR)pbBuffer)[uLength] = 0;
  } else {
    ((LPWSTR)pbBuffer)[uLength] = 0;
  }
  std::basic_string<TCHAR>* psString;
  if (uCharSize == sizeof(_TCHAR)) {
    psString = new std::basic_string<TCHAR>((PTSTR)pbBuffer);
  } else {
    PTSTR sConvertedBuffer = new TCHAR[uLength + 1];
    _stprintf_s(sConvertedBuffer, uLength + 1, _T("%S"), pbBuffer);
    psString = new std::basic_string<TCHAR>(sConvertedBuffer);
  }
  return psString;
}
std::basic_string<TCHAR>* cProcessInformation::readCharString(PCHAR pAddress, size_t uLength, BOOL bOutputError) {
  return readString((PBYTE)pAddress, uLength, sizeof(CHAR));
}
std::basic_string<TCHAR>* cProcessInformation::readWCharString(PWCHAR pAddress, size_t uLength, BOOL bOutputError) {
  return readString((PBYTE)pAddress, uLength, sizeof(WCHAR));
}

std::basic_string<TCHAR>* cProcessInformation::readString(PBYTE pAddress, size_t uCharSize, BOOL bOutputError) {
  if (!hProcess) {
    _tprintf(_T("{\"sEventName\": \"internalError\", \"sErrorMessage\": \"Cannot read from memory without initializing process %u.\"};\r\n"), dwProcessId);
    return FALSE;
  }
  std::basic_string<TCHAR>* psString = new std::basic_string<TCHAR>(_T(""));
  if (uCharSize == sizeof(TCHAR)) {
    TCHAR cChar;
    while (1) {
      if (!ReadProcessMemory(hProcess, pAddress, (PBYTE)&cChar, sizeof(TCHAR), NULL)) {
        if (bOutputError) {
          _tprintf(_T("{\"sEventName\": \"internalError\", \"sErrorMessage\": \"Cannot read from memory at 0x%IX+0x%IX in process %u: 0x%X\"};\r\n"), pAddress, sizeof(TCHAR), dwProcessId, GetLastError());
        }
        break;
      }
      if (!cChar) {
        break; // Terminated by '\0'
      }
      *psString += cChar;
      pAddress += uCharSize;
    }
  } else {
    TCHAR sCharConversionBuffer[2];
    PBYTE pbBuffer = new BYTE[uCharSize * 2];
    pbBuffer[1] = 0;
    while (1) {
      if (!ReadProcessMemory(hProcess, pAddress, pbBuffer, uCharSize, NULL)) { // Terminated by access violation
        if (bOutputError) {
          _tprintf(_T("{\"sEventName\": \"internalError\", \"sErrorMessage\": \"Cannot read from memory at 0x%IX+0x%IX in process %u: 0x%X\"};\r\n"), pAddress, uCharSize, dwProcessId, GetLastError());
        }
        break;
      }
      _stprintf_s(sCharConversionBuffer, 2, _T("%S"), pbBuffer);
      if (!sCharConversionBuffer[0]) {
        break; // Terminated by '\0'
      }
      *psString += sCharConversionBuffer[0];
      pAddress += uCharSize;
    }
  }
  return psString;
}
std::basic_string<TCHAR>* cProcessInformation::readCharString(PCHAR pAddress, BOOL bOutputError) {
  return readString((PBYTE)pAddress, sizeof(CHAR), bOutputError);
}
std::basic_string<TCHAR>* cProcessInformation::readWCharString(PWCHAR pAddress, BOOL bOutputError) {
  return readString((PBYTE)pAddress, sizeof(WCHAR), bOutputError);
}
BOOL cProcessInformation::writeBytes(PBYTE pAddress, PBYTE pBuffer, size_t uCount, BOOL bOutputError, BOOL bIgnoreProtection) {
  LOG(_tprintf(_T("cProcessInformation.writeBytes: Writing 0x%IX bytes to 0x%IX in process %u.\r\n"), uCount, pAddress, dwProcessId));
  if (!hProcess) {
    _tprintf(_T("{\"sEventName\": \"internalError\", \"sErrorMessage\": \"Cannot write to memory without initializing process %u.\"};\r\n"), dwProcessId);
    return FALSE;
  }
  DWORD fOldProtection;
  if (!WriteProcessMemory(hProcess, pAddress, pBuffer, uCount, NULL)) {
    BOOL bWriteSucceeded = FALSE;
    if (bIgnoreProtection) {
      LOG(_tprintf(_T("cProcessInformation.writeBytes:   - Direct write failed; attempting to modify memory protection.\r\n")));
      if (!VirtualProtectEx(hProcess, pAddress, uCount, PAGE_READWRITE, &fOldProtection)) {
        if (bOutputError) {
          _tprintf(_T("{\"sEventName\": \"internalError\", \"sErrorMessage\": \"Cannot modify memory protection flags at 0x%IX*0x%IX in process %u: 0x%X\"};\r\n"), pAddress, uCount, dwProcessId, GetLastError());
        }
        return FALSE;
      }
      bWriteSucceeded = WriteProcessMemory(hProcess, pAddress, pBuffer, uCount, NULL);
    }
    if (!bWriteSucceeded) {
      if (bOutputError) {
        _tprintf(_T("{\"sEventName\": \"internalError\", \"sErrorMessage\": \"Cannot write to memory at 0x%IX*0x%IX in process %u: 0x%X\"};\r\n"), pAddress, uCount, dwProcessId, GetLastError());
      }
      return FALSE;
    }
    if (fOldProtection == PAGE_READONLY) {
      LOG(_tprintf(_T("cProcessInformation.writeBytes:   + Cannot restore memory protection.\r\n")));
    } else {
      LOG(_tprintf(_T("cProcessInformation.writeBytes:   + Attempting to restore memory protection (0x%IX).\r\n"), fOldProtection));
      if (!VirtualProtectEx(hProcess, pAddress, uCount, fOldProtection, NULL)) {
        if (bOutputError) {
          _tprintf(_T("{\"sEventName\": \"internalError\", \"sErrorMessage\": \"Cannot modify memory protection flags at 0x%IX*0x%IX in process %u: 0x%X\"};\r\n"), pAddress, uCount, dwProcessId, GetLastError());
        }
        return FALSE;
      }
    }
  }
  LOG(_tprintf(_T("cProcessInformation.writeBytes:   + Write successful.\r\n")));
  if (!FlushInstructionCache(hProcess, pAddress, uCount)) {
    if (bOutputError) {
      _tprintf(_T("{\"sEventName\": \"internalError\", \"sErrorMessage\": \"Cannot flush instruction cache at 0x%IX*0x%IX in process %u: 0x%X\"};\r\n"), pAddress, uCount, dwProcessId, GetLastError());
    }
    return FALSE;
  }
  LOG(_tprintf(_T("cProcessInformation.writeBytes:   + Instruction cache flushed.\r\n")));
  LOG(_tprintf(_T("cProcessInformation.writeBytes:   + OK.\r\n")));
  return TRUE;
}
BOOL cProcessInformation::writeByte(PBYTE pAddress, BYTE& xResult, BOOL bOutputError, BOOL bIgnoreProtection) {
  return writeBytes(pAddress, (PBYTE)&xResult, sizeof(xResult), bOutputError, bIgnoreProtection);
}
BOOL cProcessInformation::writeWord(PWORD pAddress, WORD& xResult, BOOL bOutputError, BOOL bIgnoreProtection) {
  return writeBytes((PBYTE)pAddress, (PBYTE)&xResult, sizeof(xResult), bOutputError, bIgnoreProtection);
}
BOOL cProcessInformation::writeDWord(PDWORD pAddress, DWORD& xResult, BOOL bOutputError, BOOL bIgnoreProtection) {
  return writeBytes((PBYTE)pAddress, (PBYTE)&xResult, sizeof(xResult), bOutputError, bIgnoreProtection);
}
BOOL cProcessInformation::writeDWord64(PDWORD64 pAddress, DWORD64& xResult, BOOL bOutputError, BOOL bIgnoreProtection) {
  return writeBytes((PBYTE)pAddress, (PBYTE)&xResult, sizeof(xResult), bOutputError, bIgnoreProtection);
}

cProcessInformation* getProcessInformation(DWORD dwProcessId) {
  if (gdpoProcessInformation_by_dwId.count(dwProcessId) == 0) {
    _tprintf(_T("{\"sEventName\": \"internalError\", \"sErrorMessage\": \"Process %u is unknown\"};\r\n"), dwProcessId);
    return NULL;
  }
  LOG(_tprintf(_T("cProcessInformation: request for process %u.\r\n"), dwProcessId));
  cProcessInformation* poProcessInformation = gdpoProcessInformation_by_dwId[dwProcessId];
  poProcessInformation->get();
  return poProcessInformation;
}

