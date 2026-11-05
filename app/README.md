# Pinesy.app

The macOS app for Pi/Ne-Sy. It is a SwiftUI document app that links `libmame_pinesy.dylib`, the MAME library built from `../mame` with the `pinesy` OSD in `../engine`. The C interface is `../engine/include/pinesy.h`.

## Build

Build `../mame/libmame_pinesy.dylib` first, then:

```sh
app/build.sh [output-dir]
```

The default output directory is `app/build`. `MAME_LIB=<path>` bundles a different library. The script needs Xcode's command line tools (`swiftc`, `install_name_tool`, `codesign`) and nothing else.

The script copies the MAME library and every library it loads from outside `/usr/lib` and `/System` (from Homebrew: glib, libintl and pcre2) into `Contents/Frameworks`. It rewrites their install names to `@rpath/<name>` and removes their search paths. The executable searches `@executable_path/../Frameworks`. The build fails if a binary in the bundle still loads a library from outside it. The bundle is signed ad hoc.

## Configurations

A configuration is a document: a machine, what is fitted to it, its ROM and the media in its drives. It is saved as a `.pinesy` file, a JSON object with three keys:

- `format`: the file format version, now 1.
- `machine`: the driver that stands for the machine in the machine list.
- `arguments`: the MAME command line the configuration runs, as an array of strings. The first string is the driver to run, then options, such as `-exp intf1,bios=v1` or `-pi /path/nextpi.img`. An empty value, as in `-rs232_rpi ""`, leaves a slot empty. A string that starts with `-` is always an option name, never a value, so a flag such as `-skip_gameinfo` stays a flag. Strings that are not options stay where they are.

File > New (⌘N) opens a window and asks for the machine. A launch with no document to open does the same. The configuration window shows the machine, its model and ROM, its hardware and its media, and the command line. Every change is checked with MAME, and an option the machine no longer has is removed. When MAME rejects the driver itself, no option is removed. When MAME rejects the command line, the window shows MAME's error text and Run is not available. MAME's warnings are shown too, and do not stop a run. A media file that is not there is marked on its row.

## The machine list

The list is built from MAME's own data at launch (`pinesy_list_machines()`). The app names no driver:

- An unofficial driver (an after-market change to a released machine, such as the +3e ROMs) is a ROM choice of its machine. Its machine is the driver's parent, or the official driver in the same source file with the same devices.
- A driver and its clones that share one name before a colon, such as "ZX Spectrum Next: KS1", are one machine with a model choice. The parent of that set is MAME's reference for the models and is not offered as a model; on the Next it is the machine that reports the emulator machine ID, which headless runs can still use as `tbblue`. A new configuration uses the newest model.
- Machines are grouped by their first maker. A clone joins its parent's group when they share a maker. Makers with one product are listed under "Other makers".

Search finds machines by name, maker, model or ROM variant name.

## Starting configuration

A new configuration starts as MAME's default for its driver, with one change. A port where MAME fits the same card on every model of the machine's MAME family is left empty, because no model of that machine came with that card: the Kempston joystick interface on the ZX Spectrum and its clones, and the Pi and MIDI ports of the Next. A card that MAME fits on some models and not others stays, because it records what each model has: the Next's ESP WiFi module is fitted on the KS2 and KS3 and not on the KS1. Memory, sound chips and disk drives keep MAME's setting.

The library reports each slot with a kind, so the app can tell these apart: `memory` (the cards are RAM), `drive` (a floppy drive connector), `sound` (the cards make sound and hold no media) or `port`. The kind comes from the slot's default card, or, for a slot without one, from all the cards it takes. It does not change with the card fitted, so an empty slot keeps its name.

## Running

Machine > Run (⌘R) runs the front configuration in its own window, and Machine > Run with Debugger (⌥⌘R) adds `-debug`. The window takes the size and shape MAME asks for. One machine runs at a time; while it runs, configurations cannot be changed, and a configuration opened then waits for the run to end. Machine > Stop (⌘.) or closing the window stops the machine, also while it is still starting. When MAME stops with an error, an alert shows MAME's error lines.

Settings:

- General: extra ROM folders, and the folder where MAME keeps its files (`~/Library/Application Support/Pinesy` by default).
- Debugger: MAME's debugger windows, or the GDB remote stub (`-debugger gdbstub -debugger_port <port>`, port 23946 by default, from 1 to 65535).

Machine > Copy Command Line copies the headless command for the configuration, quoted for zsh. Headless mode adds the paths below itself.

## Headless

Any arguments make the executable run MAME with them and no UI. A leading `--` is dropped. The exit code is MAME's. MAME shows no machine window here. With `-debug` and MAME's debugger windows (no `-debugger none` or `-debugger gdbstub`), the process runs as a regular app so the debugger windows can open. Headless mode does not read `.pinesy` files; the `arguments` array of one is a command line to pass.

```sh
Pinesy.app/Contents/MacOS/Pinesy -- tbblue -video none -sound none -seconds_to_run 10
```

## ROMs and MAME's files

Pinesy is not MAME: it reads no `mame.ini` and uses nothing in `~/.mame`. Every run, headless or not, starts with these options, and the caller's own options come after them, so any of them can be overridden for one run (`-cfg_directory <dir>`, for example):

- `-noreadconfig`: no MAME ini file is read.
- `-rompath`: the ROM sets in the bundle (`Contents/Resources/roms`), then `roms` in the data folder from Settings (`~/Library/Application Support/Pinesy/roms` by default), then the ROM folders in Settings. MAME separates these with `;`, so Settings does not take a folder with `;` in its path.
- MAME's other search paths, in `Contents/Resources`: `hash`, `samples`, `artwork`, `ctrlr`, `fonts`, `cheat`, `crosshair`, `plugins`, `language`, `software` and `bgfx`.
- The folders MAME writes, in the data folder from Settings: `ini` (`inipath`), `home` (`homepath`), `cfg`, `nvram`, `inp`, `sta`, `snap`, `diff`, `comments` and `share`.

`build.sh` copies the app icon, `Pinesy.icns`, into `Contents/Resources`. It copies `../roms` into the bundle. Each entry there must be a ROM set: a zip, a 7z or a folder of ROM dumps. The script stops on anything else, and on a disk image (SD card, hard disk or floppy) loose in a folder or inside a zip. It cannot list a 7z, so it does not check what one holds. It also copies MAME's `plugins`, `hash`, `bgfx`, `artwork` and `language` folders from `../mame`; the other search paths have no folder in the bundle.

`Pinesy -- -showconfig` prints every path a run uses.
