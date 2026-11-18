// license:BSD-3-Clause
// copyright-holders:D. Rimron-Soutter
//============================================================
//
//  pinesy_core.h - C interface to libpinesy, the shared core
//  of the pinesy front ends
//
//  The core holds every rule the front ends share: the
//  machine catalogue, configurations and .pinesy files, the
//  starting configuration, option checking, MAME's paths, the
//  settings schema, the copied command line and headless
//  mode. It calls the engine, libmame_pinesy (pinesy.h).
//
//  Conventions:
//
//  - All text is UTF-8.
//  - A command line ("arguments") is a JSON array of strings:
//    the driver, then options, as in a .pinesy file. It has no
//    fixed options; the core adds them.
//  - A function that returns char * returns text the caller
//    owns and frees with pinesy_core_free(). It returns NULL
//    when an argument is NULL or is not the JSON it must be.
//  - JSON field names are MAME's for MAME's data (slots,
//    media, BIOS sets, as pinesy.h gives them) and camelCase
//    for the core's own objects.
//  - Calls that ask the engine run on the main thread, and
//    never while a machine runs: pinesy_describe() must not
//    run inside a running MAME. These are marked "Asks the
//    engine".
//
//============================================================

#ifndef PINESY_CORE_H
#define PINESY_CORE_H

#include "pinesy.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct pinesy_core pinesy_core;

// The GDB stub port a run uses when the settings name none, and
// the range a port must be in.
#define PINESY_CORE_DEFAULT_GDB_PORT 23946
#define PINESY_CORE_MIN_GDB_PORT 1
#define PINESY_CORE_MAX_GDB_PORT 65535

//------------------------------------------------------------
//  The core
//------------------------------------------------------------

// Makes a core. The two folders are the platform's part:
//
// resources_folder holds the bundled ROM sets (its "roms" folder)
// and MAME's search folders (hash, samples, artwork, ctrlr, fonts,
// cheat, crosshair, plugins, language, software, bgfx). On macOS it
// is the bundle's Resources folder; on Linux the front end chooses.
//
// default_data_folder is where MAME's files go when the settings
// name no data folder: ~/Library/Application Support/Pinesy on
// macOS, $XDG_DATA_HOME/pinesy (by default ~/.local/share/pinesy)
// on Linux.
//
// The core starts with the default settings. Returns NULL when a
// folder is NULL.
pinesy_core *pinesy_core_create(const char *resources_folder, const char *default_data_folder);

// Frees a core. NULL is allowed.
void pinesy_core_destroy(pinesy_core *core);

// Frees text the core returned. NULL is allowed.
void pinesy_core_free(char *text);

//------------------------------------------------------------
//  Settings: what is the same for every machine
//------------------------------------------------------------

// Gives the core the user's settings, as a JSON object:
//
//   romFolders  array of folder paths, searched after the bundled
//               ROMs and the data folder's "roms"
//   dataFolder  the folder where MAME keeps its files; "" for the
//               platform's default
//   debugger    "windows" (MAME's debugger windows) or "gdbStub"
//               (the GDB remote stub)
//   gdbPort     the GDB stub's port
//
// The front end stores the settings; the core reads them only from
// here. A missing or unusable value takes its default: no ROM
// folders, the default data folder, "windows", and
// PINESY_CORE_DEFAULT_GDB_PORT for a port outside
// PINESY_CORE_MIN_GDB_PORT to PINESY_CORE_MAX_GDB_PORT. Returns 0,
// or -1 when the text is not a JSON object.
int pinesy_core_set_settings(pinesy_core *core, const char *settings_json);

// The settings in use, as the JSON object pinesy_core_set_settings()
// takes, with every value filled in.
char *pinesy_core_settings(const pinesy_core *core);

// The data folder in use: the settings' folder, else the default.
char *pinesy_core_data_folder(const pinesy_core *core);

// The user's ROM folder in a data folder: its "roms" folder.
char *pinesy_core_user_rom_folder(const char *data_folder);

// 1 when a folder can be a ROM folder, 0 when it cannot. MAME
// separates ROM folders with ";", so a path with ";" cannot be one.
int pinesy_core_is_usable_rom_folder(const char *path);

//------------------------------------------------------------
//  MAME's paths
//------------------------------------------------------------

// The options every run and every check starts with, as a JSON
// array of strings: -noreadconfig; -rompath with the ROM folders;
// MAME's search paths in the resources folder; and the folders MAME
// writes in the data folder (inipath ini, homepath home,
// cfg_directory cfg, nvram_directory nvram, input_directory inp,
// state_directory sta, snapshot_directory snap, diff_directory
// diff, comment_directory comments, share_directory share); and
// -skip_gameinfo, so MAME does not show the machine information
// screen at start. The caller's own options come after these, so any of them can be
// overridden for one run.
char *pinesy_core_fixed_options(const pinesy_core *core);

