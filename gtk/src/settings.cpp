#include "settings.hpp"

#include <giomm/settingsschemasource.h>
#include <glibmm/miscutils.h>

#include <cstdio>
#include <filesystem>

namespace pinesy::gtk {

Glib::RefPtr<Gio::Settings> open_settings()
{
	// Gio::Settings::create() aborts on a schema GLib cannot find, so
	// look it up first.
	auto source = Gio::SettingsSchemaSource::get_default();
	if (!source || !source->lookup(PINESY_APP_ID, true))
	{
		std::fprintf(stderr,
				"pinesy: the settings schema %s is not installed. To run from the build folder, "
				"set GSETTINGS_SCHEMA_DIR=<build folder>/schemas.\n",
				PINESY_APP_ID);
		return {};
	}
	return Gio::Settings::create(PINESY_APP_ID);
}

core::Settings core_settings(const Glib::RefPtr<Gio::Settings> &settings)
{
	core::Settings result;
	for (const auto &folder : settings->get_string_array(key::rom_folders))
		result.rom_folders.push_back(folder);
	result.data_folder = settings->get_string(key::data_folder);
	result.debugger = settings->get_string(key::debugger) == "gdbStub"
			? core::Settings::Debugger::gdb_stub
			: core::Settings::Debugger::windows;
	result.gdb_port = settings->get_int(key::gdb_port);
	result.pi_gdb_port = settings->get_int(key::pi_gdb_port);
	result.start_paused = settings->get_boolean(key::start_paused);
	return result;
}

std::string default_data_folder()
{
	return Glib::build_filename(Glib::get_user_data_dir(), "pinesy");
}

std::unique_ptr<core::Core> make_core(const Glib::RefPtr<Gio::Settings> &settings)
{
	auto result = std::make_unique<core::Core>(core::Platform{PINESY_RESOURCES_DIR, default_data_folder()});
	result->set_settings(core_settings(settings));
	return result;
}

std::string executable_path()
{
	std::error_code error;
	auto path = std::filesystem::read_symlink("/proc/self/exe", error);
	return error ? std::string("pinesy") : path.string();
}

} // namespace pinesy::gtk
