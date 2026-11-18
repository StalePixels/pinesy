# Pinesy.app

The macOS app for Pi/Ne-Sy. It is a SwiftUI document app. It links `libpinesy.dylib`, the core in `../core`, and `libmame_pinesy.dylib`, the MAME library built from `../mame` with the `pinesy` OSD in `../engine`. The core holds every rule this README describes, the same rules for the GNOME and KDE apps; the Swift code holds only the user interface. Swift imports the core's C interface, `../core/include/pinesy_core.h`, and the engine's, `../engine/include/pinesy.h`, through the module map in `Sources/CPinesy`.

To build it, see [docs/build-macos.md](../docs/build-macos.md).

## Configurations

A configuration is a document: a machine, what is fitted to it, its ROM and the media in its drives. It is saved as a `.pinesy` file, a JSON object with four keys:

- `format`: the file format version, now 1.
- `machine`: the driver that stands for the machine in the machine list.
- `arguments`: the MAME command line the configuration runs, as an array of strings. The first string is the driver to run, then options, such as `-exp intf1,bios=v1` or `-pi /path/nextpi.img`. An empty value, as in `-rs232_rpi ""`, leaves a slot empty. A string that starts with `-` is always an option name, never a value, so a flag such as `-skip_gameinfo` stays a flag. Strings that are not options stay where they are.
- `windowSize`: the machine window's size. `-1` is Full screen, `0` is Fit to screen, and a whole number of 1 or more is that magnification of the frame. A file without this key, or with another value, opens as Fit to screen.

File > New (⌘N) opens a window and asks for the machine. A launch with no document to open does the same. The configuration window shows the machine, its model and ROM, its window size, its hardware and its media, and the command line. Every change is checked with MAME, and an option the machine no longer has is removed. When MAME rejects the driver itself, no option is removed. When MAME rejects the command line, the window shows MAME's error text and Run is not available. MAME's warnings are shown too, and do not stop a run. A media row with a problem shows it in red under the row's name: a file the machine needs and does not have, a file that is not found (the row shows its full path), or an SD card image that MAME's SD card cannot use. MAME takes SD cards of up to 1 GiB, and SDHC cards of up to 32 GiB that are a multiple of 512 KiB. A CHD is not checked. Choose refuses such an SD card image and says why, and a configuration that holds one cannot run. Each media row shows the file extensions MAME lists for the device, but any file can be chosen, because MAME also opens others, such as `.img` SD card images.

## The machine list

The core builds the list from MAME's own data (`pinesy_list_machines()`) the first time the app asks for it. No driver is named in the rules:

- An unofficial driver (an after-market change to a released machine, such as the +3e ROMs) is a ROM choice of its machine. Its machine is the driver's parent, or the official driver in the same source file with the same devices.
- A driver and its clones that share one name before a colon, such as "ZX Spectrum Next: KS1", are one machine with a model choice. The parent of that set is MAME's reference for the models and is not offered as a model; on the Next it is the machine that reports the emulator machine ID, which headless runs can still use as `tbblue`. A new configuration uses the newest model.
- Machines are grouped by their first maker. A clone joins its parent's group when they share a maker. Makers with one product are listed under "Other makers". Byte (`sinclair/byte.cpp`) has its own entry.

Search finds machines by name, maker, model or ROM variant name.

## Starting configuration

A new configuration starts as MAME's default for its driver, with one change. A port where MAME fits the same card on every model of the machine's MAME family is left empty, because no model of that machine came with that card: the Kempston joystick interface on the ZX Spectrum and its clones, and the Pi and MIDI ports of the Next. A card that MAME fits on some models and not others stays, because it records what each model has: the Next's ESP WiFi module is fitted on the KS2 and KS3 and not on the KS1. Memory, sound chips and disk drives keep MAME's setting.

MIDI ports on the machine itself have no row in the window. MAME gives them to the Next for the UART of its joystick ports, which has no MIDI socket. A card's MIDI ports, such as those of the Music Machine, have rows.

The library reports each slot with a kind, so the app can tell these apart: `memory` (the cards are RAM), `drive` (a floppy drive connector), `sound` (the cards make sound and hold no media) or `port`. The kind comes from the slot's default card, or, for a slot without one, from all the cards it takes. It does not change with the card fitted, so an empty slot keeps its name.

