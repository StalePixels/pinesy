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

std::vector<std::string> debug_arguments(const Settings &settings, bool debug)
{
	if (!debug) return {};
	std::vector<std::string> result{ "-debug" };
	if (settings.debugger == Settings::Debugger::gdb_stub)
	{
		result.insert(result.end(), { "-debugger", "gdbstub", "-debugger_port", std::to_string(settings.gdb_port) });
	}
	return result;
}

std::vector<std::string> mouse_arguments(bool mouse)
{
	if (!mouse) return {};
	return { "-mouse" };
}

std::vector<std::string> Core::run_command_line(const std::vector<std::string> &arguments, bool debug, bool mouse) const
{
	std::vector<std::string> command = fixed_options();
	command.insert(command.end(), arguments.begin(), arguments.end());
	auto pointer = mouse_arguments(mouse);
	command.insert(command.end(), pointer.begin(), pointer.end());
	auto extra = debug_arguments(m_settings, debug);
	command.insert(command.end(), extra.begin(), extra.end());
	return command;
}

int Core::run(const std::vector<std::string> &arguments, bool debug, bool mouse, const pinesy_host *host) const
{
	create_folders();
	std::vector<std::string> command = run_command_line(arguments, debug, mouse);
	CStrings c(command);
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
		bool mouse) const
{
	std::vector<std::string> words{ executable, "--" };
	words.insert(words.end(), arguments.begin(), arguments.end());
	auto pointer = mouse_arguments(mouse);
	words.insert(words.end(), pointer.begin(), pointer.end());
	auto extra = debug_arguments(m_settings, debug);
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

} // namespace headless

int Core::headless(std::vector<std::string> arguments, const pinesy_host *host) const
{
	if (!arguments.empty() && arguments.front() == "--") arguments.erase(arguments.begin());
	create_folders();
	std::vector<std::string> command = fixed_options();
	command.insert(command.end(), arguments.begin(), arguments.end());
	CStrings c(command);
	return pinesy_run(c.argc(), c.data(), host);
}

} // namespace pinesy::core
