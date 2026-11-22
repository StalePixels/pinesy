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
//  settings schema, the machine window's size, recent configurations,
//  the copied command line, the rules of the out-of-date notice and
//  headless mode. It calls the engine, libmame_pinesy (pinesy.h).
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

// The GDB stub ports a run uses when the settings name none (the
// Next's, and the Pi's), and the range a port must be in.
#define PINESY_CORE_DEFAULT_GDB_PORT 23946
#define PINESY_CORE_DEFAULT_PI_GDB_PORT 23947
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
//   startPaused true: in a run with the debugger, the Next waits at its
//               first instruction (the --paused switch)
//   piGdbPort   the port of the Pi's GDB stub (QEMU), for a run with
//               the debugger of a configuration with a Pi SD card
//   updateChecks
//               false: the app does not look for updates (Never remind
//               again)
//   ignoredBuild
//               the build the out-of-date notice ignores (Never remind
//               for this version); "" for none
//
// The front end stores the settings; the core reads them only from
// here. A missing or unusable value takes its default: no ROM
// folders, the default data folder, "windows", false,
// PINESY_CORE_DEFAULT_GDB_PORT and PINESY_CORE_DEFAULT_PI_GDB_PORT
// for a port outside PINESY_CORE_MIN_GDB_PORT to
// PINESY_CORE_MAX_GDB_PORT, true for updateChecks and "" for
// ignoredBuild. Returns 0, or -1 when the text is not a
// JSON object.
int pinesy_core_set_settings(pinesy_core *core, const char *settings_json);

// The settings in use, as the JSON object pinesy_core_set_settings()
// takes, with every value filled in.
char *pinesy_core_settings(const pinesy_core *core);

// The data folder in use: the settings' folder, else the default.
char *pinesy_core_data_folder(const pinesy_core *core);

// The user's ROM folder in a data folder: its "roms" folder.
char *pinesy_core_user_rom_folder(const char *data_folder);

// The resources folder of a Linux front end: <installed>, relative to
// the folder of the running program (/proc/self/exe), when that folder
// exists, as in an installed copy; otherwise build_folder. NULL when
// either argument is NULL.
char *pinesy_core_resources_folder(const char *installed, const char *build_folder);

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

// The folder MAME keeps NVRAM in for a configuration: the last
// -nvram_directory in its command line, else the data folder's
// "nvram", as pinesy_core_fixed_options() gives it.
char *pinesy_core_nvram_folder(const pinesy_core *core, const char *arguments);

// Deletes the NVRAM MAME saved for a configuration's driver: the
// entries in pinesy_core_nvram_folder() named as the driver, or as the
// driver, "_" and a number (MAME's name for the NVRAM of a ROM that is
// not the driver's default), files and folders alike. Nothing else is
// deleted. MAME writes NVRAM when a machine exits, so a front end
// does not call this while a machine runs. Returns 0 when every such
// entry is gone, also when there was none; -1 when one could not be
// deleted, or arguments is not a command line with a driver.
int pinesy_core_clear_nvram(const pinesy_core *core, const char *arguments);

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
//   mouse        true when the machine with this hardware has mouse
//                inputs (pinesy_describe() in pinesy.h). The
//                configuration window shows the mouse setting only
//                then, and only then does a run get a mouse option
//                (pinesy_core_run_command_line()).
//   piSDCard     the media name of the Pi's SD card ("pi") when the
//                command line names a file for it, else "". The
//                configuration window shows the Pi pause setting
//                next to that media row only then.
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
//  The machine window's size
//------------------------------------------------------------

// A configuration's window size, as a .pinesy file saves it in
// windowSize: PINESY_CORE_WINDOW_FULL_SCREEN, PINESY_CORE_WINDOW_FIT,
// or a whole-number magnification of the frame, 1 or more.
#define PINESY_CORE_WINDOW_FULL_SCREEN (-1)
#define PINESY_CORE_WINDOW_FIT 0

