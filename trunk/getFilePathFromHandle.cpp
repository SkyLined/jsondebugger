#include "stdafx.h"
#include "getFilePathFromHandle.h"

std::basic_string<TCHAR>* getFilePathFromHandle(HANDLE hFile) {
  // This will only work if compiled with unicode - how to fix?
  BYTE abFileInformation[sizeof(DWORD) + MAX_PATH * sizeof(WCHAR)];
  FILE_NAME_INFO* poFileNameInfo = (FILE_NAME_INFO*)abFileInformation;
  std::basic_string<TCHAR>* psFilePath = new std::basic_string<TCHAR>(_T(""));
  if (!GetFileInformationByHandleEx(hFile, FileNameInfo, &abFileInformation, sizeof(abFileInformation))) {
    _tprintf(_T("GetFileInformationByHandleEx() error: 0x%X\r\n"), GetLastError());
    return NULL;
  } else {
    (*psFilePath).assign(poFileNameInfo->FileName, poFileNameInfo->FileNameLength / sizeof(WCHAR));
  }
  return psFilePath;
}
