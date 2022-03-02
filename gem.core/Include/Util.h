
#pragma once

#include <string>

namespace GemUtil
{
	bool StringEquals(const std::string& src, const std::string& other, bool ignoreCase = false);
	bool StringEqualsW(const std::wstring& src, const std::wstring& other);

	bool StringStartsWith(const std::string& src, const std::string& start);
	bool StringStartsWithW(const std::wstring& src, const std::wstring& start);
	
	bool StringEndsWith(const std::string& src, const std::string& end);
	bool StringEndsWithW(const std::wstring& src, const std::wstring& end);
	
	bool StringReplace(std::string& src, const std::string& find, const std::string& replace);
	std::string StringFmt(const char* format...);
	std::string StringUpper(std::string str);
	std::string StringLower(std::string str);
	std::string SanitizeString(const std::string& src, char replacement = ' ');
	std::string SanitizeCString(const char* src, int len, char replacement = ' ');
	std::wstring StringAToW(const std::string& str);
	int ParseNumericString(const std::string& src);
	long UnixTimestamp();
}