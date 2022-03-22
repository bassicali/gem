
#include <iostream>
#include <memory>
#include <cstdlib>

#include "GemApp.h"
#include "AppLog.h"

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

extern "C" int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	auto log = make_shared<AppLog>("Gem-log.txt");
	IGemLog::SetLogger(log.get());

	vector<string> arguments;
	for (int i = 0; i < __argc; i++)
	{
		arguments.push_back(string(__argv[i]));
	}

	if (!app.Init(arguments))
		return -1;

	atexit(_AtExitHandler);

	int exit_code = 0;

	try
	{
		app.WindowLoop();
	}
	catch (exception& ex)
	{
		LOG_ERROR("Unhandled error: %s", ex.what());
		exit_code = -1;
	}
	catch (...)
	{
		LOG_ERROR("Unhandled error");
		exit_code = -1;
	}

	return exit_code;
}