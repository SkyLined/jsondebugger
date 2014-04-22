#include "stdafx.h"
#include <map>
#include <string>

std::basic_string<TCHAR> JSONStringify(std::basic_string<TCHAR> sInput);
std::basic_string<TCHAR> JSONStringify(std::basic_string<TCHAR>* psInput);
std::basic_string<TCHAR> JSONStringify(PTSTR sInput);
std::basic_string<TCHAR> JSONStringify(PBYTE pInput, size_t uSize);

std::basic_string<TCHAR> JSONStringifyNoQuotes(std::basic_string<TCHAR> sInput);
std::basic_string<TCHAR> JSONStringifyNoQuotes(PBYTE pInput, size_t uSize);
