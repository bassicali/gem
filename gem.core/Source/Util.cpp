
#define _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS 
#include <chrono>
#include <algorithm>
#include <locale>
#include <codecvt>
#include <string>
#include <cstdarg>

#include "Util.h"

using namespace std;


bool GemUtil::StringStartsWith(const string& src, const string& start)
{
	string prefix(start);
	if (prefix.length() > src.length()) return false;
	return equal(prefix.begin(), prefix.end(), src.begin());
}

bool GemUtil::StringStartsWithW(const wstring& src, const wstring& start)
{
	wstring prefix(start);
	if (prefix.length() > src.length()) return false;
	return equal(prefix.begin(), prefix.end(), src.begin());
}


bool GemUtil::StringEndsWith(const string& src, const string& end)
{
	string suffix(end);
	if (suffix.length() > src.length()) return false;
	return equal(suffix.rbegin(), suffix.rend(), src.rbegin());
}

bool GemUtil::StringEndsWithW(const std::wstring& src, const wstring& end)
{
	wstring suffix(end);
	if (suffix.length() > src.length()) return false;
	return equal(suffix.rbegin(), suffix.rend(), src.rbegin());
}

bool GemUtil::StringReplace(string& src, const string& find, const string& replace)
{
	int pos = src.find(find);
	if (pos == string::npos)
		return false;

	src.replace(pos, find.length(), replace);
	return true;
}

string GemUtil::StringUpper(string str)
{
	transform(str.begin(), str.end(), str.begin(), ::toupper);
	return str;
}

string GemUtil::StringLower(string str)
{
	transform(str.begin(), str.end(), str.begin(), ::tolower);
	return str;
}

bool GemUtil::StringEquals(const string& src, const std::string& other, bool ignoreCase)
{
	if (ignoreCase)
		return StringEquals(StringUpper(src), StringUpper(other), false);
	else
		return src.compare(other) == 0;
}

bool GemUtil::StringEqualsW(const wstring& src, const std::wstring& other)
{
	return src.compare(other) == 0;
}

bool IsValidChar(char c)
{
	return c >= 32 && c <= 126;
}

string GemUtil::SanitizeString(const string& src, char replacement /*=' '*/)
{
	string sanitized(src);

	for (int i = 0; i < sanitized.length(); i++)
	{
		char c = sanitized[i];
		if (!IsValidChar(c))
			sanitized[i] = replacement;
	}

	return sanitized;
}

string SanitizeCString(char* src, int len, char replacement /*= ' '*/)
{
	for (int i = 0; i < len; i++)
	{
		if (!IsValidChar(src[i]))
			src[i] = replacement;
	}

	return string(src, len);
}

int GemUtil::ParseNumericString(const string& src)
{
	if (StringEndsWith(src, "h")) 
		return stoi(src.substr(0, src.length() - 1), nullptr, 16);

	return stoi(src);
}

long GemUtil::UnixTimestamp()
{
	using namespace chrono;
	return duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
}

wstring GemUtil::StringAToW(const string& str)
{
	wstring_convert<codecvt_utf8_utf16<wchar_t>> converter;
	return converter.from_bytes(str);
}

string VStringFmt(const char* fmt, va_list args)
{
	static char message[256];
	memset(message, 0, sizeof(char) * 256);
	vsnprintf(message, 256, fmt, args);
	return string(message);
}

std::string GemUtil::StringFmt(const char* fmt ...)
{
	va_list args;
	va_start(args, fmt);
	string s = VStringFmt(fmt, args);
	va_end(args);
	return s;
}