// The configuration window's choices for the window size: Fit to
// screen, Full screen, each magnification from 1x to
// PINESY_CORE_WINDOW_LISTED_MAGNIFICATIONS by name, and Custom for
// any other magnification. PINESY_CORE_WINDOW_CUSTOM stands for
// Custom; it is a choice, never a window size.
#define PINESY_CORE_WINDOW_LISTED_MAGNIFICATIONS 4
#define PINESY_CORE_WINDOW_CUSTOM (-2)

// 1 when window_size is a window size a configuration can have, 0
// when it is not.
int pinesy_core_is_window_size(int window_size);

// The choice that shows a window size: the window size itself for
// full screen, fit and a magnification from 1 to
// PINESY_CORE_WINDOW_LISTED_MAGNIFICATIONS; PINESY_CORE_WINDOW_CUSTOM
// for a larger magnification; PINESY_CORE_WINDOW_FIT for a value
// that is not a window size. With custom_chosen not 0 (the user has
// chosen Custom in this window), every magnification shows as
// Custom, so its number field stays while the user enters a number
// from 1 to PINESY_CORE_WINDOW_LISTED_MAGNIFICATIONS in it.
int pinesy_core_window_choice(int window_size, int custom_chosen);

// The window size a choice gives, from the configuration's current
// window size. Full screen, fit and a listed magnification give
// themselves. PINESY_CORE_WINDOW_CUSTOM keeps window_size when it is
// a magnification, and gives 1 when it is not; a number the user
// enters for Custom goes in as window_size. Any other choice gives
// PINESY_CORE_WINDOW_FIT.
int pinesy_core_window_size_for_choice(int choice, int window_size);

// 1 when the machine window opens in full screen for this window
// size, 0 when it opens at the size pinesy_core_window_size() gives.
int pinesy_core_window_full_screen(int window_size);

// The size of the machine window's content, in logical units, for a
// frame of frame_width x frame_height device pixels, as the engine's
// size callback gives it (pinesy.h). pixel_ratio is the screen's
// device pixels per logical unit. usable_width x usable_height is the
// room the window's content has on the screen, in logical units: the
// screen's usable area, without the window's title bar and frame.
//
// The content is the frame times a whole number. With
// PINESY_CORE_WINDOW_FIT it is the largest that fits the room. With a
// magnification it is that one, or the largest that fits when it
// does not. When even 1x does not fit, the content is the largest
// size of the frame's shape that fits. A usable size of 0 or less is
// a screen of unknown size: fit is then 1x, and a magnification is
// used as it is. A window_size of 0 or less is fit, so
// PINESY_CORE_WINDOW_FULL_SCREEN gives the size the window has when
// the user leaves full screen.
//
// Sets width and height. Returns the multiple used, 0 when even 1x
// does not fit, or -1 (with width and height 0) when a frame size is
// 0 or less, pixel_ratio is not more than 0, or an output is NULL.
int pinesy_core_window_size(int frame_width, int frame_height, int window_size, double pixel_ratio,
		int usable_width, int usable_height, int *width, int *height);

//------------------------------------------------------------
//  .pinesy files
//------------------------------------------------------------

// Reads a .pinesy file's text. Returns a JSON object with machine,
// arguments, windowSize, mouse and piPaused, or NULL when the text is not a
// configuration this core reads: not a JSON object with format (a
// whole number), machine (a string) and arguments (an array of
// strings), a format newer than 1, or empty arguments. windowSize is
// a window size (pinesy_core_is_window_size()); a file without it, or
// with a value that is not a window size, reads as
// PINESY_CORE_WINDOW_FIT. mouse is the mouse setting, true or false;
// a file without it, or with another value, reads as true, the
// default. piPaused is the Pi pause setting, true or false; a file
// without it, or with another value, reads as false, the default.
char *pinesy_core_read_document(const char *text);

// The text of a .pinesy file: a JSON object with format 1, the
// machine, the arguments, the window size, the mouse setting (mouse
// not 0 is true) and the Pi pause setting (pi_paused not 0 is true),
// keys in order, indented. A value that is not a window size is
// written as PINESY_CORE_WINDOW_FIT.
char *pinesy_core_write_document(const char *machine, const char *arguments, int window_size, int mouse,
		int pi_paused);

