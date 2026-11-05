// license:BSD-3-Clause
// copyright-holders:D. Rimron-Soutter
//============================================================
//
//  pinesy_core.hpp - the shared rules of the pinesy front ends
//
//  Every rule the front ends share: the machine catalogue,
//  the configuration model and .pinesy files, the starting
//  configuration, option checking, MAME's paths, the settings
//  schema, the copied command line and headless mode. The
//  front ends hold only UI.
//
//  The C interface in pinesy_core.h wraps this one.
//
//  Calls that ask the engine (Core::describe and every call
//  that checks a command line, Core::catalogue the first time,
//  Core::run and Core::headless) run on the main thread, and
//  never while a machine runs.
//
//============================================================

#ifndef PINESY_CORE_HPP
#define PINESY_CORE_HPP

#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

struct pinesy_host;

namespace pinesy::core {

//------------------------------------------------------------
//  The engine's data: the JSON documents of
//  pinesy_list_machines() and pinesy_describe(), with MAME's
//  field names, as pinesy.h documents them.
//------------------------------------------------------------

struct BiosSet
{
	std::string name;
	std::string description;
	bool is_default = false;
	bool operator==(const BiosSet &) const = default;
};

// MAME's default set: the one marked default, else the first.
const BiosSet *default_set(const std::vector<BiosSet> &sets);

struct SlotOption
{
	std::string name;
	std::string description;
	bool operator==(const SlotOption &) const = default;
};

struct Slot
{
	std::string tag;               // the option name, such as "exp" or "exp:intf1:exp"
	std::string parent;            // the slot of the card this slot belongs to; empty for the machine
	std::string kind;              // "memory", "drive", "sound" or "port"
	std::string type;
	std::string typedescription;
	bool fixed = false;
	std::string default_card;      // MAME's "default"
	std::string current;           // the fitted card; empty for none
	std::optional<std::string> currentdescription;
	std::vector<SlotOption> options;
	std::optional<std::vector<BiosSet>> biossets;  // the fitted card's ROMs
	std::optional<std::string> bios;
	bool is_port() const { return kind == "port"; }
	bool operator==(const Slot &) const = default;
};

struct MediaOwner
{
	std::string tag;
	std::string description;
	std::string slot;              // the slot of the card holding the device; empty for the machine
	std::string partof;            // the device it is part of, such as an SD card slot
	bool operator==(const MediaOwner &) const = default;
};

struct Media
{
	std::string name;              // the option name, such as "cass" or "pi"
	std::string instance;
	std::string brief;
	std::string type_name;         // MAME's "typename"
	MediaOwner owner;
	std::string extensions;        // comma-separated, without dots
	std::string interface;
	bool mandatory = false;
	std::string current;           // the file named on the command line; empty for none
	std::vector<std::string> extension_list() const;
	bool operator==(const Media &) const = default;
};

struct MachineDescription
{
	std::optional<std::string> driver;
	std::optional<std::string> bios;
	std::optional<std::vector<BiosSet>> biossets;
	std::optional<std::vector<Slot>> slots;
	std::optional<std::vector<Media>> media;
	std::string errors;
	std::optional<std::string> warnings;   // they do not stop a run

	// No errors, and MAME named the driver.
	bool is_valid() const { return errors.empty() && driver.has_value(); }
};

struct DriverInfo
{
	std::string name;
	std::string description;
	std::string manufacturer;
	std::string year;
	std::optional<std::string> cloneof;
	std::string sourcefile;
	bool unofficial = false;
	std::string status;
	std::vector<BiosSet> biossets;
	std::vector<std::string> devices;
	std::vector<Slot> slots;
};

// The list from pinesy_list_machines(). Text that does not decode
// gives an empty list.
std::vector<DriverInfo> decode_driver_list(std::string_view json);

// A description from pinesy_describe(). Text that does not decode
// gives a description with only an error.
MachineDescription decode_description(std::string_view json);

//------------------------------------------------------------
//  The catalogue: the machines the user can choose, built from
//  MAME's driver list by rules that use only MAME's data.
//------------------------------------------------------------

class Catalogue
{
public:
	struct Model
	{
		std::string driver;
		std::string label;
		std::string year;
	};