// The ROM folders in search order, as a JSON array: the bundled ROMs
// (the resources folder's "roms"), the data folder's "roms", then
// the settings' ROM folders. A folder with ";" in its path is left
// out.
char *pinesy_core_rom_folders(const pinesy_core *core);

// Creates the data folder's "roms" folder and the folders MAME
// writes. pinesy_core_run() and pinesy_core_headless() do this
// themselves.
void pinesy_core_create_folders(const pinesy_core *core);

//------------------------------------------------------------
//  The machine catalogue
//
//  A machine is a JSON object:
//
//    id            the driver that stands for the machine: the
//                  family parent when it has models, else its own
//                  driver. A document's "machine" is this id.
//    name          "ZX Spectrum Next" for the Next's models
//    manufacturer  MAME's manufacturer field
//    year          the first model's year, else the driver's
//    preliminary   true when the newest model is preliminary
//    defaultDriver the driver a new configuration runs: the
//                  newest model, else the id
//    models        released models in year order: driver, label
//                  ("KS1") and year. Empty for a machine without
//                  models.
//    otherModels   models MAME has that are not offered (the
//                  family parent); a configuration that names one
//                  still opens
//    variants      ROM choices that run another driver: driver,
//                  name and biossets
//
//  The catalogue is built from pinesy_list_machines() the first
//  time a catalogue call needs it, and kept. Asks the engine.
//------------------------------------------------------------

// The machine list as a JSON array of sections, each with name,
// showsMaker (true for "Other makers", whose rows also show the
// maker) and machines. Sections are in year order, "Other makers"
// last; machines in each section are in year order, then by name.
char *pinesy_core_catalogue(pinesy_core *core);

// The machines whose name, maker, model or ROM variant name
// contains the text, ignoring case, as a JSON array. Spaces and tabs
// around the text are ignored; empty text finds nothing.
char *pinesy_core_search(pinesy_core *core, const char *text);

// The machine a driver belongs to (as its id, a model or a ROM
// variant), as a JSON object, or the JSON text null.
char *pinesy_core_machine_for_driver(pinesy_core *core, const char *driver);

// The command line a new configuration for a driver starts with:
// the driver, then "-<port>", "" for each port MAME fills with the
// same card on every model of the driver's MAME family.
char *pinesy_core_starting_arguments(pinesy_core *core, const char *driver);

//------------------------------------------------------------
//  Configurations
//
//  Each edit returns the new command line. Every edit except
//  pinesy_core_set_media() then checks it with MAME and removes
//  the options the machine no longer has.
//------------------------------------------------------------

// A new configuration for a machine (its id): its newest model, as
// it shipped. NULL when the catalogue has no machine with that id.
char *pinesy_core_new_configuration(pinesy_core *core, const char *machine);

// Runs another driver of the same machine (a model or a ROM
// variant), keeping every choice the new driver also has. A port the
// old driver left empty because it starts empty follows the new
// driver's start. The ROM ("-bios") is kept when the new driver has
// it. Asks the engine.
char *pinesy_core_set_driver(pinesy_core *core, const char *arguments, const char *driver);

// Chooses a ROM: a BIOS set of the machine's driver, or of a variant
// driver. bios NULL, or the driver's default set, removes "-bios".
// Asks the engine.
char *pinesy_core_set_rom(pinesy_core *core, const char *arguments, const char *driver, const char *bios);

// Fits a card in the slot with a tag; "" leaves the slot empty. The
// option is removed when the card is MAME's default for the slot and
// the slot does not start empty. NULL when MAME's description of the
// command line has no slot with that tag. Asks the engine.
char *pinesy_core_set_slot(pinesy_core *core, const char *arguments, const char *slot, const char *card);

// Chooses the ROM of the card fitted in the slot with a tag
// ("-<slot> <card>,bios=<bios>"). NULL when MAME's description of
// the command line has no slot with that tag. Asks the engine.
char *pinesy_core_set_card_rom(pinesy_core *core, const char *arguments, const char *slot, const char *bios);

// Puts a file in a media device (the media's "name", such as "cass");
// path NULL empties it. Not checked with MAME.
char *pinesy_core_set_media(const char *arguments, const char *media, const char *path);

// Why MAME's SD card cannot use the image at path, as text; "" when it
// can, and for a CHD or a file that is not there.
char *pinesy_core_sd_card_problem(const char *path);

// The command line with every option MAME rejects removed. Options
// are added back in their order, so a card's own slots and media are
// kept once the card is fitted. When MAME rejects the driver itself,
// the command line stays as it is. Asks the engine.
char *pinesy_core_normalise(pinesy_core *core, const char *arguments);