//------------------------------------------------------------
//  Runs
//------------------------------------------------------------

// The options a run of a configuration with the debugger adds, from
// the settings, as a JSON array: "-debug"; with the GDB stub
// "-debugger", "gdbstub", "-debugger_port", "<port>"; and when the
// configuration has a Pi SD card (piSDCard in the summary)
// "-pi_gdb_port", "<Pi port>". Empty when debug is 0.
char *pinesy_core_debug_arguments(const pinesy_core *core, const char *arguments, int debug);

// The whole command line a run gives MAME, as a JSON array: the
// fixed options, the configuration, the mouse option, then the
// debugger options when debug is not 0, then "-pi_gdb_wait" when the
// run waits for the Pi: a run with the debugger, pi_paused (the
// configuration's Pi pause setting) not 0, and a Pi SD card in the
// configuration (piSDCard in the summary). takes_mouse is the
// summary's mouse, and mouse the configuration's mouse setting. For a
// machine that takes a mouse, the mouse option is "-mouse" with the
// setting on and "-mouse_device none" with it off, so the setting
// turns the machine's mouse on and off. For any other machine there is
// none.
char *pinesy_core_run_command_line(const pinesy_core *core, const char *arguments, int debug, int takes_mouse,
		int mouse, int pi_paused);

// Creates the folders and runs a configuration with
// pinesy_run(), with the command line pinesy_core_run_command_line()
// gives, and with pinesy_set_start_paused() set from the settings'
// startPaused in a run with the debugger. host is
// passed to pinesy_run() and may be NULL. Returns MAME's exit code, or
// -1 when arguments is not a JSON array of strings. Main thread only.
int pinesy_core_run(const pinesy_core *core, const char *arguments, int debug, int takes_mouse, int mouse,
		int pi_paused, const pinesy_host *host);

// The headless command for a configuration, quoted for zsh, as
// Copy Command Line gives it: the executable, "--", the
// configuration, the mouse option, then the debugger options when
// debug is not 0. It has none of Pinesy's switches. takes_mouse
// and mouse are as for pinesy_core_run_command_line(). Headless mode
// adds the fixed options itself. A string of letters, digits and
// "-_./:=,+@%" stays as it is; any other string, the empty string
// included, is put in single quotes.
char *pinesy_core_copy_command_line(const pinesy_core *core, const char *executable, const char *arguments, int debug,
		int takes_mouse, int mouse);

//------------------------------------------------------------
//  Recent configurations
//
//  Each front end keeps its list of recent .pinesy files in the
//  platform's own store, newest first. A list of paths is a JSON
//  array of strings.
//------------------------------------------------------------

// The most recent configurations shown, and kept by a front end that
// keeps the list itself.
#define PINESY_CORE_RECENT_LIMIT 10

// The recent configurations to show, from the platform's list of
// paths, newest first: each path that names an existing file, once
// (the first time it is listed), and at most PINESY_CORE_RECENT_LIMIT
// of them. An empty path is left out. A JSON array of objects, each
// with path, name (the file's name without its extension) and folder
// (the folder the file is in).
char *pinesy_core_recent_configurations(const char *paths);

// The list of paths to keep after a configuration is opened or saved:
// path first, then the other paths in their order without it, at most
// PINESY_CORE_RECENT_LIMIT. An empty path leaves the list as it is.
char *pinesy_core_note_recent(const char *paths, const char *path);

// What a launch opens.
#define PINESY_CORE_LAUNCH_FILES 0
#define PINESY_CORE_LAUNCH_RECENT 1
#define PINESY_CORE_LAUNCH_NEW_CONFIGURATION 2

// What a launch opens: with files to open (files_to_open not 0), the
// files; with none, the recent configurations when recent_count (the
// entries of pinesy_core_recent_configurations()) is more than 0,
// otherwise a new configuration, which asks for the machine.
int pinesy_core_launch(int files_to_open, int recent_count);

// 1 when a configuration window is restored at the next launch: the
// configuration has a machine. 0 for a window with no machine, or when
// arguments is not a JSON array of strings.
int pinesy_core_restorable(const char *arguments);

