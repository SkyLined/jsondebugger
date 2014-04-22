#include "stdafx.h"
#include <map>
#include <string>
#pragma once

class cProcessInformation {
public:
  DWORD dwProcessId;
  PBYTE pBaseAddress;
  BOOL bIs64Bit;
  HANDLE hProcess;
  std::basic_string<TCHAR>* psImageFilePath;

  cProcessInformation(DWORD _dwProcessId, PBYTE _pBaseAddress, std::basic_string<TCHAR>* _psImageFilePath);
  ~cProcessInformation();
  BOOL get();

  BOOL addDll(PBYTE pAddress, std::basic_string<TCHAR>* psDllFilePath);
  std::basic_string<TCHAR>* getDllFilePath(PBYTE pAddress);
  BOOL removeDll(PBYTE pAddress);

  BOOL readBytes(PBYTE pAddress, PBYTE pBuffer, size_t uCount, BOOL bOutputError);
  BOOL readByte(PBYTE pAddress, BYTE& bResult, BOOL bOutputError);
  BOOL readWord(PWORD pAddress, WORD& wResult, BOOL bOutputError);
  BOOL readDWord(PDWORD pAddress, DWORD& dwResult, BOOL bOutputError);
  BOOL readDWord64(PDWORD64 pAddress, DWORD64& dw64Result, BOOL bOutputError);
  
  std::basic_string<TCHAR>* readString(PBYTE pAddress, size_t uStringLength, size_t uStringCharSize, BOOL bOutputError);
  std::basic_string<TCHAR>* readCharString(PCHAR pAddress, size_t uLength, BOOL bOutputError);
  std::basic_string<TCHAR>* readWCharString(PWCHAR pAddress, size_t uLength, BOOL bOutputError);
  std::basic_string<TCHAR>* readString(PBYTE pAddress, size_t uStringCharSize, BOOL bOutputError);
  std::basic_string<TCHAR>* readCharString(PCHAR pAddress, BOOL bOutputError);
  std::basic_string<TCHAR>* readWCharString(PWCHAR pAddress, BOOL bOutputError);
  
  BOOL writeBytes(PBYTE pAddress, PBYTE pBuffer, size_t uCount, BOOL bOutputError, BOOL bIgnoreProtection = FALSE);
  BOOL writeByte(PBYTE pAddress, BYTE& bResult, BOOL bOutputError, BOOL bIgnoreProtection = FALSE);
  BOOL writeWord(PWORD pAddress, WORD& wResult, BOOL bOutputError, BOOL bIgnoreProtection = FALSE);
  BOOL writeDWord(PDWORD pAddress, DWORD& dwResult, BOOL bOutputError, BOOL bIgnoreProtection = FALSE);
  BOOL writeDWord64(PDWORD64 pAddress, DWORD64& dw64Result, BOOL bOutputError, BOOL bIgnoreProtection = FALSE);

private:
  std::map<PBYTE, std::basic_string<TCHAR>*> dDll_psFilePath_by_pBaseAddress;
};

cProcessInformation* getProcessInformation(DWORD dwProcessId);