	struct Variant
	{
		std::string driver;
		std::string name;
		std::vector<BiosSet> biossets;
	};

	struct Machine
	{
		std::string id;                // the family parent when the machine has models, else its own driver
		std::string name;
		std::string manufacturer;
		std::string year;
		bool preliminary = false;
		std::vector<Model> models;      // released models, in year order
		std::vector<Model> other_models; // models MAME has that are not offered (the family parent)
		std::vector<Variant> variants;  // ROM choices that run another driver
		std::string search_text;

		// The driver a new configuration runs: the newest model.
		const std::string &default_driver() const;
		std::vector<std::string> drivers() const;
		const Model *model(std::string_view driver) const;
	};

	struct Section
	{
		std::string name;
		std::vector<Machine> machines;
		bool shows_maker = false;   // true for "Other makers"
	};

	static constexpr const char *other_makers = "Other makers";

	Catalogue() = default;
	explicit Catalogue(const std::vector<DriverInfo> &list);

	const std::vector<Section> &sections() const { return m_sections; }
	std::vector<Machine> machines() const;
	const Machine *machine_for_driver(std::string_view driver) const;
	const DriverInfo *driver(std::string_view name) const;

	// Machines whose name, maker, model or ROM name contains the text.
	// Empty text finds nothing.
	std::vector<Machine> search(std::string_view text) const;

	// The command line of a new configuration for a driver: MAME's
	// defaults, with the ports MAME fills only for convenience left
	// empty.
	std::vector<std::string> starting_arguments(const std::string &driver) const;

	// The ports of a driver's default configuration that MAME fills
	// with the same card on every model of its family.
	std::vector<std::string> convenience_ports(const std::string &driver) const;

	static int year_value(std::string_view year);
	static std::vector<std::string> makers(std::string_view manufacturer);

private:
	std::vector<Section> m_sections;
	std::map<std::string, DriverInfo, std::less<>> m_drivers;
	std::map<std::string, size_t, std::less<>> m_machine_by_driver;   // index into m_machines
	std::vector<Machine> m_machines;

	std::string family_root(const std::string &driver) const;
};

//------------------------------------------------------------
//  The configuration: a MAME command line, a driver and then
//  options, in order.
//------------------------------------------------------------

struct Arguments
{
	struct Option
	{
		std::string name;      // empty for a token that is not an option
		std::string value;
		bool has_value = true; // false for a flag
		bool operator==(const Option &) const = default;
	};

	std::string driver;
	std::vector<Option> options;

	Arguments() = default;
	explicit Arguments(std::string driver, std::vector<Option> options = {}) : driver(std::move(driver)), options(std::move(options)) {}

	// Reads a command line. A string that starts with "-" is always an
	// option name, never a value.
	static Arguments parse(const std::vector<std::string> &array);

	std::vector<std::string> array() const;

	// The value of the option with a name, when it has one.
	std::optional<std::string> get(std::string_view name) const;

	// Adds an option, or replaces the one with its name in its place.
	void set(const Option &option);

