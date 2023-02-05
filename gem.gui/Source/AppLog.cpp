#pragma once

#include <iostream>
#include <cstdio>
#include <windows.h>

#include "AppLog.h"
#include "GemConsole.h"

using namespace std;

AppLog::AppLog()
	: handle(nullptr)
{
}

AppLog::AppLog(const char* file)
{
	handle = (void*)CreateFileA(file, GENERIC_WRITE, FILE_SHARE_READ, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
}

AppLog::~AppLog()
{
	if (handle)
		CloseHandle(handle); // Return value ignored
}

void AppLog::LogMessage(bool error, bool console, const char* format, va_list args)
{
	static char message[256];
	static char rendered[256];
	static SYSTEMTIME time = { 0 };
	static DWORD bytes_written = 0;

	GetSystemTime(&time);
	vsnprintf(message, 256, format, args);
	int len = snprintf(rendered, 256, "[%02d:%02d:%02d.%03d] %s", time.wHour, time.wMinute, time.wSecond, time.wMilliseconds, message);
	
	if (console)
	{
		if (!error)
			GemConsole::Get().PrintLn(message);
		else
			GemConsole::Get().ErrorLn(message);
	}

	OutputDebugStringA(message);
	OutputDebugStringA("\n");

	static char nl = '\n';

	if (handle)
	{
		WriteFile(handle, (void*)rendered, len, &bytes_written, nullptr);
		WriteFile(handle, (void*)&nl, 1, &bytes_written, nullptr);
		FlushFileBuffers(handle);
	}
}



void AppLog::Error(const char* Message...)
{
	va_list args;
	va_start(args, Message);
	LogMessage(true, false, Message, args);
	va_end(args);
}

void AppLog::Console(const char* Message...)
{
	va_list args;
	va_start(args, Message);
	LogMessage(false, true, Message, args);
	va_end(args);
}

#define DEFN_LOG_FUNC(name) void AppLog::##name(const char* Message...)\
{ \
	va_list args; \
	va_start(args, Message); \
	LogMessage(false, false, Message, args); \
	va_end(args);  \
}
DEFN_LOG_FUNC(Verbose);
DEFN_LOG_FUNC(Debug);
DEFN_LOG_FUNC(Info);
DEFN_LOG_FUNC(Warn);
DEFN_LOG_FUNC(Fatal);
DEFN_LOG_FUNC(Violation);
DEFN_LOG_FUNC(Diag);
#undef DEFN_LOG_FUNC
