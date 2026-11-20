// license:BSD-3-Clause
// copyright-holders:D. Rimron-Soutter
//============================================================
//
//  core.cpp - MAME's paths, runs, the copied command line and
//  headless mode
//
//  Pinesy is not MAME: it reads no mame.ini and uses nothing
//  in ~/.mame. Every call into MAME that parses options starts
//  with the fixed options, so a later option on the same
//  command line (a -cfg_directory for one run, say) still
//  takes precedence.
//
//============================================================

#include "pinesy_core.hpp"

#include "pinesy.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <utility>

namespace pinesy::core {

namespace {

// Folders MAME searches for its own files, in the resources folder.
const std::pair<const char *, const char *> search_paths[] = {
	{ "hashpath", "hash" }, { "samplepath", "samples" }, { "artpath", "artwork" },
	{ "ctrlrpath", "ctrlr" }, { "fontpath", "fonts" }, { "cheatpath", "cheat" },
	{ "crosshairpath", "crosshair" }, { "pluginspath", "plugins" },
	{ "languagepath", "language" }, { "swpath", "software" }, { "bgfx_path", "bgfx" },
};

// Folders MAME writes, in the data folder.
const std::pair<const char *, const char *> data_folders[] = {
	{ "inipath", "ini" }, { "homepath", "home" },
	{ "cfg_directory", "cfg" }, { "nvram_directory", "nvram" },
	{ "input_directory", "inp" }, { "state_directory", "sta" },
	{ "snapshot_directory", "snap" }, { "diff_directory", "diff" },
	{ "comment_directory", "comments" }, { "share_directory", "share" },
};

// A folder and a name in it, without a doubled "/".
std::string join(std::string folder, const std::string &name)
{
	while (folder.size() > 1 && folder.back() == '/') folder.pop_back();
	if (folder.empty()) return name;
	if (folder == "/") return folder + name;
	return folder + "/" + name;
}

struct CStrings
{
	std::vector<const char *> argv;
	explicit CStrings(const std::vector<std::string> &strings)
	{
		for (const std::string &s : strings) argv.push_back(s.c_str());
	}
	int argc() const { return int(argv.size()); }
	const char *const *data() const { return argv.data(); }
};

void receive_text(void *context, const char *text)
{
	if (context && text) *static_cast<std::string *>(context) = text;
}

} // anonymous namespace

//------------------------------------------------------------
//  Paths
//------------------------------------------------------------

namespace paths {

std::string user_roms(const std::string &data_folder)
{
	return join(data_folder, "roms");
}

bool is_usable_rom_folder(std::string_view path)
{
	return path.find(';') == std::string_view::npos;
}

} // namespace paths

Core::Core(Platform platform) : m_platform(std::move(platform)) {}

std::string Core::data_folder() const
{
	return m_settings.data_folder.empty() ? m_platform.default_data_folder : m_settings.data_folder;
}

std::vector<std::string> Core::rom_folders() const
{
	std::vector<std::string> folders{ join(m_platform.resources_folder, "roms"), paths::user_roms(data_folder()) };
	folders.insert(folders.end(), m_settings.rom_folders.begin(), m_settings.rom_folders.end());
	std::erase_if(folders, [](const std::string &f) { return !paths::is_usable_rom_folder(f); });
	return folders;
}

std::vector<std::string> Core::fixed_options() const
{
	std::string rompath;
	for (const std::string &folder : rom_folders())
		rompath += (rompath.empty() ? "" : ";") + folder;
	std::vector<std::string> result{ "-noreadconfig", "-rompath", rompath };
	for (const auto &[option, folder] : search_paths)
	{
		result.push_back(std::string("-") + option);
		result.push_back(join(m_platform.resources_folder, folder));
	}
	std::string data = data_folder();
	for (const auto &[option, folder] : data_folders)
	{
		result.push_back(std::string("-") + option);
		result.push_back(join(data, folder));
	}
	result.push_back("-skip_gameinfo");
	return result;
}

void Core::create_folders() const
{
	std::string data = data_folder();
	std::vector<std::string> folders{ paths::user_roms(data) };
	for (const auto &[option, folder] : data_folders) folders.push_back(join(data, folder));
	for (const std::string &folder : folders)
	{
		std::error_code error;
		std::filesystem::create_directories(folder, error);   // a folder that cannot be made is left to MAME
	}
}

//------------------------------------------------------------
//  NVRAM
//------------------------------------------------------------

std::string Core::nvram_folder(const std::vector<std::string> &arguments) const
{
	// a later option takes precedence, as in MAME
	std::optional<std::string> folder;
	for (const Arguments::Option &option : Arguments::parse(arguments).options)
		if (option.name == "nvram_directory" && option.has_value) folder = option.value;
	return folder ? *folder : join(data_folder(), "nvram");
}

bool Core::clear_nvram(const std::vector<std::string> &arguments) const
{
	const std::string driver = Arguments::parse(arguments).driver;
	if (driver.empty()) return false;

	// MAME names a machine's NVRAM after its driver, with "_<n>" for a
	// ROM that is not the driver's default (running_machine::nvram_filename)
	auto is_driver_nvram = [&driver](const std::string &name) {
		if (name == driver) return true;
		if (name.size() <= driver.size() + 1 || name.compare(0, driver.size(), driver) != 0 || name[driver.size()] != '_')
			return false;
		return std::all_of(name.begin() + driver.size() + 1, name.end(), [](char c) { return c >= '0' && c <= '9'; });
	};

	namespace fs = std::filesystem;
	std::error_code error;
	fs::directory_iterator entries(nvram_folder(arguments), error);
	if (error) return error == std::errc::no_such_file_or_directory;   // no folder: nothing saved
	bool cleared = true;
	for (const fs::directory_entry &entry : entries)
	{
		if (!is_driver_nvram(entry.path().filename().string())) continue;
		std::error_code removed;
		fs::remove_all(entry.path(), removed);
		if (removed) cleared = false;
	}
	return cleared;
}

//------------------------------------------------------------
//  The engine
//------------------------------------------------------------

MachineDescription Core::describe(const std::vector<std::string> &arguments) const
{
	std::vector<std::string> command = fixed_options();
	command.insert(command.end(), arguments.begin(), arguments.end());
	CStrings c(command);
	std::string text;
	pinesy_describe(c.argc(), c.data(), receive_text, &text);
	return decode_description(text);
}

Describer Core::describer() const
{
	return [this](const std::vector<std::string> &arguments) { return describe(arguments); };
}

const Catalogue &Core::catalogue()
{
	if (!m_catalogue)
	{
		std::string text;
		pinesy_list_machines(receive_text, &text);
		m_catalogue.emplace(decode_driver_list(text));
	}
	return *m_catalogue;
}

//------------------------------------------------------------
//  Runs
//------------------------------------------------------------

std::vector<std::string> Switches::mame_arguments() const
{
	if (paused_pi) return { "-pi_gdb_wait" };
	return {};
}

std::string pi_sd_card(const std::vector<std::string> &arguments)
{
	std::optional<std::string> image = Arguments::parse(arguments).get("pi");
	return image && !image->empty() ? "pi" : "";
}

std::vector<std::string> debug_arguments(const Settings &settings, const std::vector<std::string> &arguments, bool debug)
{
	if (!debug) return {};
	std::vector<std::string> result{ "-debug" };
	if (settings.debugger == Settings::Debugger::gdb_stub)
	{
		result.insert(result.end(), { "-debugger", "gdbstub", "-debugger_port", std::to_string(settings.gdb_port) });
	}
	if (!pi_sd_card(arguments).empty())
		result.insert(result.end(), { "-pi_gdb_port", std::to_string(settings.pi_gdb_port) });
	return result;
}

Switches run_switches(const Settings &settings, const std::vector<std::string> &arguments, bool debug, bool pi_paused)
{
	Switches switches;
	if (!debug) return switches;
	switches.paused = settings.start_paused;
	switches.paused_pi = pi_paused && !pi_sd_card(arguments).empty();
	return switches;
}

std::vector<std::string> mouse_arguments(bool takes_mouse, bool mouse)
{
	if (!takes_mouse) return {};
	if (mouse) return { "-mouse" };
	// MAME's -mouse_device is "mouse" by default, which gives a machine
	// with mouse inputs the mouse also without -mouse
	return { "-mouse_device", "none" };
}

std::vector<std::string> Core::run_command_line(const std::vector<std::string> &arguments, bool debug, bool takes_mouse,
		bool mouse, bool pi_paused) const
{
	std::vector<std::string> command = fixed_options();
	command.insert(command.end(), arguments.begin(), arguments.end());
	auto pointer = mouse_arguments(takes_mouse, mouse);
	command.insert(command.end(), pointer.begin(), pointer.end());
	auto extra = debug_arguments(m_settings, arguments, debug);
	command.insert(command.end(), extra.begin(), extra.end());
	auto switched = run_switches(m_settings, arguments, debug, pi_paused).mame_arguments();
	command.insert(command.end(), switched.begin(), switched.end());
	return command;
}

int Core::run(const std::vector<std::string> &arguments, bool debug, bool takes_mouse, bool mouse, bool pi_paused,
		const pinesy_host *host) const
{
	create_folders();
	std::vector<std::string> command = run_command_line(arguments, debug, takes_mouse, mouse, pi_paused);
	CStrings c(command);
	pinesy_set_start_paused(run_switches(m_settings, arguments, debug, pi_paused).paused ? 1 : 0);
	return pinesy_run(c.argc(), c.data(), host);
}

std::string shell_quoted(std::string_view text)
{
	// Letters, digits and "-_./:=,+@%" need no quotes. Anything else is
	// quoted in single quotes, with each "'" written as '\''.
	auto plain = [](char c) {
		return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')
				|| std::string_view("-_./:=,+@%").find(c) != std::string_view::npos;
	};
	bool all_plain = !text.empty();
	for (char c : text)
		if (!plain(c)) { all_plain = false; break; }
	if (all_plain) return std::string(text);
	std::string out = "'";
	for (char c : text)
	{
		if (c == '\'') out += "'\\''";
		else out += c;
	}
	return out + "'";
}

std::string Core::copy_command_line(const std::string &executable, const std::vector<std::string> &arguments, bool debug,
		bool takes_mouse, bool mouse) const
{
	std::vector<std::string> words{ executable, "--" };
	words.insert(words.end(), arguments.begin(), arguments.end());
	auto pointer = mouse_arguments(takes_mouse, mouse);
	words.insert(words.end(), pointer.begin(), pointer.end());
	auto extra = debug_arguments(m_settings, arguments, debug);
	words.insert(words.end(), extra.begin(), extra.end());
	std::string line;
	for (size_t i = 0; i < words.size(); i++)
		line += (i ? " " : "") + shell_quoted(words[i]);
	return line;
}

//------------------------------------------------------------
//  Headless mode
//------------------------------------------------------------

namespace headless {

bool video_none(const std::vector<std::string> &arguments)
{
	std::string video;
	for (size_t i = 0; i + 1 < arguments.size(); i++)
		if (arguments[i] == "-video") video = arguments[i + 1];
	return video == "none";
}

bool opens_debugger_windows(const std::vector<std::string> &arguments)
{
	bool debug = false;
	for (const std::string &a : arguments)
		if (a == "-debug" || a == "-d") debug = true;
	if (!debug) return false;
	std::string debugger = "auto";
	for (size_t i = 0; i + 1 < arguments.size(); i++)
		if (arguments[i] == "-debugger") debugger = arguments[i + 1];
	return debugger != "none" && debugger != "gdbstub";
}

CommandLine split(const std::vector<std::string> &arguments)
{
	CommandLine result;
	auto separator = std::find(arguments.begin(), arguments.end(), "--");
	if (separator == arguments.end())
	{
		result.mame = arguments;
		return result;
	}
	for (auto word = arguments.begin(); word != separator; ++word)
	{
		if (*word == "--paused")
			result.switches.paused = true;
		else if (*word == "--paused-pi")
			result.switches.paused_pi = true;
		else if (result.error.empty())
			result.error = "unknown switch before \"--\": " + *word + " (Pinesy's switches are --paused and --paused-pi)";
	}
	result.mame.assign(separator + 1, arguments.end());
	return result;
}

} // namespace headless

int Core::headless(const std::vector<std::string> &arguments, const pinesy_host *host) const
{
	const headless::CommandLine line = headless::split(arguments);
	if (!line.error.empty())
	{
		std::fprintf(stderr, "Pinesy: %s\n", line.error.c_str());
		return 6;   // MAME's EMU_ERR_INVALID_CONFIG, as for an unknown option
	}
	create_folders();
	std::vector<std::string> command = fixed_options();
	command.insert(command.end(), line.mame.begin(), line.mame.end());
	auto switched = line.switches.mame_arguments();
	command.insert(command.end(), switched.begin(), switched.end());
	CStrings c(command);
	pinesy_set_start_paused(line.switches.paused ? 1 : 0);
	return pinesy_run(c.argc(), c.data(), host);
}

} // namespace pinesy::core