## Running

Machine > Run (⌘R) runs the front configuration in its own window, and Machine > Run with Debugger (⌥⌘R) adds `-debug`. One machine runs at a time; while it runs, configurations cannot be changed, and a configuration opened then waits for the run to end. Machine > Stop (⌘.) or closing the window stops the machine, also while it is still starting. Machine > Reset resets the running machine and keeps its RAM, as MAME's F3 does; Machine > Hard Reset starts it again from power-on, as Shift+F3 does. These menu items work also on keyboards with no Scroll Lock key, which MAME needs to turn on its own UI keys. When MAME stops with an error, an alert shows MAME's error lines.

Settings:

- General: extra ROM folders, and the folder where MAME keeps its files (`~/Library/Application Support/Pinesy` by default).
- Debugger: MAME's debugger windows, or the GDB remote stub (`-debugger gdbstub -debugger_port <port>`, port 23946 by default, from 1 to 65535).

Machine > Copy Command Line copies the headless command for the configuration, quoted for zsh. Headless mode adds the paths below itself.

## The machine window

MAME renders each frame in software at a fixed size: the visible area of the machine's first screen times a whole number on each axis, so each machine pixel is the same number of frame pixels wide and high. Of the multiples that fit in 800 x 900, it takes the one whose shape is closest to MAME's view of the machine, and of equal ones the smaller (the rule of Holub's emame). On the Next, 720 x 288 becomes 720 x 576, a multiple of 1 across and 2 down. The picture fills the frame, as with MAME's `-nokeepaspect`, so no machine pixel is squeezed to MAME's 4:3 view. MAME's video options menu can still turn Keep Aspect on for the run.

The frame size is in device pixels. The window's content is the frame times a whole number, divided by the screen's pixels per point, so at 1x one frame pixel is one device pixel. The configuration's window size chooses the number: Fit to screen, the default, takes the largest that fits the screen's usable area below the title bar; a magnification takes that one, or the largest that fits when it does not. When even 1x does not fit, the window is the largest size of the frame's shape that fits. The window takes this size when the machine starts and again when the frame's size changes, as with a new screen mode. Full screen opens the window at the Fit to screen size and then puts it in macOS full screen. The user leaves full screen in the usual macOS ways, and the window then has the Fit to screen size for the current frame. While the window is in full screen, a new frame size does not change it. A machine window in headless mode uses Fit to screen. On a screen whose pixels per point is not a whole number, the window can be one device pixel off a whole multiple; the picture is then smoothed.

The configuration window's Window size picker offers Fit to screen, Full screen, 1x to 4x and Custom. Custom shows a Magnification field for any whole number of 1 or more. A configuration opens with 1x to 4x as those entries, and with any larger magnification as Custom with its number. The core holds these choices (`pinesy_core_window_choice()`, `pinesy_core_window_size_for_choice()`), so the three apps offer the same ones.

The window keeps the frame's shape when it is resized. Where the view's shape differs, as in full screen, the frame is drawn at its own shape, centred, with black bars. It is sharp (nearest-neighbour) where the drawn size is a whole multiple of the frame, and smoothed at any other size.

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
- `-skip_gameinfo`: MAME does not show its machine information screen at start, because the machine was chosen in the app.

`build.sh` compiles the core's sources in `../core/src` with `clang++` into `libpinesy.dylib`, and puts it in `Contents/Frameworks` with `libmame_pinesy.dylib` and the libraries that one needs. It copies the app icon, `Pinesy.icns`, and the icon of `.pinesy` files, `PinesyDocument.icns`, into `Contents/Resources`. The `.pinesy` type conforms to `public.data`, not `public.json`: Finder shows a JSON type's Quick Look text preview in place of its icon. It copies `../roms` into the bundle. Each entry there must be a ROM set: a zip, a 7z or a folder of ROM dumps. The script stops on anything else, and on a disk image (SD card, hard disk or floppy) loose in a folder or inside a zip. It cannot list a 7z, so it does not check what one holds. It also copies MAME's `plugins`, `hash`, `bgfx`, `artwork` and `language` folders from `../mame`; the other search paths have no folder in the bundle.

`Pinesy -- -showconfig` prints every path a run uses.
