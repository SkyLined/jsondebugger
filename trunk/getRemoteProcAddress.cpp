#include "stdafx.h"
#include "cProcessInformation.h"
#include "JSONStringify.h"

//#define LOG(x) x

#ifndef LOG
#define LOG(x)
#endif

PBYTE getRemoteProcAddress(cProcessInformation& oProcessInformation, PBYTE pBaseAddress, PTSTR sProcedureName) {
  IMAGE_DOS_HEADER oImageDOSHeader;
  if (!oProcessInformation.readBytes(pBaseAddress, (PBYTE)&oImageDOSHeader, sizeof(oImageDOSHeader), FALSE)) {
    _tprintf(_T("{\"sEventName\": \"warning\", \"sErrorMessage\": \"Cannot read IMAGE_DOS_HEADER at 0x%IX*0x%IX from module at 0x%IX in process %u.\"};\r\n"),
        pBaseAddress, sizeof(oImageDOSHeader), pBaseAddress, oProcessInformation.dwProcessId);
    return NULL;
  }
  if (oImageDOSHeader.e_magic != IMAGE_DOS_SIGNATURE) {
    _tprintf(_T("{\"sEventName\": \"warning\", \"sErrorMessage\": \"Invalid IMAGE_DOS_HEADER (at 0x%IX) magic (0x%04X vs 0x%04X) in module at 0x%IX in process %u.\"};\r\n"),
      pBaseAddress, oImageDOSHeader.e_magic, IMAGE_DOS_SIGNATURE, pBaseAddress, oProcessInformation.dwProcessId);
    return NULL;
  }
  LOG(_tprintf(_T("getRemoteProcAddress: IMAGE_DOS_HEADER @ 0x%IX*0x%IX OK\r\n"), pBaseAddress, sizeof(IMAGE_DOS_HEADER)));
  PBYTE pImageNtHeadersAddress = pBaseAddress + oImageDOSHeader.e_lfanew;
  DWORD dwImageNTHeaders_Signature;
  if (!oProcessInformation.readDWord((PDWORD)(pImageNtHeadersAddress), dwImageNTHeaders_Signature, FALSE)) {
    _tprintf(_T("{\"sEventName\": \"warning\", \"sErrorMessage\": \"Cannot read IMAGE_NT_HEADERS Signature at 0x%IX*0x%IX from module at 0x%IX in process %u.\"};\r\n"),
      pBaseAddress, sizeof(DWORD), pBaseAddress, oProcessInformation.dwProcessId);
    return NULL;
  }
  if (dwImageNTHeaders_Signature != IMAGE_NT_SIGNATURE) {
    _tprintf(_T("{\"sEventName\": \"warning\", \"sErrorMessage\": \"Invalid IMAGE_NT_HEADERS (at 0x%IX) Signature (0x%08X vs 0x%08X) at 0x%IX from module at 0x%IX in process %u.\"};\r\n"),
      pImageNtHeadersAddress, dwImageNTHeaders_Signature, IMAGE_NT_SIGNATURE, pImageNtHeadersAddress, pBaseAddress, oProcessInformation.dwProcessId);
    return NULL;
  }
  LOG(_tprintf(_T("getRemoteProcAddress: IMAGE_NT_HEADERS @ 0x%IX*0x?? OK\r\n"), pImageNtHeadersAddress));
  PBYTE pFileHeaderAddress = pImageNtHeadersAddress + sizeof(dwImageNTHeaders_Signature);
  IMAGE_FILE_HEADER oFileHeader;
  if (!oProcessInformation.readBytes(pFileHeaderAddress, (PBYTE)&oFileHeader, sizeof(oFileHeader), FALSE)) {
    _tprintf(_T("{\"sEventName\": \"warning\", \"sErrorMessage\": \"Cannot read IMAGE_FILE_HEADER at 0x%IX*0x%IX from module at 0x%IX in process %u.\"};\r\n"),
      pFileHeaderAddress, sizeof(oFileHeader), pBaseAddress, oProcessInformation.dwProcessId);
    return NULL;
  }
  PBYTE pOptionalHeaderAddress = pFileHeaderAddress + sizeof(IMAGE_FILE_HEADER);
  PBYTE pFirstDataDirectoryAddress;
  // The export table is the first data directory after the optional header. The size of that header depends on the type of image:
  size_t uOptionalHeaderSize;
  if (oFileHeader.Machine == IMAGE_FILE_MACHINE_I386) {
    LOG(_tprintf(_T("getRemoteProcAddress: IMAGE_FILE_HEADER @ 0x%IX*0x%IX = x86\r\n"), pFileHeaderAddress, sizeof(IMAGE_FILE_HEADER)));
    IMAGE_OPTIONAL_HEADER32 oOptionalHeader;
    if (!oProcessInformation.readBytes(pOptionalHeaderAddress, (PBYTE)&oOptionalHeader, sizeof(oOptionalHeader), FALSE)) {
      _tprintf(_T("{\"sEventName\": \"warning\", \"sErrorMessage\": \"Cannot read IMAGE_OPTIONAL_HEADER32 at 0x%IX*0x%IX from module at 0x%IX in process %u.\"};\r\n"),
          pOptionalHeaderAddress, sizeof(oOptionalHeader), pBaseAddress, oProcessInformation.dwProcessId);
      return NULL;
    }
    uOptionalHeaderSize = (PBYTE)&oOptionalHeader.DataDirectory - (PBYTE)&oOptionalHeader;
    if (oOptionalHeader.NumberOfRvaAndSizes == 0) {
      _tprintf(_T("{\"sEventName\": \"warning\", \"sErrorMessage\": \"No Export Table in IMAGE_OPTIONAL_HEADER32 at 0x%IX*0x%IX from module at 0x%IX in process %u.\"};\r\n"),
        pOptionalHeaderAddress, sizeof(oOptionalHeader), pBaseAddress, oProcessInformation.dwProcessId);
      return NULL;
    }
  } else if (oFileHeader.Machine == IMAGE_FILE_MACHINE_AMD64) {
    LOG(_tprintf(_T("getRemoteProcAddress: IMAGE_FILE_HEADER @ 0x%IX*0x%IX = x64\r\n"), pFileHeaderAddress, sizeof(IMAGE_FILE_HEADER)));
    IMAGE_OPTIONAL_HEADER64 oOptionalHeader;
    if (!oProcessInformation.readBytes(pOptionalHeaderAddress, (PBYTE)&oOptionalHeader, sizeof(oOptionalHeader), FALSE)) {
      _tprintf(_T("{\"sEventName\": \"warning\", \"sErrorMessage\": \"Cannot read IMAGE_OPTIONAL_HEADER64 at 0x%IX*0x%IX from module at 0x%IX in process %u.\"};\r\n"),
          pOptionalHeaderAddress, sizeof(oOptionalHeader), pBaseAddress, oProcessInformation.dwProcessId);
      return NULL;
    }
    uOptionalHeaderSize = (PBYTE)&oOptionalHeader.DataDirectory - (PBYTE)&oOptionalHeader;
    if (oOptionalHeader.NumberOfRvaAndSizes == 0) {
      _tprintf(_T("{\"sEventName\": \"warning\", \"sErrorMessage\": \"No Export Table in IMAGE_OPTIONAL_HEADER64 at 0x%IX*0x%IX from module at 0x%IX in process %u.\"};\r\n"),
        pOptionalHeaderAddress, sizeof(oOptionalHeader), pBaseAddress, oProcessInformation.dwProcessId);
      return NULL;
    }
  } else {
    _tprintf(_T("{\"sEventName\": \"warning\", \"sErrorMessage\": \"Unknown IMAGE_FILE_HEADER (at 0x%IX) Machine (0x%04X) in module at 0x%IX in process %u.\"};\r\n"),
      pFileHeaderAddress, oFileHeader.Machine, pBaseAddress, oProcessInformation.dwProcessId);
    return NULL;
  }
  pFirstDataDirectoryAddress = pOptionalHeaderAddress + uOptionalHeaderSize;
  LOG(_tprintf(_T("getRemoteProcAddress: IMAGE_OPTIONAL_HEADER @ 0x%IX*0x%IX OK\r\n"), pOptionalHeaderAddress, uOptionalHeaderSize));
  LOG(_tprintf(_T("getRemoteProcAddress: => First IMAGE_DATA_DIRECTORY @ 0x%IX*0x%IX.\r\n"), pFirstDataDirectoryAddress, sizeof(IMAGE_DATA_DIRECTORY)));
  IMAGE_DATA_DIRECTORY oFirstDataDirectory;
  if (!oProcessInformation.readBytes(pFirstDataDirectoryAddress, (PBYTE)&oFirstDataDirectory, sizeof(oFirstDataDirectory), FALSE)) {
    _tprintf(_T("{\"sEventName\": \"warning\", \"sErrorMessage\": \"Cannot read IMAGE_DATA_DIRECTORY at 0x%IX*0x%IX from module at 0x%IX in process %u.\"};\r\n"),
        pFirstDataDirectoryAddress, sizeof(oFirstDataDirectory), pBaseAddress, oProcessInformation.dwProcessId);
    return NULL;
  }
  IMAGE_EXPORT_DIRECTORY oExportTable;
  if (sizeof(oExportTable) >= oFirstDataDirectory.Size) {
    _tprintf(_T("{\"sEventName\": \"warning\", \"sErrorMessage\": \"Invalid IMAGE_EXPORT_DIRECTORY (at 0x%IX) Size (0x%X vs 0x%X) in module at 0x%IX in process %u.\"};\r\n"),
      pFirstDataDirectoryAddress, oFirstDataDirectory.Size, sizeof(oExportTable), pBaseAddress, oProcessInformation.dwProcessId);
    return NULL;
  }
  LOG(_tprintf(_T("getRemoteProcAddress: IMAGE_DATA_DIRECTORY @ 0x%IX*0x%IX OK\r\n"), pFirstDataDirectoryAddress, sizeof(oFirstDataDirectory)));
  PBYTE pExportTableAddress = pBaseAddress + oFirstDataDirectory.VirtualAddress;
  if (!oProcessInformation.readBytes(pExportTableAddress, (PBYTE)&oExportTable, sizeof(oExportTable), FALSE)) {
    _tprintf(_T("{\"sEventName\": \"warning\", \"sErrorMessage\": \"Cannot read IMAGE_EXPORT_DIRECTORY at 0x%IX*0x%IX from module at 0x%IX in process %u.\"};\r\n"),
      pExportTableAddress, sizeof(oExportTable), pBaseAddress, oProcessInformation.dwProcessId);
    return NULL;
  }
  LOG(_tprintf(_T("getRemoteProcAddress: IMAGE_EXPORT_DIRECTORY @ 0x%IX*0x%IX OK\r\n"), pExportTableAddress, sizeof(oExportTable)));
  PBYTE pNamesTableAddress = pBaseAddress + oExportTable.AddressOfNames;
  size_t uNamesTableSize = oExportTable.NumberOfNames * sizeof(DWORD);
  DWORD* adwNamesTable = new DWORD[oExportTable.NumberOfNames]; // TODO: delete
  if (!oProcessInformation.readBytes(pNamesTableAddress, (PBYTE)adwNamesTable, uNamesTableSize, FALSE)) {
    _tprintf(_T("{\"sEventName\": \"warning\", \"sErrorMessage\": \"Cannot read Names Table at 0x%IX*0x%IX from module at 0x%IX in process %u.\"};\r\n"),
      pNamesTableAddress, uNamesTableSize, pBaseAddress, oProcessInformation.dwProcessId);
    return NULL;
  }
  LOG(_tprintf(_T("getRemoteProcAddress: Names Table @ 0x%IX*0x%IX OK\r\n"), pNamesTableAddress, uNamesTableSize));
  PBYTE pNameOrdinalsTableAddress = pBaseAddress + oExportTable.AddressOfNameOrdinals;
  size_t uNameOrdinalsTableSize = oExportTable.NumberOfNames * sizeof(WORD);
  WORD* awNameOrdinalsTable = new WORD[oExportTable.NumberOfNames]; // TODO: delete
  if (!oProcessInformation.readBytes(pNameOrdinalsTableAddress, (PBYTE)awNameOrdinalsTable, uNameOrdinalsTableSize, FALSE)) {
    _tprintf(_T("{\"sEventName\": \"warning\", \"sErrorMessage\": \"Cannot read Name Ordinals Table at 0x%IX*0x%IX from module at 0x%IX in process %u.\"};\r\n"),
      pNameOrdinalsTableAddress, uNameOrdinalsTableSize, pBaseAddress, oProcessInformation.dwProcessId);
    return NULL;
  }
  LOG(_tprintf(_T("getRemoteProcAddress: Name Ordinals Table @ 0x%IX*0x%IX OK\r\n"), pNameOrdinalsTableAddress, uNameOrdinalsTableSize));
  PBYTE pFunctionAddressesTableAddress = pBaseAddress + oExportTable.AddressOfFunctions;
  size_t uFunctionAddressesTableSize = oExportTable.NumberOfFunctions * sizeof(DWORD);
  DWORD* adwFunctionAddressesTable = new DWORD[oExportTable.NumberOfFunctions]; // TODO: delete
  if (!oProcessInformation.readBytes(pFunctionAddressesTableAddress, (PBYTE)adwFunctionAddressesTable, uFunctionAddressesTableSize, FALSE)) {
    _tprintf(_T("{\"sEventName\": \"warning\", \"sErrorMessage\": \"Cannot read Function Addresses Table at 0x%IX*0x%IX from module at 0x%IX in process %u.\"};\r\n"),
      pFunctionAddressesTableAddress, uFunctionAddressesTableSize, pBaseAddress, oProcessInformation.dwProcessId);
    return NULL;
  }
  LOG(_tprintf(_T("getRemoteProcAddress: Function Addresses Table @ 0x%IX*0x%IX OK\r\n"), pFunctionAddressesTableAddress, uFunctionAddressesTableSize));
  for (size_t i = 0; i < oExportTable.NumberOfNames; i++) {
    PBYTE pNameAddress = pBaseAddress + adwNamesTable[i];
    std::basic_string<TCHAR>* psName = oProcessInformation.readCharString((PCHAR)pNameAddress, FALSE);
    if (!psName) {
      _tprintf(_T("{\"sEventName\": \"warning\", \"sErrorMessage\": \"Cannot read Function Name #%d at 0x%IX from module at 0x%IX in process %u.\"};\r\n"),
        i, pNameAddress, pBaseAddress, oProcessInformation.dwProcessId);
      return NULL;
    }
    BOOL bFoundFunction = _tcscmp(sProcedureName, (*psName).c_str()) == 0;
    delete psName;
    if (bFoundFunction) {
      size_t iOrdinal = (size_t)awNameOrdinalsTable[i];
      PBYTE pFunctionAddress = pBaseAddress + adwFunctionAddressesTable[iOrdinal];
      if (pFunctionAddress >= pBaseAddress + oFirstDataDirectory.VirtualAddress && pFunctionAddress < pBaseAddress + oFirstDataDirectory.VirtualAddress + oFirstDataDirectory.Size) {
        // Function address is inside the export table, which means the function is forwarded to another DLL.
        std::basic_string<TCHAR>* psForwardName = oProcessInformation.readCharString((PCHAR)pFunctionAddress, FALSE);
        _tprintf(_T("{\"sEventName\": \"warning\", \"sErrorMessage\": \"Function %s in module at 0x%IX in process %u is forwarded to %s.\"};\r\n"),
            JSONStringify(sProcedureName).c_str(), pBaseAddress, oProcessInformation.dwProcessId, JSONStringify(*psForwardName).c_str());
        return NULL;
      }
      return pFunctionAddress;
    }
  }
  _tprintf(_T("{\"sEventName\": \"warning\", \"sErrorMessage\": \"Cannot find function %s in module at 0x%IX in process %u.\"};\r\n"),
      JSONStringify(sProcedureName).c_str(), pBaseAddress, oProcessInformation.dwProcessId);
  return NULL;
}