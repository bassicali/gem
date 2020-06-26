
#pragma once

#include <string>

namespace GemCmdUtil
{
	bool StringEquals(const std::string& src, const char* other);
	bool StringStartsWith(const std::string& src, const char* start);
	bool StringEndsWith(const std::string& src, const char* end);
	std::string SanitizeString(const std::string& src, char replacement = ' ');
	std::string SanitizeCString(const char* src, int len, char replacement = ' ');
	int ParseNumericString(const std::string& src);
	long UnixTimestamp();
}