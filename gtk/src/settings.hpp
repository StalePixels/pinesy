// The app's settings, stored with GSettings, and the core they feed.

#ifndef PINESY_GTK_SETTINGS_HPP
#define PINESY_GTK_SETTINGS_HPP

#include "pinesy_core.hpp"

#include <giomm/settings.h>

#include <memory>
#include <string>

namespace pinesy::gtk {

// The settings keys, as the schema in data/ names them.
namespace key {
inline constexpr const char *rom_folders = "rom-folders";
inline constexpr const char *data_folder = "data-folder";
inline constexpr const char *debugger = "debugger";
inline constexpr const char *gdb_port = "gdb-port";
inline constexpr const char *pi_gdb_port = "pi-gdb-port";
inline constexpr const char *start_paused = "start-paused";
} // namespace key

// The settings, or nullptr when the schema is not installed. Then it
// prints how to point GLib at the schema. An installed copy reads the
// schema from beside itself (PINESY_INSTALLED_SCHEMAS, relative to the
// program's folder) before GLib's own folders.
Glib::RefPtr<Gio::Settings> open_settings();

// The core's view of the settings.
core::Settings core_settings(const Glib::RefPtr<Gio::Settings> &settings);

// $XDG_DATA_HOME/pinesy, by default ~/.local/share/pinesy.
std::string default_data_folder();

// A core with the resources folder, the default data folder and the
// settings. The resources folder is an installed copy's own
// (PINESY_INSTALLED_RESOURCES, relative to the program's folder) when
// it exists, otherwise this build's (PINESY_RESOURCES_DIR).
std::unique_ptr<core::Core> make_core(const Glib::RefPtr<Gio::Settings> &settings);

// The path of this executable, for the copied command line.
std::string executable_path();

} // namespace pinesy::gtk

#endif
