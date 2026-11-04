// license:BSD-3-Clause
// copyright-holders:D. Rimron-Soutter
//============================================================
//
//  core_tests.cpp - the core's rules, checked against the
//  real engine
//
//  core_tests rules     the catalogue, configurations, .pinesy
//                       files, checking and the fixed options
//  core_tests headless  one headless run with -video none
//
//  Every expected value comes from the Swift app's README
//  (public/app/README.md) or its sources (public/app/Sources);
//  each check names the file and line. Values that come from
//  MAME's own data (driver names, years, slot tags) are those
//  of the engine's driver list, read by the rule the comment
//  names.
//
//  The resources folder is PINESY_TEST_RESOURCES, so its roms
//  folder holds the ROM sets; the data folder, where MAME
//  writes, is PINESY_TEST_DATA in the build folder.
//
//============================================================

#include "pinesy_core.h"
#include "pinesy_core.hpp"
#include "json.hpp"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

using namespace pinesy::core;
using Strings = std::vector<std::string>;

namespace {

int s_checks = 0;
int s_failed = 0;

void check(bool ok, const std::string &what)
{
	s_checks++;
	if (!ok) s_failed++;
	std::printf("%s - %s\n", ok ? "ok" : "FAIL", what.c_str());
}

std::string take(char *text)
{
	std::string s = text ? text : "<NULL>";
	pinesy_core_free(text);
	return s;
}

json::Value parse(char *text)
{
	std::string s = take(text);
	try
	{
		return json::parse(s);
	}
	catch (const json::Error &)
	{
		return json::Value();
	}
}

Strings strings(const json::Value &v)
{
	Strings out;
	if (!v.is_array()) return out;
	for (const auto &item : v.array()) out.push_back(item.string());
	return out;
}

std::string args_json(const Strings &list)
{
	json::Array a;
	for (const auto &s : list) a.push_back(s);
	return json::write(json::Value(std::move(a)));
}

std::string show(const Strings &list)
{
	return args_json(list);
}

const json::Value *member(const json::Value &object, const char *key)
{
	return object.is_object() ? object.find(key) : nullptr;
}

std::string text_of(const json::Value &object, const char *key)
{
	const json::Value *v = member(object, key);
	return v && !v->is_null() ? v->string() : std::string("<null>");
}

const json::Value *find_by(const json::Value &list, const char *key, const std::string &value)
{
	if (!list.is_array()) return nullptr;
	for (const auto &item : list.array())
		if (text_of(item, key) == value) return &item;
	return nullptr;
}

Strings drivers_of(const json::Value &models)
{
	Strings out;
	if (models.is_array())
		for (const auto &m : models.array()) out.push_back(text_of(m, "driver"));
	return out;
}

bool contains(const Strings &list, const std::string &item)
{
	return std::find(list.begin(), list.end(), item) != list.end();
}

pinesy_core *make_core()
{
	pinesy_core *core = pinesy_core_create(PINESY_TEST_RESOURCES, PINESY_TEST_DATA);
	pinesy_core_create_folders(core);
	return core;
}

//------------------------------------------------------------
//  The catalogue
//------------------------------------------------------------

void test_catalogue(pinesy_core *core)
{
	json::Value sections = parse(pinesy_core_catalogue(core));
	check(sections.is_array() && !sections.array().empty(), "the catalogue has sections");
	if (!sections.is_array() || sections.array().empty()) return;

	auto section_of = [&](const std::string &id) -> const json::Value * {
		for (const auto &s : sections.array())
			if (find_by(*member(s, "machines"), "id", id)) return &s;
		return nullptr;
	};
	auto machine = [&](const std::string &id) -> const json::Value * {
		const json::Value *s = section_of(id);
		return s ? find_by(*member(*s, "machines"), "id", id) : nullptr;
	};

	// README.md:32: a driver and its clones sharing one name before a
	// colon ("ZX Spectrum Next: KS1") are one machine with a model
	// choice; the parent (tbblue) is not offered as a model.
	const json::Value *next = machine("tbblue");
	check(next && text_of(*next, "name") == "ZX Spectrum Next", "the Next is one machine named \"ZX Spectrum Next\" (README.md:32)");
	if (next)
	{
		// Catalogue.swift:127: models are the clones in year order;
		// their labels are the text after the colon (Catalogue.swift:180-184).
		Strings models = drivers_of(*member(*next, "models"));
		check(models == Strings{ "specnext_ks1", "specnext_ks2", "specnext_ks3" },
				"the Next's models are KS1, KS2, KS3 in year order (README.md:32, Catalogue.swift:127): " + show(models));
		const json::Value *ks1 = find_by(*member(*next, "models"), "driver", "specnext_ks1");
		check(ks1 && text_of(*ks1, "label") == "KS1", "the KS1 model's label is \"KS1\" (Catalogue.swift:180-184)");
		check(drivers_of(*member(*next, "otherModels")) == Strings{ "tbblue" },
				"tbblue is not offered as a model, only kept as the reference (README.md:32, Catalogue.swift:137)");
		// README.md:32: a new configuration uses the newest model;
		// Catalogue.swift:56: defaultDriver is the last model.
		check(text_of(*next, "defaultDriver") == "specnext_ks3", "the Next's default driver is the newest model, specnext_ks3 (README.md:32, Catalogue.swift:56)");
	}
	json::Value ks2 = parse(pinesy_core_machine_for_driver(core, "specnext_ks2"));
	check(text_of(ks2, "id") == "tbblue", "the KS2 driver belongs to the Next machine (Catalogue.swift:154-159)");

	// README.md:33: machines are grouped by their first maker.
	// Catalogue.swift:191-205: "SpecNext Ltd., Victor Trucco, Fabio
	// Belavenuto" names the makers "SpecNext Ltd.", "Victor Trucco"
	// and "Fabio Belavenuto"; the first is the section.
	const json::Value *next_section = section_of("tbblue");
	check(next_section && text_of(*next_section, "name") == "SpecNext Ltd.", "the Next is in the \"SpecNext Ltd.\" section (README.md:33, Catalogue.swift:191-205)");

	const json::Value *spectrum = section_of("spectrum");
	const json::Value *spec128 = section_of("spec128");
	check(spectrum && text_of(*spectrum, "name") == "Sinclair Research Ltd" && spectrum == spec128,
			"the ZX Spectrum and the ZX Spectrum 128 are in the \"Sinclair Research Ltd\" section (README.md:33)");
	check(spectrum && member(*spectrum, "showsMaker")->boolean() == false, "a maker's section does not show the maker (Catalogue.swift:70-71)");

	// README.md:33: a clone joins its parent's group only when they share
	// a maker. The +2 (Amstrad plc) is a clone of the 128 (Sinclair
	// Research Ltd), so it is in Amstrad's section with the +2a and +3.
	const json::Value *plus2 = section_of("specpls2");
	check(plus2 && text_of(*plus2, "name") == "Amstrad plc" && plus2 == section_of("specpl2a") && plus2 == section_of("specpls3"),
			"the +2, +2a and +3 are in the \"Amstrad plc\" section (README.md:33, Catalogue.swift:212-222)");

	// README.md:33: makers with one product are under "Other makers".
	// Catalogue.swift:228: so is a maker MAME does not know ("<unknown>").
	// Catalogue.swift:244: "Other makers" comes after the makers' sections.
	const json::Value &last = sections.array().back();
	check(text_of(last, "name") == Catalogue::other_makers && member(last, "showsMaker")->boolean(),
			"\"Other makers\" is the last section and shows the maker (Catalogue.swift:70-71, 244)");
	check(section_of("bk08") == &last, "BK-08, the only product of its maker (Orel), is under \"Other makers\" (README.md:33)");
	check(section_of("zvezda") == &last, "Zvezda, whose maker is \"<unknown>\", is under \"Other makers\" (Catalogue.swift:228)");

	// README.md:31: an unofficial driver (the +3e ROMs) is a ROM choice
	// of its machine: its parent, or the official driver in the same
	// source file with the same devices. The +3e drivers have no parent;
	// in MAME's data that driver is specpls3, the +3.
	json::Value plus3e = parse(pinesy_core_machine_for_driver(core, "specpl3e"));
	check(text_of(plus3e, "id") == "specpls3", "the +3e driver is a ROM choice of the ZX Spectrum +3 (README.md:31, Catalogue.swift:163-172)");
	if (plus3e.is_object())
	{
		Strings variants = drivers_of(*member(plus3e, "variants"));
		check(contains(variants, "specpl3e") && contains(variants, "sp3e8bit") && contains(variants, "sp3eata") && contains(variants, "sp3ezcf"),
				"the +3's ROM choices hold every +3e driver (README.md:31): " + show(variants));
	}
	check(!machine("specpl3e"), "a ROM variant is not a machine of its own (README.md:31)");

	// README.md:35: search finds machines by name, maker, model or ROM
	// variant name. Catalogue.swift:94-98: empty text finds nothing.
	auto found = [&](const char *text) {
		Strings ids;
		json::Value list = parse(pinesy_core_search(core, text));
		if (list.is_array())
			for (const auto &m : list.array()) ids.push_back(text_of(m, "id"));
		return ids;
	};
	check(contains(found("spectrum next"), "tbblue"), "search finds the Next by name, ignoring case (README.md:35, Catalogue.swift:97)");
	check(contains(found("SpecNext"), "tbblue"), "search finds the Next by maker (README.md:35)");
	check(contains(found("KS2"), "tbblue"), "search finds the Next by model (README.md:35)");
	check(contains(found("+3e 8bit ZXCF"), "specpls3"), "search finds the +3 by a ROM variant name (README.md:35)");
	check(found("  ").empty(), "search for blank text finds nothing (Catalogue.swift:95-96)");
	check(!contains(found("unknown"), "zvezda"), "the maker \"<unknown>\" is not searched (Catalogue.swift:143-144)");
}

//------------------------------------------------------------
//  The starting configuration
//------------------------------------------------------------

const json::Value *slot_row(const json::Value &summary, const std::string &tag)
{
	const json::Value *hardware = member(summary, "hardware");
	if (!hardware || !hardware->is_array()) return nullptr;
	for (const auto &row : hardware->array())
		if (text_of(*member(row, "slot"), "tag") == tag) return &row;
	return nullptr;
}

std::string fitted(pinesy_core *core, const Strings &arguments, const std::string &tag)
{
	json::Value summary = parse(pinesy_core_summary(core, args_json(arguments).c_str(), nullptr));
	const json::Value *row = slot_row(summary, tag);
	return row ? text_of(*member(*row, "slot"), "current") : "<no slot>";
}

void test_starting(pinesy_core *core)
{
	// README.md:39: a port where MAME fits the same card on every model
	// of the family is left empty: the Pi and MIDI ports of the Next.
	// Catalogue.swift:253-258: each becomes "-<port>", "".
	// Configuration.swift:112: a new configuration starts the newest model.
	Strings next = strings(parse(pinesy_core_new_configuration(core, "tbblue")));
	check(!next.empty() && next[0] == "specnext_ks3", "a new Next configuration runs the newest model, specnext_ks3 (README.md:32, Configuration.swift:112): " + show(next));
	Arguments args = Arguments::parse(next);
	check(args.get("rs232_rpi") == std::string(), "a new Next configuration leaves the Pi port empty (README.md:39): " + show(next));
	check(args.get("mdin") == std::string() && args.get("mdthru") == std::string() && args.get("mdout") == std::string(),
			"a new Next configuration leaves the MIDI ports empty (README.md:39)");
	// README.md:39: memory keeps MAME's setting; the ESP card, fitted on
	// some models and not others, stays as MAME has it.
	check(!args.get("ram") && !args.get("rs232_esp"), "a new Next configuration names no memory and no ESP option (README.md:39)");

	check(fitted(core, next, "rs232_esp") == "esp8266_at", "the ESP is fitted on a new KS3 configuration (README.md:39)");
	check(fitted(core, next, "rs232_rpi") == "", "the Pi port is empty on a new KS3 configuration (README.md:39)");

	Strings ks2 = strings(parse(pinesy_core_starting_arguments(core, "specnext_ks2")));
	check(!Arguments::parse(ks2).get("rs232_esp") && fitted(core, ks2, "rs232_esp") == "esp8266_at",
			"the ESP is fitted on a new KS2 configuration (README.md:39): " + show(ks2));
	Strings ks1 = strings(parse(pinesy_core_starting_arguments(core, "specnext_ks1")));
	check(!Arguments::parse(ks1).get("rs232_esp") && fitted(core, ks1, "rs232_esp") == "",
			"the ESP is not fitted on a new KS1 configuration (README.md:39): " + show(ks1));

	// README.md:39: the Kempston joystick interface on the ZX Spectrum
	// and its clones is left empty.
	Strings spectrum = strings(parse(pinesy_core_starting_arguments(core, "spectrum")));
	check(Arguments::parse(spectrum).get("exp") == std::string(), "a new ZX Spectrum configuration leaves the Kempston port empty (README.md:39): " + show(spectrum));

	// Catalogue.swift:264: a driver the catalogue does not have starts
	// as just the driver.
	Strings unknown = strings(parse(pinesy_core_starting_arguments(core, "nosuchdriver")));
	check(unknown == Strings{ "nosuchdriver" }, "an unknown driver starts with no options (Catalogue.swift:264)");
}

//------------------------------------------------------------
//  Arguments and .pinesy files
//------------------------------------------------------------

void test_documents()
{
	// README.md:23: a string that starts with "-" is always an option
	// name, never a value, so a flag stays a flag; strings that are not
	// options stay where they are. Configuration.swift:26-45.
	Strings line{ "tbblue", "-skip_gameinfo", "-pi", "/path/nextpi.img", "-rs232_rpi", "", "loose" };
	Arguments args = Arguments::parse(line);
	check(args.driver == "tbblue" && args.options.size() == 4 && !args.options[0].has_value && args.options[0].name == "skip_gameinfo",
			"a flag stays a flag (README.md:23, Configuration.swift:37-43)");
	check(args.get("rs232_rpi") == std::string() && args.options[3].name.empty() && args.options[3].value == "loose",
			"an empty value is a value, and a loose string stays where it is (README.md:23)");
	check(args.array() == line, "a command line reads back as it was written (Configuration.swift:47-52)");

	// README.md:19-22, ConfigurationDocument.swift:44-48: a JSON object
	// with format (now 1), machine and arguments.
	Strings saved{ "specnext_ks3", "-rs232_rpi", "", "-pi", "/tmp/a b/next.img" };
	std::string text = take(pinesy_core_write_document("tbblue", args_json(saved).c_str()));
	json::Value written;
	try { written = json::parse(text); } catch (const json::Error &) {}
	check(written.is_object() && written.object().size() == 3 && member(written, "format") && member(written, "format")->number() == 1
			&& text_of(written, "machine") == "tbblue" && strings(*member(written, "arguments")) == saved,
			"a .pinesy file has format 1, machine and arguments (README.md:19-23, ConfigurationDocument.swift:12)");
	check(text.find("\"/tmp/a b/next.img\"") != std::string::npos, "slashes are not escaped (ConfigurationDocument.swift:46)");
	check(text.find("\"arguments\"") < text.find("\"format\"") && text.find("\"format\"") < text.find("\"machine\""),
			"keys are written sorted (ConfigurationDocument.swift:46)");

	json::Value read = parse(pinesy_core_read_document(text.c_str()));
	check(text_of(read, "machine") == "tbblue" && member(read, "arguments") && strings(*member(read, "arguments")) == saved,
			"a .pinesy file reads back as it was written");

	// ConfigurationDocument.swift:37: a newer format, or empty arguments,
	// is not read.
	check(take(pinesy_core_read_document(R"({"format":2,"machine":"tbblue","arguments":["tbblue"]})")) == "<NULL>",
			"a file of format 2 is not read (ConfigurationDocument.swift:37)");
	check(take(pinesy_core_read_document(R"({"format":1,"machine":"tbblue","arguments":[]})")) == "<NULL>",
			"a file with empty arguments is not read (ConfigurationDocument.swift:37)");
	check(take(pinesy_core_read_document(R"({"format":1,"arguments":["tbblue"]})")) == "<NULL>",
			"a file without machine is not read (ConfigurationDocument.swift:26-30, 36)");
}

//------------------------------------------------------------
//  Checking with MAME
//------------------------------------------------------------

void test_checking(pinesy_core *core)
{
	// README.md:25: an option the machine no longer has is removed.
	// The Next has no "exp" slot.
	Strings lacking = strings(parse(pinesy_core_normalise(core, args_json({ "specnext_ks3", "-exp", "intf1" }).c_str())));
	check(lacking == Strings{ "specnext_ks3" }, "an option the Next lacks is removed (README.md:25, Configuration.swift:180-200): " + show(lacking));

	// README.md:25: a change of model keeps what the new model also has
	// and drops the rest. The KS1 takes 1m of memory; the KS3 does not.
	Strings ks1 = strings(parse(pinesy_core_starting_arguments(core, "specnext_ks1")));
	ks1.insert(ks1.end(), { "-ram", "1m" });
	Strings ks3 = strings(parse(pinesy_core_set_driver(core, args_json(ks1).c_str(), "specnext_ks3")));
	check(!ks3.empty() && ks3[0] == "specnext_ks3" && !Arguments::parse(ks3).get("ram") && Arguments::parse(ks3).get("rs232_rpi") == std::string(),
			"a KS1 configuration changed to the KS3 drops -ram 1m and keeps the empty Pi port (README.md:25, Configuration.swift:117-132): " + show(ks3));

	// README.md:25: when MAME rejects the driver itself, no option is removed.
	Strings bad{ "nosuchdriver", "-ram", "1m" };
	check(strings(parse(pinesy_core_normalise(core, args_json(bad).c_str()))) == bad,
			"no option is removed when MAME rejects the driver (README.md:25, Configuration.swift:185)");

	// Configuration.swift:182: a command line MAME accepts stays as it is.
	Strings start = strings(parse(pinesy_core_new_configuration(core, "tbblue")));
	check(strings(parse(pinesy_core_normalise(core, args_json(start).c_str()))) == start, "a valid command line is not changed (Configuration.swift:182)");

	// README.md:25: when MAME rejects the command line, the window shows
	// MAME's error text and Run is not available.
	json::Value rejected = parse(pinesy_core_summary(core, args_json({ "specnext_ks3", "-nosuch", "1" }).c_str(), nullptr));
	check(!text_of(rejected, "errors").empty() && text_of(rejected, "errors") != "<null>" && member(rejected, "canRun") && !member(rejected, "canRun")->boolean(),
			"a rejected command line has MAME's error text and cannot run (README.md:25): " + text_of(rejected, "errors"));

	// README.md:25: a media file that is not there is marked on its row.
	Strings with_tape = strings(parse(pinesy_core_set_media(args_json(start).c_str(), "cass", "/nonexistent/pinesy-test.tap")));
	json::Value tape = parse(pinesy_core_summary(core, args_json(with_tape).c_str(), "tbblue"));
	check(text_of(tape, "errors").empty() && member(tape, "missingFiles") && strings(*member(tape, "missingFiles")) == Strings{ "cass" },
			"a missing tape file is marked on the cassette row (README.md:25, Configuration.swift:306-308)");

	// Configuration.swift:264: the summary names the machine.
	json::Value summary = parse(pinesy_core_summary(core, args_json(start).c_str(), nullptr));
	check(member(summary, "machine") && text_of(*member(summary, "machine"), "id") == "tbblue" && member(summary, "canRun")->boolean(),
			"the summary of a new Next configuration names the Next and can run (Configuration.swift:257, 264)");
	const json::Value *ram = slot_row(summary, "ram");
	check(ram && text_of(*ram, "label") == "Memory", "the memory slot's row is labelled \"Memory\" (Configuration.swift:362-369)");

	// Configuration.swift:137-138: the driver's default ROM removes -bios.
	Strings rom = strings(parse(pinesy_core_set_rom(core, args_json(start).c_str(), "specnext_ks3", "v30204")));
	check(!Arguments::parse(rom).get("bios"), "choosing the default ROM names no -bios (Configuration.swift:137-138): " + show(rom));

	// Configuration.swift:143-151: MAME's default card is written out when
	// the slot starts empty. The Spectrum's port starts empty, so fitting
	// the Kempston interface (MAME's default there) names it.
	Strings spectrum = strings(parse(pinesy_core_starting_arguments(core, "spectrum")));
	Strings kempston = strings(parse(pinesy_core_set_slot(core, args_json(spectrum).c_str(), "exp", "kempjoy")));
	check(Arguments::parse(kempston).get("exp") == std::string("kempjoy"),
			"fitting MAME's default card in a port that starts empty names the card (Configuration.swift:143-151): " + show(kempston));

	// Configuration.swift:155-165: a card ROM other than the default is
	// written as "<card>,bios=<set>"; the default ROM of MAME's default
	// card, in a slot that does not start empty, removes the option.
	Strings esp = strings(parse(pinesy_core_set_card_rom(core, args_json(start).c_str(), "rs232_esp", "at13")));
	check(Arguments::parse(esp).get("rs232_esp") == std::string("esp8266_at,bios=at13"),
			"a card ROM that is not the default is written with the card (Configuration.swift:155-165): " + show(esp));
	Strings esp_default = strings(parse(pinesy_core_set_card_rom(core, args_json(esp).c_str(), "rs232_esp", "at11")));
	check(!Arguments::parse(esp_default).get("rs232_esp"),
			"the default ROM of the default card removes the option (Configuration.swift:155-165): " + show(esp_default));
	check(take(pinesy_core_set_slot(core, args_json(start).c_str(), "nosuchslot", "")) == "<NULL>",
			"a slot the machine does not have gives NULL (pinesy_core.h)");
}

//------------------------------------------------------------
//  Paths, settings and command lines
//------------------------------------------------------------

void test_paths(pinesy_core *core)
{
	const std::string res = PINESY_TEST_RESOURCES;
	const std::string data = PINESY_TEST_DATA;
	const std::string extra = data + "/extra-roms";

	pinesy_core_set_settings(core, (R"({"romFolders":[")" + extra + R"(",")" + data + R"(/bad;folder"]})").c_str());
	Strings options = strings(parse(pinesy_core_fixed_options(core)));

	// README.md:66, MAMEPaths.swift:53: the first option is -noreadconfig.
	check(!options.empty() && options[0] == "-noreadconfig", "the fixed options start with -noreadconfig (README.md:66, MAMEPaths.swift:53)");
	// README.md:67, MAMEPaths.swift:44: the bundled ROMs, the data
	// folder's roms, then the settings' folders; one with ";" is left out.
	std::string rompath = options.size() > 2 && options[1] == "-rompath" ? options[2] : "<none>";
	check(rompath == res + "/roms;" + data + "/roms;" + extra,
			"the ROM path is the bundled ROMs, the data folder's roms, then the settings' folders, without the one with \";\" (README.md:67, MAMEPaths.swift:40-45): " + rompath);

	// README.md:68-69, MAMEPaths.swift:23-37.
	auto value_of = [&](const std::string &option) {
		auto at = std::find(options.begin(), options.end(), option);
		return at != options.end() && at + 1 != options.end() ? *(at + 1) : std::string("<none>");
	};
	check(value_of("-hashpath") == res + "/hash" && value_of("-bgfx_path") == res + "/bgfx" && value_of("-swpath") == res + "/software",
			"MAME's search paths are in the resources folder (README.md:68, MAMEPaths.swift:23-29)");
	check(value_of("-cfg_directory") == data + "/cfg" && value_of("-nvram_directory") == data + "/nvram" && value_of("-snapshot_directory") == data + "/snap"
			&& value_of("-inipath") == data + "/ini" && value_of("-homepath") == data + "/home",
			"the folders MAME writes are in the data folder (README.md:69, MAMEPaths.swift:31-37)");
	check(take(pinesy_core_data_folder(core)) == data, "an empty data folder setting uses the default (AppSettings.swift:41, 44)");

	// AppSettings.swift:42-46: an unusable debugger or port takes its default.
	pinesy_core_set_settings(core, R"({"debugger":"nonsense","gdbPort":70000})");
	json::Value settings = parse(pinesy_core_settings(core));
	check(text_of(settings, "debugger") == "windows" && member(settings, "gdbPort")->number() == PINESY_CORE_DEFAULT_GDB_PORT,
			"an unknown debugger is \"windows\" and a port out of range is 23946 (README.md:50, AppSettings.swift:42-46)");

	// Configuration.swift:407-415: the GDB stub adds its options and port.
	pinesy_core_set_settings(core, R"({"debugger":"gdbStub","gdbPort":1234})");
	Strings debug = strings(parse(pinesy_core_debug_arguments(core, 1)));
	check(debug == Strings{ "-debug", "-debugger", "gdbstub", "-debugger_port", "1234" }, "a debug run with the GDB stub adds its port (README.md:50, Configuration.swift:407-415): " + show(debug));

	// README.md:52, Configuration.swift:420-431: Copy Command Line gives
	// the headless command, quoted for zsh.
	std::string line = take(pinesy_core_copy_command_line(core, "/opt/Pinesy", args_json({ "tbblue", "-rs232_rpi", "", "-pi", "/a b/it's.img" }).c_str(), 0));
	check(line == "/opt/Pinesy -- tbblue -rs232_rpi '' -pi '/a b/it'\\''s.img'", "the copied command line is quoted for zsh (Configuration.swift:425-431): " + line);
	pinesy_core_set_settings(core, "{}");

	// Headless.swift:63-83.
	const char *video[] = { "tbblue", "-video", "opengl", "-video", "none" };
	check(pinesy_core_video_none(5, video) == 1, "the last -video wins (Headless.swift:63-70)");
	const char *gdb[] = { "tbblue", "-debug", "-debugger", "gdbstub" };
	const char *windows[] = { "tbblue", "-d" };
	check(pinesy_core_opens_debugger_windows(4, gdb) == 0 && pinesy_core_opens_debugger_windows(2, windows) == 1,
			"the GDB stub opens no debugger windows; -d alone does (README.md:56, Headless.swift:72-83)");
	check(pinesy_core_is_usable_rom_folder("/a;b") == 0, "a folder with \";\" cannot be a ROM folder (README.md:67, MAMEPaths.swift:47-49)");
}

int run_rules()
{
	pinesy_core *core = make_core();
	test_documents();
	test_paths(core);
	test_catalogue(core);
	test_starting(core);
	test_checking(core);
	pinesy_core_destroy(core);
	return 0;
}

int run_headless()
{
	// README.md:56-59: headless mode runs MAME with the arguments, drops a
	// leading "--", and exits with MAME's exit code.
	pinesy_core *core = pinesy_core_create(PINESY_TEST_RESOURCES, PINESY_TEST_DATA);
	const char *argv[] = { "--", "tbblue", "-video", "none", "-sound", "none", "-seconds_to_run", "2" };
	int code = pinesy_core_headless(core, 8, argv, nullptr);
	check(code == 0, "a headless run of tbblue with -video none exits with 0 (README.md:56-59): exit code " + std::to_string(code));
	// MAMEPaths.swift:63-69: the run creates the folders MAME writes.
	check(std::filesystem::is_directory(std::string(PINESY_TEST_DATA) + "/cfg") && std::filesystem::is_directory(std::string(PINESY_TEST_DATA) + "/roms"),
			"the headless run created the data folder's folders (MAMEPaths.swift:63-69)");
	pinesy_core_destroy(core);
	return 0;
}

} // anonymous namespace

int main(int argc, char **argv)
{
	std::string mode = argc > 1 ? argv[1] : "rules";
	if (mode == "rules") run_rules();
	else if (mode == "headless") run_headless();
	else
	{
		std::fprintf(stderr, "usage: core_tests rules | headless\n");
		return 2;
	}
	std::printf("%s: %d checks, %d failed\n", mode.c_str(), s_checks, s_failed);
	return s_failed ? 1 : 0;
}