	// Sets an option's value, keeping its place; nullopt removes it.
	void set(const std::string &name, std::optional<std::string> value);
};

// A slot option value: a card and, optionally, its ROM ("intf1,bios=v2").
std::string card_choice(const std::string &card, const std::string &bios = {});

// Asks MAME about a command line (the core's fixed options come first).
using Describer = std::function<MachineDescription(const std::vector<std::string> &)>;

// Edits a configuration's command line. Every edit except set_media
// asks MAME whether the result is valid and drops options the machine
// no longer has.
namespace editor {

// A new configuration for a machine: its newest model, as it shipped.
std::vector<std::string> new_configuration(const Catalogue::Machine &machine, const Catalogue &catalogue);

// Runs another driver of the same machine (a model or a ROM variant),
// keeping every choice the new driver also has.
std::vector<std::string> set_driver(const std::vector<std::string> &arguments, const std::string &driver,
		const Catalogue &catalogue, const Describer &describe);

// Chooses a ROM: a BIOS set of the machine's driver, or of a variant driver.
std::vector<std::string> set_rom(const std::vector<std::string> &arguments, const std::string &driver,
		const std::optional<std::string> &bios, const Catalogue &catalogue, const Describer &describe);

// Fits a card in a slot; an empty card leaves the slot empty.
std::vector<std::string> set_slot(const std::vector<std::string> &arguments, const Slot &slot,
		const std::string &card, const Catalogue &catalogue, const Describer &describe);

// Chooses the ROM of the card fitted in a slot.
std::vector<std::string> set_card_rom(const std::vector<std::string> &arguments, const Slot &slot,
		const std::string &bios, const Catalogue &catalogue, const Describer &describe);

// Puts a file in a media device; nullopt empties it.
std::vector<std::string> set_media(const std::vector<std::string> &arguments, const std::string &media,
		const std::optional<std::string> &path);

// The command line with every option MAME rejects removed. When MAME
// rejects the driver itself, the command line stays as it is.
std::vector<std::string> normalise(const std::vector<std::string> &arguments, const Describer &describe);

} // namespace editor

// The sizes MAME's SD card takes (spi_sdcard.cpp, device_start): an SD
// card of up to 1 GiB, or an SDHC card of up to 32 GiB that is a
// multiple of 512 KiB. A raw image has 512-byte blocks.
namespace sd_card {

// Whether MAME's SD card takes an image of this many 512-byte blocks.
bool usable(std::uint64_t blocks);

// Why MAME's SD card cannot use an image of this many bytes; empty when
// it can.
std::string problem(std::uint64_t bytes);

// The problem with the image at a path; empty for an image that works,
// a CHD (its file size is not its disk size), or a file that is not
// there.
std::string problem(const std::string &path);

} // namespace sd_card

// What the configuration window shows, in words a Spectrum owner uses.
struct Summary
{
	struct Choice
	{
		std::string value;
		std::string label;
	};

	struct HardwareRow
	{
		Slot slot;
		std::string label;
		int depth = 0;
		std::vector<Choice> choices;
		std::optional<std::string> fixed_text;  // the fitted card, for a slot that cannot be changed
		std::optional<std::string> meant_for;   // for one of several ports of a kind
		std::vector<Choice> roms;               // the fitted card's ROMs, when there is more than one
		std::string current_rom;
	};

	struct MediaRow
	{
		Media media;
		std::string label;
	};

	struct ROMChoice
	{
		std::string driver;
		std::optional<std::string> bios;
		std::string label;
	};

	std::optional<Catalogue::Machine> machine;
	std::string driver;
	std::string year;           // of the driver that runs
	bool preliminary = false;
	std::vector<Catalogue::Model> models;
	std::vector<ROMChoice> roms;
	std::vector<ROMChoice> variant_roms;
	std::optional<ROMChoice> current_rom;
	std::vector<HardwareRow> hardware;
	std::vector<MediaRow> media;
	std::string errors;
	std::string warnings;
	std::vector<std::string> missing_media;   // labels of mandatory media with no file
	std::set<std::string> missing_files;      // media names of files that are not there
	// for SD card media with an image MAME's SD card cannot use: the
	// reason, by media name
	std::map<std::string, std::string> sd_card_problems;

	bool can_run() const { return errors.empty() && missing_media.empty() && sd_card_problems.empty(); }

	// machine_hint is the document's machine, used when the catalogue
	// does not have the driver.
	Summary(const std::vector<std::string> &arguments, const MachineDescription &description,
			const Catalogue &catalogue, const std::string &machine_hint = {});

	// "48K" for MAME's "48k", "2 MB" for "2m".
	static std::string memory_size(const std::string &name);
};

//------------------------------------------------------------
//  .pinesy files
//------------------------------------------------------------

struct Document
{
	static constexpr int format_version = 1;

	std::string machine;                 // the machine's driver; empty until chosen
	std::vector<std::string> arguments;  // the MAME command line

