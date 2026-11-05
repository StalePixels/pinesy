// license:BSD-3-Clause
// copyright-holders:D. Rimron-Soutter
//============================================================
//
//  c_api.cpp - the C interface, pinesy_core.h, over the C++
//  core
//
//============================================================

#include "pinesy_core.h"
#include "pinesy_core.hpp"
#include "internal.hpp"

#include <cstdlib>
#include <cstring>

using namespace pinesy::core;

struct pinesy_core
{
	Core core;
};

namespace {

char *give(const std::string &text)
{
	char *out = static_cast<char *>(std::malloc(text.size() + 1));
	if (out) std::memcpy(out, text.c_str(), text.size() + 1);
	return out;
}

char *give(const json::Value &value)
{
	return give(json::write(value));
}

std::optional<std::vector<std::string>> read_arguments(const char *text)
{
	if (!text) return std::nullopt;
	try
	{
		return encode::to_strings(json::parse(text));
	}
	catch (const json::Error &)
	{
		return std::nullopt;
	}
}

std::vector<std::string> read_argv(int argc, const char *const argv[])
{
	std::vector<std::string> result;
	for (int i = 0; argv && i < argc; i++)
		result.emplace_back(argv[i] ? argv[i] : "");
	return result;
}

const char *debugger_name(Settings::Debugger d)
{
	return d == Settings::Debugger::gdb_stub ? "gdbStub" : "windows";
}

const Slot *find_slot(const MachineDescription &description, const char *tag)
{
	if (!description.slots) return nullptr;
	for (const Slot &s : *description.slots)
		if (s.tag == tag) return &s;
	return nullptr;
}

} // anonymous namespace