//------------------------------------------------------------
//  Updates
//
//  A CI build carries its identity: its track ("ci", "nightly"
//  or "release"), its version, and its kind: "macos" (Sparkle
//  updates it), "flatpak" (the Flatpak portal) or "deb" (the
//  user updates it with the system's package manager). A local
//  build has none and never looks for updates. The front end
//  finds the newest build in its kind's own way, then asks the
//  core whether to show the out-of-date notice.
//------------------------------------------------------------

// The build's identity, as a JSON object with track, version and
// kind; an empty object for a local build.
char *pinesy_core_build_identity(void);

// Compares two versions as Sparkle does: -1 when a is older than b, 0
// when they are the same, 1 when a is newer. 0 when either is NULL.
int pinesy_core_compare_versions(const char *a, const char *b);

// 1 when the app looks for updates at startup: a CI build with
// updateChecks on in the settings. Else 0.
int pinesy_core_update_checks(const pinesy_core *core);

// 1 when the app shows the out-of-date notice for a build found on the
// server. The app looks for updates, the build is not the settings'
// ignoredBuild, and it is newer. With newer_known 0, build is a
// version, newer when pinesy_core_compare_versions() says so. With
// newer_known 1, the update tool has already found it newer: the
// Flatpak portal, where build is the remote commit. Else 0.
int pinesy_core_update_notice(const pinesy_core *core, const char *build, int newer_known);

// The choices of the out-of-date notice.
#define PINESY_CORE_UPDATE_NEVER_REMIND 0   // Never remind again
#define PINESY_CORE_UPDATE_IGNORE_BUILD 1   // Never remind for this version
#define PINESY_CORE_UPDATE_NOW 2            // Update

// Applies a choice to the core's settings and gives the settings, as
// pinesy_core_settings() does, for the front end to store:
// NEVER_REMIND turns updateChecks off, IGNORE_BUILD sets ignoredBuild
// to build, NOW changes nothing. NULL for another choice.
char *pinesy_core_update_choice(pinesy_core *core, int choice, const char *build);

// What the out-of-date notice and its follow-ups say, as a JSON object
// of strings: title, message, neverRemind, ignoreBuild and update (the
// notice and its buttons), packageManager (a "deb" build's answer to
// Update), newPermissions (the Flatpak portal refused an update that
// needs more permissions) and failed (the update failed; the update
// tool's error follows it). build and newer_known as for
// pinesy_core_update_notice(). NULL when build is NULL.
char *pinesy_core_update_texts(const char *build, int newer_known);

// Sparkle's appcast for the build's track, <server>/<track>/macos/appcast.xml.
// NULL for a local build.
char *pinesy_core_appcast_url(void);

// The apt Packages index for the build's track and the architecture
// it runs on, <server>/<track>/dists/sid/main/binary-<arch>/Packages.
// NULL for a local build.
char *pinesy_core_apt_packages_url(void);

// The newest version of a package in the text of an apt Packages
// index; NULL when the index does not list it.
char *pinesy_core_apt_newest_version(const char *packages, const char *package);

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

// Runs headless mode. With "--" among the arguments, the words before
// the first "--" are Pinesy's switches and the words after it are
// MAME's command line; without "--", every word is MAME's. The
// switches:
//
//   --paused     under the debugger (-debug), the Next waits at its
//                first instruction. Without it, Pinesy continues the
//                Next there, so it runs from the start.
//   --paused-pi  the Pi waits at its first instruction until a GDB
//                client or pinesy_continue() continues it: adds
//                "-pi_gdb_wait".
//
// Any other word before "--" prints an error and returns 6, MAME's
// code for a bad option. Creates the folders and runs pinesy_run()
// with the fixed options, then MAME's command line, then the options
// the switches add. host is passed to pinesy_run(): NULL with -video
// none, else the window the front end made for the machine. Returns
// MAME's exit code. Main thread only. Does not read .pinesy files.
int pinesy_core_headless(const pinesy_core *core, int argc, const char *const argv[], const pinesy_host *host);

#ifdef __cplusplus
}
#endif

#endif // PINESY_CORE_H
