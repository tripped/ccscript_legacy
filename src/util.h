#pragma once

#include <string>
#include <codecvt>

#ifdef _WIN32
inline std::wstring ConvertToNativeString(const std::string str) {
	std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
	std::wstring wstr = converter.from_bytes(str);
	return wstr;
}
#else
inline std::string ConvertToNativeString(const std::string str) {
	return str;
}
#endif