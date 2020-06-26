
#include <chrono>

#include "Util.h"

using namespace std;

bool GemCmdUtil::StringStartsWith(const string& src, const char* start)
{
	string prefix(start);
	if (prefix.length() > src.length()) return false;
	return equal(prefix.begin(), prefix.end(), src.begin());
}

bool GemCmdUtil::StringEndsWith(const string& src, const char* end)
{
	string suffix(end);
	if (suffix.length() > src.length()) return false;
	return equal(suffix.rbegin(), suffix.rend(), src.rbegin());
}


bool GemCmdUtil::StringEquals(const string& src, const char* other)
{
	return src.compare(other) == 0;
}

bool IsValidChar(char c)
{
	return c >= 32 && c <= 126;
}

string GemCmdUtil::SanitizeString(const string& src, char replacement /*=' '*/)
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

std::string SanitizeCString(char* src, int len, char replacement /*= ' '*/)
{
	for (int i = 0; i < len; i++)
	{
		if (!IsValidChar(src[i]))
			src[i] = replacement;
	}

	return string(src, len);
}

int GemCmdUtil::ParseNumericString(const std::string& src)
{
	if (StringEndsWith(src, "h")) 
		return stoi(src.substr(0, src.length() - 1), nullptr, 16);

	return stoi(src);
}

long GemCmdUtil::UnixTimestamp()
{
	using namespace std::chrono;
	return duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
}