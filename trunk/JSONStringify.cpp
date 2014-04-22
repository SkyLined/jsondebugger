#include "stdafx.h"
#include "JSONStringify.h"

std::map<TCHAR, std::basic_string<TCHAR>> dJSON_sEncoding_by_cChar;

std::basic_string<TCHAR> JSONStringify(std::basic_string<TCHAR> sInput) {
  std::basic_string<TCHAR> sOutput = JSONStringifyNoQuotes(sInput);
  sOutput.insert(0, _T("\""));
  sOutput.append(_T("\""));
  return sOutput;
}

std::basic_string<TCHAR> JSONStringify(std::basic_string<TCHAR>* psInput) {
  if (psInput == NULL) {
    return _T("null");
  } else {
    return JSONStringify(*psInput);
  }
}

std::basic_string<TCHAR> JSONStringify(PTSTR psInput) {
  if (psInput == NULL) {
    return _T("null");
  } else {
    return JSONStringify(std::basic_string<TCHAR>(psInput));
  }
}

std::basic_string<TCHAR> JSONStringify(PBYTE pInput, size_t uSize) {
  std::basic_string<TCHAR> sOutput;
  while (uSize--) sOutput += (TCHAR)(*pInput++);
  return JSONStringify(sOutput);
}

std::basic_string<TCHAR> JSONStringifyNoQuotes(std::basic_string<TCHAR> sInput) {
  if (dJSON_sEncoding_by_cChar.empty()) {
    dJSON_sEncoding_by_cChar[_T('\"')] = _T("\\\"");
    dJSON_sEncoding_by_cChar[_T('\\')] = _T("\\\\");
    dJSON_sEncoding_by_cChar[_T('/')] = _T("\\/");
    dJSON_sEncoding_by_cChar[_T('\b')] = _T("\\b");
    dJSON_sEncoding_by_cChar[_T('\f')] = _T("\\f");
    dJSON_sEncoding_by_cChar[_T('\n')] = _T("\\n");
    dJSON_sEncoding_by_cChar[_T('\r')] = _T("\\r");
    dJSON_sEncoding_by_cChar[_T('\t')] = _T("\\t");
  }
  std::basic_string<TCHAR> sOutput = _T("");
  TCHAR sHexEncodeBuffer[7];
  for (std::basic_string<TCHAR>::iterator iCharIterator = sInput.begin(); iCharIterator != sInput.end(); iCharIterator++) {
    TCHAR cChar = *iCharIterator;
    std::map<TCHAR, std::basic_string<TCHAR>>::iterator oFound = dJSON_sEncoding_by_cChar.find(cChar);
    if (oFound != dJSON_sEncoding_by_cChar.end()) {
      sOutput += oFound->second;
    } else if (cChar < 0x20 || cChar > 0xFF) {
      _stprintf_s(sHexEncodeBuffer, 7, _T("\\u%04X"), cChar);
      sOutput += sHexEncodeBuffer;
    } else {
      sOutput += cChar;
    }
  }
  return sOutput;
} 

std::basic_string<TCHAR> JSONStringifyNoQuotes(PBYTE pInput, size_t uSize) {
  std::basic_string<TCHAR> sOutput;
  while (uSize--) sOutput += (TCHAR)(*pInput++);
  return JSONStringifyNoQuotes(sOutput);
}