# Pinesy.app

The macOS app for Pi/Ne-Sy. It is a SwiftUI document app that links `libmame_pinesy.dylib`, the MAME library built from `../mame` with the `pinesy` OSD. The C interface is `../mame/src/osd/pinesy/pinesy.h`.

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
- `arguments`: the MAME command line the configuration runs, as an array of strings. The first string is the driver to run, then options in pairs, such as `-exp intf1,bios=v1` or `-pi /path/nextpi.img`. An empty value, as in `-rs232_rpi ""`, leaves a slot empty.

File > New Configuration (⌘N) opens a window and asks for the machine. The configuration window shows the machine, its model and ROM, its hardware and its media, and the command line. Every change is checked with MAME, and an option the machine no longer has is removed. When MAME rejects the command line, the window shows MAME's error text and Run is not available.

## The machine list

The list is built from MAME's own data at launch (`pinesy_list_machines()`). The app names no driver:

- An unofficial driver (an after-market change to a released machine, such as the +3e ROMs) is a ROM choice of its machine. Its machine is the driver's parent, or the official driver in the same source file with the same devices.
- A driver and its clones that share one name before a colon, such as "ZX Spectrum Next: KS1", are one machine with a model choice. The parent of that set is MAME's reference for the models and is not offered as a model; on the Next it is the machine that reports the emulator machine ID, which headless runs can still use as `tbblue`. A new configuration uses the newest model.
- Machines are grouped by their first maker. A clone joins its parent's group when they share a maker. Makers with one product are listed under "Other makers".

Search finds machines by name, maker or driver name.

## Starting configuration

A new configuration starts as MAME's default for its driver, with one change. A port where MAME fits the same card on every model of the machine's MAME family is left empty, because no model of that machine came with that card: the Kempston joystick interface on the ZX Spectrum and its clones, and the Pi and MIDI ports of the Next. A card that MAME fits on some models and not others stays, because it records what each model has: the Next's ESP WiFi module is fitted on the KS2 and KS3 and not on the KS1. Memory, sound chips and disk drives keep MAME's setting.

The library reports each slot with a kind, so the app can tell these apart: `memory` (the card is RAM), `drive` (a floppy drive connector), `sound` (the card makes sound and holds no media) or `port`.

## Running

Machine > Run (⌘R) runs the front configuration in its own window, and Machine > Run with Debugger (⌥⌘R) adds `-debug`. One machine runs at a time; while it runs, configurations cannot be changed. Machine > Stop (⌘.) or closing the window stops the machine. When MAME stops with an error, an alert shows MAME's error lines.

Settings adds options to every run:

- General: ROM folders (`-rompath`), and the folder for MAME's machine settings, memory contents and snapshots (`-cfg_directory`, `-nvram_directory` and `-snapshot_directory`, as `cfg`, `nvram` and `snap` in `~/Library/Application Support/Pinesy` by default).
- Debugger: MAME's debugger windows, or the GDB remote stub (`-debugger gdbstub -debugger_port <port>`, port 23946 by default).

Machine > Copy Command Line copies the headless command for the configuration, with these options, quoted for zsh.

## Headless

Any arguments make the executable run MAME with them and no UI. A leading `--` is dropped. Every option goes to MAME unchanged, and the exit code is MAME's. Headless mode does not read `.pinesy` files; the `arguments` array of one is a command line to pass.

```sh
Pinesy.app/Contents/MacOS/Pinesy -- tbblue -video none -sound none -seconds_to_run 10
```

## ROMs and settings

MAME looks for `mame.ini` in `~/Library/Application Support/mame`, then `~/.mame`, then the current directory. ROMs come from the ROM folders in Settings; without any, from the `rompath` that `mame.ini` sets, or from `roms` in the current directory. Options on the command line take precedence over `mame.ini`.
