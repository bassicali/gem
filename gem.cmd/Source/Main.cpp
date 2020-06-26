
#include <iostream>

#include "BoostLogger.h"
#include "GemApp.h"

using namespace std;

GemApp app;
once_flag shutdown_flag;

void _AppShutdown()
{
	app.Shutdown();
}

void _AtExitHandler()
{
	call_once(shutdown_flag, _AppShutdown);
}

BOOL WINAPI _ConsoleCtrlHandler(DWORD type)
{
	if (type == CTRL_CLOSE_EVENT)
	{
		call_once(shutdown_flag, _AppShutdown);
		return TRUE;
	}

	return FALSE;
}

int main(int argc, char* argv[])
{
	BoostLogger::InitializeLogging();

	vector<string> arguments;
	for (int i = 0; i < argc; i++)
	{
		arguments.push_back(string(argv[i]));
	}

	if (SetConsoleCtrlHandler(_ConsoleCtrlHandler, TRUE) == 0)
	{
		LOG_WARN("Couldn't register console ctrl handler");
	}

	if (app.Init(arguments))
	{
		atexit(_AtExitHandler);

		try
		{
			thread window_thread(&GemApp::WindowLoop, &app);
			app.ConsoleLoop();
			window_thread.join();
		}
		catch (exception& ex)
		{
			LOG_ERROR("Unhandled error: %s", ex.what());
		}
		catch (...)
		{
			LOG_ERROR("Unhandled error");
		}

		return 0;
	}

	return -1;
}