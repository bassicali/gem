#pragma once

#include <cstdarg>

#include <boost/log/sources/logger.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/sources/record_ostream.hpp>

#include "Logging.h"

class BoostLogger : public IGemLog
{
	public:
		virtual void Verbose(const char* Message...) override;
		virtual void Debug(const char* Message...) override;
		virtual void Info(const char* Message...) override;
		virtual void Warn(const char* Message...) override;
		virtual void Error(const char* Message...) override;
		virtual void Fatal(const char* Message...) override;
		virtual void Violation(const char* Message...) override;
		virtual void Diag(const char* Message...) override;
		virtual void Console(const char* Message...) override;

		static void InitializeLogging();

	private:
		static boost::log::sources::logger_mt _log;
		void LogMessage(bool to_cout, const char* Message, va_list args);
};