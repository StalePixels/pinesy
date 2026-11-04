// Entry point. With arguments, pinesy runs MAME headless with that
// command line and exits with MAME's exit code. Without arguments it
// starts the app. The one exception is --gapplication-service, which
// D-Bus activation passes to start the app as a service.

#include "application.hpp"
#include "machine_runner.hpp"
#include "settings.hpp"

#include <giomm/init.h>
#include <glibmm/init.h>

#include <string>
#include <vector>

int main(int argc, char *argv[])
{
	using namespace pinesy::gtk;

	std::vector<std::string> arguments(argv + 1, argv + argc);
	bool service = arguments.size() == 1 && arguments[0] == "--gapplication-service";

	// glibmm would set the C++ global locale to the user's. MAME formats
	// numbers with the global locale, and a locale that groups digits
	// turns "-debugger_port 23999" into the socket address
	// "localhost:23,999". The engine needs the classic locale.
	Glib::set_init_to_users_preferred_locale(false);
	Gio::init();
	auto settings = open_settings();
	if (!settings)
		return 1;

	if (!arguments.empty() && !service)
	{
		auto core = make_core(settings);
		// MAME needs a window unless the last -video is none
		if (pinesy::core::headless::video_none(arguments))
			return core->headless(arguments, nullptr);
		return run_headless_in_window(*core, arguments);
	}

	auto app = Application::create(settings);
	return app->run(service ? argc : 1, argv);
}
