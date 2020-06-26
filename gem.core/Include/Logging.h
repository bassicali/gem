#pragma once

#include <cstdarg>

#define GLL_VERBOSE		0
#define GLL_DEBUG		1
#define GLL_INFO		2
#define GLL_WARN		3
#define GLL_ERROR		4
#define GLL_FATAL		5
#define GLL_OFF			5

#define GEM_LOGGING_LEVEL				GLL_ERROR
#define GEM_LOGGING_ENABLE_VIOLATIONS	0
#define GEM_LOGGING_ENABLE_CONSOLE		1
#define GEM_LOGGING_ENABLE_DIAGNOSTIC	0

class IGemLog
{
	public:
		virtual void Verbose(const char* Message...) = 0;
		virtual void Debug(const char* Message...) = 0;
		virtual void Info(const char* Message...) = 0;
		virtual void Warn(const char* Message...) = 0;
		virtual void Error(const char* Message...) = 0;
		virtual void Fatal(const char* Message...) = 0;

		virtual void Violation(const char* Message...) = 0;
		virtual void Diag(const char* Message...) = 0;
		virtual void Console(const char* Message...) = 0;

		static inline IGemLog& GetLogger() { return *_Logger; }
		static void SetLogger(IGemLog* Logger) { _Logger = Logger; }

	private:
		static IGemLog* _Logger;
};

class NoLog : public IGemLog
{
	virtual void Verbose(const char* Message...) override {}
	virtual void Debug(const char* Message...) override {}
	virtual void Info(const char* Message...) override {}
	virtual void Warn(const char* Message...) override {}
	virtual void Error(const char* Message...) override {}
	virtual void Fatal(const char* Message...) override {}
	virtual void Violation(const char* Message...) override {}
	virtual void Diag(const char* Message...) override {}
	virtual void Console(const char* Message...) override {}
};

#if GEM_LOGGING_LEVEL <= GLL_VERBOSE
#define LOG_VERBOSE(FMT,...) IGemLog::GetLogger().Verbose("[VERBOSE] : " FMT,__VA_ARGS__)
#else
#define LOG_VERBOSE(FMT,...)
#endif

#if GEM_LOGGING_LEVEL <= GLL_DEBUG
#define LOG_DEBUG(FMT,...) IGemLog::GetLogger().Debug("[DEBUG] " FMT,__VA_ARGS__)
#else
#define LOG_DEBUG(FMT,...)
#endif

#if GEM_LOGGING_LEVEL <= GLL_INFO
#define LOG_INFO(FMT,...) IGemLog::GetLogger().Info("[INFO] " FMT,__VA_ARGS__)
#else
#define LOG_INFO(FMT,...)
#endif

#if GEM_LOGGING_LEVEL <= GLL_WARN
#define LOG_WARN(FMT,...) IGemLog::GetLogger().Warn("[WARN] " FMT,__VA_ARGS__)
#else
#define LOG_WARN(FMT,...)
#endif

#if GEM_LOGGING_LEVEL <= GLL_ERROR
#define LOG_ERROR(FMT,...) IGemLog::GetLogger().Error("[ERROR] " FMT,__VA_ARGS__)
#else
#define LOG_ERROR(FMT,...)
#endif

#if GEM_LOGGING_LEVEL <= GLL_FATAL
#define LOG_FATAL(FMT,...) IGemLog::GetLogger().Fatal("[FATAL] " FMT,__VA_ARGS__)
#else
#define LOG_FATAL(FMT,...)
#endif

#if GEM_LOGGING_ENABLE_VIOLATIONS
#define LOG_VIOLATION(FMT,...) IGemLog::GetLogger().Violation("[VIOLN] " FMT,__VA_ARGS__)
#else
#define LOG_VIOLATION(FMT,...)
#endif

#if GEM_LOGGING_ENABLE_DIAGNOSTIC
#define LOG_DIAG(FMT,...) IGemLog::GetLogger().Diag("[DIAG] " FMT,__VA_ARGS__)
#else
#define LOG_DIAG(FMT,...)
#endif

#if GEM_LOGGING_ENABLE_CONSOLE
#define LOG_CONS(FMT,...) IGemLog::GetLogger().Console(FMT,__VA_ARGS__)
#else
#define LOG_CONS(FMT,...)
#endif