// Checks a configuration with MAME and returns what the configuration
// window shows, as a JSON object. machine_hint is the document's
// machine, used when the catalogue does not have the driver; it may
// be NULL. Asks the engine.
//
//   machine      the machine (as in the catalogue), or null
//   driver       the driver the command line runs
//   year, preliminary   of that driver, else of the machine
//   models       the models offered, and the family parent when the
//                command line runs it
//   roms         the ROMs of the running model: driver, bios, label
//   variantROMs  the ROMs of the machine's variants; bios is null for
//                a variant without BIOS sets
//   currentROM   the entry of roms or variantROMs in use, or null
//   hardware     one row per slot shown: slot (MAME's slot object),
//                label ("Memory", "Sound chip", "Disk drive 1", or
//                MAME's slot type, numbered when there are several),
//                depth (how many cards it is inside), choices (value
//                and label; "None" first except for memory), fixedText
//                (the fitted card for a slot that cannot be changed,
//                else null), meantFor (for one of several ports of a
//                kind, the card MAME made it for, else null), roms (the
//                fitted card's ROMs when it has more than one) and
//                currentROM
//   media        one row per media device: media (MAME's media
//                object, with extensionList: its extensions as an
//                array, without dots) and label
//   errors       MAME's error text; "" when there is none
//   warnings     MAME's warning text, which does not stop a run
//   missingMedia labels of media MAME needs that have no file
//   missingFiles media names whose file is not there
//   sdCardProblems  for SD card media with an image MAME's SD card
//                cannot use: an object of media name to the reason
//   canRun       true when there are no errors, no missingMedia and
//                no sdCardProblems
char *pinesy_core_summary(pinesy_core *core, const char *arguments, const char *machine_hint);

//------------------------------------------------------------
//  .pinesy files
//------------------------------------------------------------

// Reads a .pinesy file's text. Returns a JSON object with machine
// and arguments, or NULL when the text is not a configuration this
// core reads: not a JSON object with format (a whole number),
// machine (a string) and arguments (an array of strings), a format
// newer than 1, or empty arguments.
char *pinesy_core_read_document(const char *text);

// The text of a .pinesy file: a JSON object with format 1, the
// machine and the arguments, keys in order, indented.
char *pinesy_core_write_document(const char *machine, const char *arguments);

//------------------------------------------------------------
//  Runs
//------------------------------------------------------------

// The options a run with the debugger adds, from the settings, as a
// JSON array: "-debug", and with the GDB stub "-debugger", "gdbstub",
// "-debugger_port", "<port>". Empty when debug is 0.
char *pinesy_core_debug_arguments(const pinesy_core *core, int debug);

// The whole command line a run gives MAME, as a JSON array: the
// fixed options, the configuration, then the debugger options when
// debug is not 0.
char *pinesy_core_run_command_line(const pinesy_core *core, const char *arguments, int debug);

// Creates the folders and runs a configuration with
// pinesy_run(), with the command line pinesy_core_run_command_line()
// gives. host is passed to pinesy_run() and may be NULL. Returns
// MAME's exit code, or -1 when arguments is not a JSON array of
// strings. Main thread only.
int pinesy_core_run(const pinesy_core *core, const char *arguments, int debug, const pinesy_host *host);

// The headless command for a configuration, quoted for zsh, as
// Copy Command Line gives it: the executable, "--", the
// configuration, then the debugger options when debug is not 0.
// Headless mode adds the fixed options itself. A string of letters,
// digits and "-_./:=,+@%" stays as it is; any other string, the
// empty string included, is put in single quotes.
char *pinesy_core_copy_command_line(const pinesy_core *core, const char *executable, const char *arguments, int debug);

//------------------------------------------------------------
//  Headless mode
//
//  With any arguments, a front end's executable runs MAME with
//  them and no UI, and exits with MAME's exit code. argv is the
//  executable's arguments without its own name.
//------------------------------------------------------------

// 1 when the last "-video" option is "none", as in MAME: MAME then
// needs no window. Else 0.
int pinesy_core_video_none(int argc, const char *const argv[]);

// 1 when the command line asks for the debugger ("-debug" or "-d")
// and does not pick one without windows: the last "-debugger" is not
// "none" or "gdbstub". Else 0.
int pinesy_core_opens_debugger_windows(int argc, const char *const argv[]);

// Runs headless mode. A leading "--" is dropped. Creates the folders
// and runs pinesy_run() with the fixed options, then the arguments.
// host is passed to pinesy_run(): NULL with -video none, else the
// window the front end made for the machine. Returns MAME's exit
// code. Main thread only. Does not read .pinesy files.
int pinesy_core_headless(const pinesy_core *core, int argc, const char *const argv[], const pinesy_host *host);

#ifdef __cplusplus
}
#endif

#endif // PINESY_CORE_H
