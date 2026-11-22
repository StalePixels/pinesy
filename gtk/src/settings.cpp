#include "settings.hpp"

#include <giomm/settingsschema.h>
#include <giomm/settingsschemasource.h>
#include <glibmm/miscutils.h>

#include <cstdio>
#include <filesystem>

namespace pinesy::gtk {

namespace {

// The schema folder of an installed copy, beside the program, as a
// source in front of GLib's own; GLib's own alone when the folder is
// not there, as in a run from the build folder.
Glib::RefPtr<Gio::SettingsSchemaSource> schema_source()
{
	auto source = Gio::SettingsSchemaSource::get_default();
	std::error_code error;
	auto program = std::filesystem::read_symlink("/proc/self/exe", error);
	if (error)
		return source;
	auto folder = (program.parent_path() / PINESY_INSTALLED_SCHEMAS).lexically_normal();
	if (!std::filesystem::exists(folder / "gschemas.compiled", error))
		return source;
	try
	{
		return Gio::SettingsSchemaSource::create(folder.string(), true, source);
	}
	catch (const Glib::Error &)
	{
		return source;
	}
}

} // namespace

Glib::RefPtr<Gio::Settings> open_settings()
{
	// Gio::Settings::create() aborts on a schema GLib cannot find, so
	// look it up first.
	auto source = schema_source();
	auto schema = source ? source->lookup(PINESY_APP_ID, true) : Glib::RefPtr<Gio::SettingsSchema>();
	if (!schema)
	{
		std::fprintf(stderr,
				"pinesy: the settings schema %s is not installed. To run from the build folder, "
				"set GSETTINGS_SCHEMA_DIR=<build folder>/schemas.\n",
				PINESY_APP_ID);
		return {};
	}
	return Glib::wrap(g_settings_new_full(schema->gobj(), nullptr, nullptr));
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
	result.update_checks = settings->get_boolean(key::update_checks);
	result.ignored_build = settings->get_string(key::ignored_build);
	return result;
}

std::string default_data_folder()
{
	return Glib::build_filename(Glib::get_user_data_dir(), "pinesy");
}

std::unique_ptr<core::Core> make_core(const Glib::RefPtr<Gio::Settings> &settings)
{
	auto result = std::make_unique<core::Core>(core::Platform{core::paths::resources_folder(PINESY_INSTALLED_RESOURCES, PINESY_RESOURCES_DIR), default_data_folder()});
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
