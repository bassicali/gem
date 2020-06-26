#pragma once

#include <iostream>
#include <cstdio>
#include <windows.h>

#include "BoostLogger.h"
#include "GemConsole.h"

boost::log::sources::logger_mt BoostLogger::_log;

namespace logging = boost::log;
namespace keywords = boost::log::keywords;

using namespace std;

void BoostLogger::InitializeLogging()
{
	logging::add_file_log
	(
		keywords::file_name = "Gem-log.txt",
		keywords::format = "[%TimeStamp%] %Message%"
	);

	IGemLog::SetLogger(new BoostLogger());
}

void BoostLogger::LogMessage(bool to_stdout, const char* Message, va_list args)
{
	static thread_local char msg_buffer[256];
	static thread_local char whole_buffer[270];
	static thread_local SYSTEMTIME time = {0};

	logging::record rec = _log.open_record();

	if (rec)
	{
		vsnprintf(msg_buffer, 256, Message, args);

		if (to_stdout)
			GemConsole::VPrintLn(Message, args);

		GetSystemTime(&time);
		snprintf(whole_buffer, 270, "[%02d:%02d:%02d.%03d] %s\r\n", time.wHour, time.wMinute, time.wSecond, time.wMilliseconds, msg_buffer);
		logging::record_ostream strm(rec);
		strm << whole_buffer;
		strm.flush();
		_log.push_record(move(rec));

		OutputDebugStringA(whole_buffer);
	}
}

#define DEFN_LOG_FUNC(name) void BoostLogger::##name(const char* Message...)\
{ \
	va_list args; \
	va_start(args, Message); \
	LogMessage(false, Message, args); \
	va_end(args);  \
}

#define DEFN_LOG_FUNC_CONS(name) void BoostLogger::##name(const char* Message...)\
{ \
	va_list args; \
	va_start(args, Message); \
	LogMessage(true, Message, args); \
	va_end(args);  \
}

DEFN_LOG_FUNC(Verbose);
DEFN_LOG_FUNC(Debug);
DEFN_LOG_FUNC(Info);
DEFN_LOG_FUNC(Warn);
DEFN_LOG_FUNC(Error);
DEFN_LOG_FUNC(Fatal);
DEFN_LOG_FUNC(Violation);
DEFN_LOG_FUNC(Diag);
DEFN_LOG_FUNC_CONS(Console);

#undef DEFN_LOG_FUNC