extern "C" {

pinesy_core *pinesy_core_create(const char *resources_folder, const char *default_data_folder)
{
	if (!resources_folder || !default_data_folder) return nullptr;
	return new pinesy_core{ Core(Platform{ resources_folder, default_data_folder }) };
}

void pinesy_core_destroy(pinesy_core *core)
{
	delete core;
}

void pinesy_core_free(char *text)
{
	std::free(text);
}

//------------------------------------------------------------
//  Settings
//------------------------------------------------------------

int pinesy_core_set_settings(pinesy_core *core, const char *settings_json)
{
	if (!core || !settings_json) return -1;
	json::Value root;
	try
	{
		root = json::parse(settings_json);
		root.object();
	}
	catch (const json::Error &)
	{
		return -1;
	}

	Settings settings;
	if (const json::Value *v = root.find("romFolders"))
		try { settings.rom_folders = encode::to_strings(*v); } catch (const json::Error &) {}
	if (const json::Value *v = root.find("dataFolder"))
		try { settings.data_folder = v->string(); } catch (const json::Error &) {}
	if (const json::Value *v = root.find("debugger"))
		try { if (v->string() == "gdbStub") settings.debugger = Settings::Debugger::gdb_stub; } catch (const json::Error &) {}
	if (const json::Value *v = root.find("gdbPort"))
		try
		{
			double port = v->number();
			if (port == double(int(port)) && port >= Settings::min_gdb_port && port <= Settings::max_gdb_port)
				settings.gdb_port = int(port);
		}
		catch (const json::Error &) {}
	core->core.set_settings(std::move(settings));
	return 0;
}

char *pinesy_core_settings(const pinesy_core *core)
{
	if (!core) return nullptr;
	const Settings &s = core->core.settings();
	return give(json::Object{
		{ "romFolders", encode::strings(s.rom_folders) },
		{ "dataFolder", s.data_folder },
		{ "debugger", debugger_name(s.debugger) },
		{ "gdbPort", s.gdb_port } });
}

char *pinesy_core_data_folder(const pinesy_core *core)
{
	return core ? give(core->core.data_folder()) : nullptr;
}

char *pinesy_core_user_rom_folder(const char *data_folder)
{
	return data_folder ? give(paths::user_roms(data_folder)) : nullptr;
}

int pinesy_core_is_usable_rom_folder(const char *path)
{
	return path && paths::is_usable_rom_folder(path) ? 1 : 0;
}

//------------------------------------------------------------
//  Paths
//------------------------------------------------------------

char *pinesy_core_fixed_options(const pinesy_core *core)
{
	return core ? give(encode::strings(core->core.fixed_options())) : nullptr;
}

char *pinesy_core_rom_folders(const pinesy_core *core)
{
	return core ? give(encode::strings(core->core.rom_folders())) : nullptr;
}

void pinesy_core_create_folders(const pinesy_core *core)
{
	if (core) core->core.create_folders();
}

//------------------------------------------------------------
//  The catalogue
//------------------------------------------------------------

char *pinesy_core_catalogue(pinesy_core *core)
{
	if (!core) return nullptr;
	json::Array sections;
	for (const auto &s : core->core.catalogue().sections())
		sections.push_back(json::Object{
			{ "name", s.name }, { "showsMaker", s.shows_maker }, { "machines", encode::machines(s.machines) } });
	return give(json::Value(std::move(sections)));
}

char *pinesy_core_search(pinesy_core *core, const char *text)
{
	if (!core || !text) return nullptr;
	return give(encode::machines(core->core.catalogue().search(text)));
}

char *pinesy_core_machine_for_driver(pinesy_core *core, const char *driver)
{
	if (!core || !driver) return nullptr;
	const Catalogue::Machine *m = core->core.catalogue().machine_for_driver(driver);
	return give(m ? encode::machine(*m) : json::Value());
}

char *pinesy_core_starting_arguments(pinesy_core *core, const char *driver)
{
	if (!core || !driver) return nullptr;
	return give(encode::strings(core->core.catalogue().starting_arguments(driver)));
}

//------------------------------------------------------------
//  Configurations
//------------------------------------------------------------

char *pinesy_core_new_configuration(pinesy_core *core, const char *machine)
{
	if (!core || !machine) return nullptr;
	const Catalogue &catalogue = core->core.catalogue();
	for (const auto &section : catalogue.sections())
		for (const auto &m : section.machines)
			if (m.id == machine) return give(encode::strings(editor::new_configuration(m, catalogue)));
	return nullptr;
}

char *pinesy_core_set_driver(pinesy_core *core, const char *arguments, const char *driver)
{
	auto args = read_arguments(arguments);
	if (!core || !args || !driver) return nullptr;
	return give(encode::strings(editor::set_driver(*args, driver, core->core.catalogue(), core->core.describer())));
}

char *pinesy_core_set_rom(pinesy_core *core, const char *arguments, const char *driver, const char *bios)
{
	auto args = read_arguments(arguments);
	if (!core || !args || !driver) return nullptr;
	std::optional<std::string> set = bios ? std::optional<std::string>(bios) : std::nullopt;
	return give(encode::strings(editor::set_rom(*args, driver, set, core->core.catalogue(), core->core.describer())));
}

char *pinesy_core_set_slot(pinesy_core *core, const char *arguments, const char *slot, const char *card)
{
	auto args = read_arguments(arguments);
	if (!core || !args || !slot || !card) return nullptr;
	const Catalogue &catalogue = core->core.catalogue();
	MachineDescription description = core->core.describe(*args);
	const Slot *found = find_slot(description, slot);
	if (!found) return nullptr;
	return give(encode::strings(editor::set_slot(*args, *found, card, catalogue, core->core.describer())));
}

char *pinesy_core_set_card_rom(pinesy_core *core, const char *arguments, const char *slot, const char *bios)
{
	auto args = read_arguments(arguments);
	if (!core || !args || !slot || !bios) return nullptr;
	const Catalogue &catalogue = core->core.catalogue();
	MachineDescription description = core->core.describe(*args);
	const Slot *found = find_slot(description, slot);
	if (!found) return nullptr;
	return give(encode::strings(editor::set_card_rom(*args, *found, bios, catalogue, core->core.describer())));
}

char *pinesy_core_set_media(const char *arguments, const char *media, const char *path)
{
	auto args = read_arguments(arguments);
	if (!args || !media) return nullptr;
	std::optional<std::string> file = path ? std::optional<std::string>(path) : std::nullopt;
	return give(encode::strings(editor::set_media(*args, media, file)));
}

char *pinesy_core_sd_card_problem(const char *path)
{
	return give(path ? sd_card::problem(std::string(path)) : std::string());
}

char *pinesy_core_normalise(pinesy_core *core, const char *arguments)
{
	auto args = read_arguments(arguments);
	if (!core || !args) return nullptr;
	return give(encode::strings(editor::normalise(*args, core->core.describer())));
}

char *pinesy_core_summary(pinesy_core *core, const char *arguments, const char *machine_hint)
{
	auto args = read_arguments(arguments);
	if (!core || !args) return nullptr;
	const Catalogue &catalogue = core->core.catalogue();
	Summary summary(*args, core->core.describe(*args), catalogue, machine_hint ? machine_hint : "");
	return give(encode::summary(summary));
}

//------------------------------------------------------------
//  .pinesy files
//------------------------------------------------------------

char *pinesy_core_read_document(const char *text)
{
	if (!text) return nullptr;
	auto document = Document::read(text);
	if (!document) return nullptr;
	return give(json::Object{ { "machine", document->machine }, { "arguments", encode::strings(document->arguments) } });
}

char *pinesy_core_write_document(const char *machine, const char *arguments)
{
	auto args = read_arguments(arguments);
	if (!machine || !args) return nullptr;
	return give(Document{ machine, *args }.write());
}

//------------------------------------------------------------
//  Runs and headless mode
//------------------------------------------------------------

char *pinesy_core_debug_arguments(const pinesy_core *core, int debug)
{
	return core ? give(encode::strings(debug_arguments(core->core.settings(), debug != 0))) : nullptr;
}

char *pinesy_core_run_command_line(const pinesy_core *core, const char *arguments, int debug)
{
	auto args = read_arguments(arguments);
	if (!core || !args) return nullptr;
	return give(encode::strings(core->core.run_command_line(*args, debug != 0)));
}

int pinesy_core_run(const pinesy_core *core, const char *arguments, int debug, const pinesy_host *host)
{
	auto args = read_arguments(arguments);
	if (!core || !args) return -1;
	return core->core.run(*args, debug != 0, host);
}

char *pinesy_core_copy_command_line(const pinesy_core *core, const char *executable, const char *arguments, int debug)
{
	auto args = read_arguments(arguments);
	if (!core || !executable || !args) return nullptr;
	return give(core->core.copy_command_line(executable, *args, debug != 0));
}

int pinesy_core_video_none(int argc, const char *const argv[])
{
	return headless::video_none(read_argv(argc, argv)) ? 1 : 0;
}

int pinesy_core_opens_debugger_windows(int argc, const char *const argv[])
{
	return headless::opens_debugger_windows(read_argv(argc, argv)) ? 1 : 0;
}

int pinesy_core_headless(const pinesy_core *core, int argc, const char *const argv[], const pinesy_host *host)
{
	if (!core) return -1;
	return core->core.headless(read_argv(argc, argv), host);
}

} // extern "C"