	// Reads a .pinesy file's text. nullopt when it is not JSON with
	// format, machine and arguments, its format is newer than this
	// core reads, or its arguments are empty.
	static std::optional<Document> read(std::string_view text);

	// The file's text.
	std::string write() const;
};

//------------------------------------------------------------
//  Settings, paths and runs
//------------------------------------------------------------

// What the front end tells the core about its platform.
struct Platform
{
	// The folder with the bundled ROM sets ("roms") and MAME's search
	// folders. macOS: the bundle's Resources folder.
	std::string resources_folder;
	// The data folder when the settings name none. macOS:
	// ~/Library/Application Support/Pinesy. Linux: $XDG_DATA_HOME/pinesy.
	std::string default_data_folder;
};

// The settings schema: what is the same for every machine. Settings
// are added to the command line at run time and are not stored in
// configurations. The front end stores them.
struct Settings
{
	enum class Debugger { windows, gdb_stub };

	static constexpr int default_gdb_port = 23946;
	static constexpr int min_gdb_port = 1;
	static constexpr int max_gdb_port = 65535;

	std::vector<std::string> rom_folders;
	std::string data_folder;   // empty: the platform's default
	Debugger debugger = Debugger::windows;
	int gdb_port = default_gdb_port;
};

namespace paths {

// The user's ROM folder in a data folder, searched after the bundled ROMs.
std::string user_roms(const std::string &data_folder);

// False for a folder MAME cannot take in its ROM path (";" in the path).
bool is_usable_rom_folder(std::string_view path);

} // namespace paths

// "-debug" and the debugger options from the settings; empty without debug.
std::vector<std::string> debug_arguments(const Settings &settings, bool debug);

// A string quoted for zsh; plain strings stay as they are.
std::string shell_quoted(std::string_view text);

namespace headless {

// True when the last -video option is "none", as in MAME.
bool video_none(const std::vector<std::string> &arguments);

// True when the command line asks for the debugger and does not pick
// one without windows (none or gdbstub). The last -debugger wins.
bool opens_debugger_windows(const std::vector<std::string> &arguments);

} // namespace headless

// The core: a platform, the settings, and the catalogue, built from
// the engine on first use.
class Core
{
public:
	explicit Core(Platform platform);

	const Platform &platform() const { return m_platform; }
	const Settings &settings() const { return m_settings; }
	void set_settings(Settings settings) { m_settings = std::move(settings); }

	// The data folder in use: the settings' folder, else the default.
	std::string data_folder() const;

	// The ROM folders, in search order: the bundled ROMs, the user's
	// ROM folder, then the folders from the settings. A folder with
	// ";" in its path is left out.
	std::vector<std::string> rom_folders() const;

	// The options every run starts with: -noreadconfig, -rompath, the
	// search paths in the resources folder, the folders MAME writes
	// in the data folder, and -skip_gameinfo.
	std::vector<std::string> fixed_options() const;

	// Creates the user's ROM folder and the folders MAME writes.
	void create_folders() const;

	// Asks MAME about a command line, with the fixed options first.
	MachineDescription describe(const std::vector<std::string> &arguments) const;
	Describer describer() const;

	const Catalogue &catalogue();

	// The full command line of a run: fixed options, the
	// configuration, and the debugger options.
	std::vector<std::string> run_command_line(const std::vector<std::string> &arguments, bool debug) const;

	// Creates the folders and runs a configuration. Returns MAME's exit code.
	int run(const std::vector<std::string> &arguments, bool debug, const pinesy_host *host) const;

	// The headless command for a configuration, quoted for zsh.
	std::string copy_command_line(const std::string &executable, const std::vector<std::string> &arguments, bool debug) const;

	// Headless mode: the executable's arguments, without its own name.
	// A leading "--" is dropped. Creates the folders and runs MAME with
	// the fixed options first. Returns MAME's exit code.
	int headless(std::vector<std::string> arguments, const pinesy_host *host) const;

private:
	Platform m_platform;
	Settings m_settings;
	std::optional<Catalogue> m_catalogue;
};

} // namespace pinesy::core

#endif // PINESY_CORE_HPP
