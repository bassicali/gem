#pragma once

#include <cstdarg>

#include "Logging.h"

class AppLog : public IGemLog
{
	public:
		AppLog();
		AppLog(const char* file);
		~AppLog();

		virtual void Verbose(const char* Message...) override;
		virtual void Debug(const char* Message...) override;
		virtual void Info(const char* Message...) override;
		virtual void Warn(const char* Message...) override;
		virtual void Error(const char* Message...) override;
		virtual void Fatal(const char* Message...) override;
		virtual void Violation(const char* Message...) override;
		virtual void Diag(const char* Message...) override;
		virtual void Console(const char* Message...) override;

	private:
		void* handle;
		void LogMessage(bool error, bool console, const char* Message, va_list args);
};