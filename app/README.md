# Pinesy.app

The macOS app for Pi/Ne-Sy. It is a SwiftUI document app. It links `libpinesy.dylib`, the core in `../core`, and `libmame_pinesy.dylib`, the MAME library built from `../mame` with the `pinesy` OSD in `../engine`. The core holds every rule this README describes, the same rules for the GNOME and KDE apps; the Swift code holds only the user interface. Swift imports the core's C interface, `../core/include/pinesy_core.h`, and the engine's, `../engine/include/pinesy.h`, through the module map in `Sources/CPinesy`. The app uses the standard SwiftUI and AppKit controls of macOS 26, which take Liquid Glass themselves, and draws no glass of its own. Nothing covers the machine's picture.

To build it, see [docs/build-macos.md](../docs/build-macos.md).

## Configurations

A configuration is a document: a machine, what is fitted to it, its ROM and the media in its drives. It is saved as a `.pinesy` file, a JSON object with five keys:

- `format`: the file format version, 1.
- `machine`: the driver that stands for the machine in the machine list.
- `arguments`: the MAME command line the configuration runs, as an array of strings. The first string is the driver to run, then options, such as `-exp intf1,bios=v1` or `-pi /path/nextpi.img`. An empty value, as in `-rs232_rpi ""`, leaves a slot empty. A string that starts with `-` is always an option name, never a value, so a flag such as `-skip_gameinfo` stays a flag. Strings that are not options stay where they are.
- `windowSize`: the machine window's size. `-1` is Full screen, `0` is Fit to screen, and a whole number of 1 or more is that magnification of the frame. A file without this key, or with another value, opens as Fit to screen.
- `mouse`: the mouse setting, `true` or `false` (see "The mouse"). A file without this key, or with another value, opens with it on.
- `piPaused`: the Pi pause setting, `true` or `false` (see "The debugger"). A file without this key, or with another value, opens with it off.

File > New (⌘N) opens a window and asks for the machine. A launch with no document to open does the same when there is no recent configuration. When there are recent configurations, the launch shows them in their own window instead: the files of File > Open Recent that still exist, newest first, at most 10. Open opens the selected file, and Create a New Machine… opens a new window that asks for the machine. A window with no machine chosen is not restored at the next launch.

The configuration window shows the machine, its model and ROM, its window size, the mouse setting for a machine that takes a mouse, its hardware and its media, and the command line. Pinesy checks every change with MAME and removes any option the machine no longer has. When MAME rejects the driver itself, no option is removed. When MAME rejects the command line, the window shows MAME's error text and Run is not available. The window also shows MAME's warnings, which do not stop a run.

A media row with a problem shows it in red under the row's name: a file the machine needs and does not have, a file that is not found (the row shows its full path), or an SD card image that MAME's SD card cannot use. MAME takes SD cards of up to 1 GiB, and SDHC cards of up to 32 GiB that are a multiple of 512 KiB. A CHD is not checked. Choose refuses such an SD card image and says why, and a configuration that holds one cannot run. Each media row shows the file extensions MAME lists for the device, but any file can be chosen, because MAME also opens others. A hard disk device (MAME's image type `harddisk`, such as the Next's SD card) also lists `.img` and `.raw`, for raw disk images; the core adds them to the device's extension list.

The machine is chosen once, when the configuration is made. The Model and ROM choices change the driver within that machine, and a new model keeps every option it also has. The `arguments` hold only the options that differ from the starting configuration, and the ports that it leaves empty. Media paths are absolute. App settings are not saved in the file; a run adds them.

## The machine list

The core builds the list from MAME's own data (`pinesy_list_machines()`) the first time the app asks for it. No driver is named in the rules:

- An unofficial driver (an after-market change to a released machine, such as the +3e ROMs) is a ROM choice of its machine. Its machine is the driver's parent, or the official driver in the same source file with the same devices.
- A driver and its clones that share one name before a colon, such as "ZX Spectrum Next: KS1", are one machine with a model choice. The parent of that set is MAME's reference for the models and is not offered as a model; on the Next it is the machine that reports the emulator machine ID, which headless runs can still use as `tbblue`. A new configuration uses the newest model.
- Machines are grouped by their first maker. A clone joins its parent's group when they share a maker. Makers with one product are listed under "Other makers". Byte (`sinclair/byte.cpp`) has its own entry.

MAME has no option to load another driver's ROM, so a ROM variant runs its own driver: the ZX Spectrum +3 with the ROM "ZX Spectrum +3e, English" runs `specpl3e -bios en`. An unofficial driver with no single parent is listed as a machine. Groups are in order of their earliest year, with "Other makers" last, and the machines in a group are in year order. A machine that MAME marks `preliminary` shows "Partly working".

Search finds machines by name, maker, model or ROM variant name. It does not match driver names, because the app does not show them.

## Starting configuration

A new configuration starts as MAME's default for its driver, with one change. A port where MAME fits the same card on every model of the machine's MAME family is left empty, because no model of that machine came with that card: the Kempston joystick interface on the ZX Spectrum and its clones, and the Pi and MIDI ports of the Next. A card that MAME fits on some models and not others stays, because it records what each model has: the Next's ESP WiFi module is fitted on the KS2 and KS3 and not on the KS1. Memory, sound chips and disk drives keep MAME's setting.

MIDI ports on the machine itself have no row in the window. MAME gives them to the Next for the UART of its joystick ports, which has no MIDI socket. A card's MIDI ports, such as those of the Music Machine, have rows.

The library reports each slot with a kind, so the app can tell these apart: `memory` (the cards are RAM), `drive` (a floppy drive connector), `sound` (the cards make sound and hold no media) or `port`. The kind comes from the slot's default card, or, for a slot without one, from all the cards it takes. It does not change with the card fitted, so an empty slot keeps its name.

A slot's row is labelled "Memory", "Sound chip" or "Disk drive" for the first three kinds, and for a port with the description MAME gives the slot's own device type, such as "RS-232 Port". Memory options show as sizes, such as "48K". Rows with the same label under one parent are numbered, and disk drives always are. A numbered port with a default card shows "Made for:" and that card's description, whatever is fitted, so the Next's Pi port and ESP port can be told apart.

## Running

Machine > Run (⌘R) runs the front configuration in its own window, and Machine > Run with Debugger (⌥⌘R) adds `-debug`. One machine runs at a time; while it runs, configurations cannot be changed, and a configuration opened then waits for the run to end. Machine > Stop (⌘.) or closing the window stops the machine, also while it is still starting. Machine > Reset resets the running machine and keeps its RAM, as MAME's F3 does; Machine > Hard Reset starts it again from power-on, as Shift+F3 does. These menu items work also on keyboards with no Scroll Lock key, which MAME needs to turn on its own UI keys. Machine > Release Pointer (⇧⌘M) shows and frees the pointer while a machine with `-mouse` holds it, and the item becomes Grab Pointer, which holds it in the view again. Machine > Clear NVRAM… deletes the NVRAM that MAME saved for the front configuration's driver, after an alert asks to confirm. It deletes only the entries in the NVRAM folder (`nvram` in the data folder, or the configuration's own `-nvram_directory`) that MAME names after the driver: the driver's name, and the driver's name with `_` and a number, which MAME uses for a ROM that is not the driver's default. MAME writes NVRAM when a machine exits, so the item is not available while a machine runs. When MAME stops with an error, an alert shows MAME's error lines.

In the machine window, key combinations with ⌘ that a menu owns go to the menu, and every other key goes to MAME, MAME's own UI keys included (Tab opens MAME's menu). Media changes during a run go through the File Manager in MAME's menu. Settings that are not command-line options, such as Contention on the Spectrum and the input mapping, are set in MAME's menu, and MAME saves them in its `cfg` folder.

Settings:

- General: extra ROM folders, and the folder where MAME keeps its files (`~/Library/Application Support/Pinesy` by default).
- Debugger: MAME's debugger windows, or the GDB remote stub (`-debugger gdbstub -debugger_port <port>`, port 23946 by default, from 1 to 65535); the Pi GDB port (23947 by default, same range); and Start paused (see "The debugger").

Machine > Copy Command Line copies the headless command for the configuration, quoted for zsh: the executable, `--`, then the command line. It is the command line of Run, so it has no switches and no debugger options. Headless mode adds the paths below itself.

## The debugger

Run with Debugger adds `-debug`, the options of the debugger in Settings, and, for a configuration with a Pi SD card, `-pi_gdb_port <Pi GDB port>`, which starts QEMU's GDB stub for the Pi. [../docs/gdb.md](../docs/gdb.md) describes both GDB stubs.

With `-debug`, MAME stops the Next at its first instruction. Pinesy continues it there, so the Next runs from the start. With Start paused on in Settings, the Next stays stopped there (the `--paused` switch). The configuration window shows a "Pi starts paused" switch next to the Pi's SD card, when the configuration has one. With it on, a run with the debugger adds `-pi_gdb_wait`, and the Pi waits at its first instruction until a GDB client or the Continue button continues it (the `--paused-pi` switch). The setting is saved in the `.pinesy` file. A run without the debugger uses neither switch.

The machine window shows a Continue button at the right of its title bar while the Next is stopped in MAME's debugger (by a breakpoint, by a GDB client or at the start), or while the Pi waits (at its first instruction, or stopped by its GDB stub). It continues both, and hides again. The engine tells the app through the host's `stopped` callback, and the button calls `pinesy_continue()`. The app's windows stay live while the Next is stopped, with either debugger, so Stop and closing the window also work then.

## The machine window

MAME renders each frame in software at a fixed size: the visible area of the machine's first screen times a whole number on each axis, so each machine pixel is the same number of frame pixels wide and high. Of the multiples that fit in 800 x 900, it takes the one with the shape closest to MAME's view of the machine, and of equal ones the smaller (the rule of Holub's emame). On the Next, 720 x 288 becomes 720 x 576, a multiple of 1 across and 2 down. The picture fills the frame, as with MAME's `-nokeepaspect`, so no machine pixel is squeezed to MAME's 4:3 view. MAME's video options menu can still turn Keep Aspect on for the run.

The frame size is in device pixels. The window's content is the frame times a whole number, divided by the screen's pixels per point, so at 1x one frame pixel is one device pixel. The configuration's window size chooses the number: Fit to screen, the default, takes the largest that fits the screen's usable area below the title bar; a magnification takes that one, or the largest that fits when it does not. When even 1x does not fit, the window is the largest size of the frame's shape that fits. The window takes this size when the machine starts and again when the frame's size changes, as with a new screen mode. Full screen opens the window at the Fit to screen size and then puts it in macOS full screen. The user leaves full screen in the usual macOS ways, and the window then has the Fit to screen size for the current frame. While the window is in full screen, a new frame size does not change it. A machine window in headless mode uses Fit to screen. On a screen where the pixels per point is not a whole number, the window can be one device pixel off a whole multiple; the picture is then smoothed.

The configuration window's Window size picker offers Fit to screen, Full screen, 1x to 4x and Custom. Custom shows a Magnification field for any whole number of 1 or more. A configuration opens with 1x to 4x as those entries, and with any larger magnification as Custom with its number. The core holds these choices (`pinesy_core_window_choice()`, `pinesy_core_window_size_for_choice()`), so the three apps offer the same ones.

The window keeps the frame's shape when it is resized. Where the view's shape differs, as in full screen, the frame is drawn at its own shape, centred, with black bars. It is sharp (nearest-neighbour) where the drawn size is a whole multiple of the frame, and smoothed at any other size.

## The mouse

The configuration window shows a Mouse switch only for a machine that has mouse inputs with the hardware that is fitted. The Next has a Kempston mouse built in, so it always has the switch; a ZX Spectrum has it only while the Kempston mouse interface is fitted. The core decides this from MAME's description of the configuration (`mouse` in `pinesy_describe()`: an input of MAME's mouse types), not from a list of machines. The switch turns the machine's mouse on or off, and a new configuration starts with it on. On, a run adds `-mouse` after the configuration. Off, it adds `-mouse_device none`, because MAME's `-mouse_device` is `mouse` by default and would otherwise give the machine the mouse anyway. Copy Command Line adds the same option. A machine without mouse inputs gets neither option. The setting is saved in the `.pinesy` file. It stays saved while hardware without a mouse is fitted, and then has no effect.

The machine window sends the mouse over its view to MAME through the engine (`pinesy_mouse_inside()`, `pinesy_mouse_move()`, `pinesy_mouse_button()`, `pinesy_mouse_wheel()`): its movement, the left, right and middle buttons and two more, and the vertical wheel. With natural scrolling on, the wheel is still sent in the direction it turns. MAME puts the mouse in the machine's mouse inputs.

With `-mouse`, MAME hides the pointer while it is over the machine's view, and holds it there: the pointer stays at the view's centre and the mouse's movement still reaches the machine. MAME gives the pointer back when the app loses the focus, when MAME's menu opens, and when the run ends. Machine > Release Pointer (⇧⌘M) also gives it back, so the pointer can reach the menus. The machine then gets no mouse input until Machine > Grab Pointer (⇧⌘M) or a click in the view holds the pointer again; that click does not reach the machine. Grab Pointer moves the pointer into the view. The same key is MAME's UI Release Pointer key, which Pinesy sets to ⇧⌘M; MAME's Input menu can change it. The engine passes these requests to the app through the host's `pointer` callback. With `-mouse_device none`, the machine gets no mouse, and the pointer stays visible and free.

A machine window in headless mode sends the mouse the same way, and MAME's own options decide: with `-mouse` it hides and holds the pointer, and without `-mouse_device none` a machine with mouse inputs gets the mouse.

## Updates

A build that CI made and https://pinesy.zx.immo delivered looks for a newer build of its own track (`ci`, `nightly` or `release`) once, at startup. It never moves to another track. A build made with `build.sh` has no track and never looks. The core holds the build's identity (its track, its version and how it updates) and the rules below, so the GNOME and KDE apps follow them too.

When a newer build exists, the app says that it is out of date, and offers three choices:

- Never Remind Again: the app stops looking for updates.
- Never Remind for This Version: the app ignores this build. A newer build gets a new notice.
- Update: the app updates.

A CI build's version is `0.1.YYYYMMDD.HHMM`, the date and time in UTC when CI built it. A build is newer when its version is newer, compared as Sparkle compares versions: part by part, with numbers as numbers.

On macOS, Sparkle does the update. The app reads its track's appcast, `https://pinesy.zx.immo/<track>/macos/appcast.xml`. The notice is the app's own; after Update, Sparkle's windows show the download, and Sparkle checks the update's EdDSA signature (`SUPublicEDKey` in `Info.plist`), installs it and starts the new app. Sparkle checks only when the app asks it to, and schedules nothing (`SUEnableAutomaticChecks` is false).

## Headless

Any arguments make the executable run MAME with them and no UI. With `--` among the arguments, the words before the first `--` are Pinesy's switches, and the words after it are MAME's command line. Without `--`, every word is MAME's. The exit code is MAME's.

- `--paused`: with `-debug`, the Next waits at its first instruction. Without it, Pinesy continues the Next there.
- `--paused-pi`: the Pi waits at its first instruction until a GDB client or the Continue button continues it. It adds `-pi_gdb_wait`. Without `-pi_gdb_port` there is no Pi stub, and only the Continue button continues the Pi.

Any other word before `--` is an error, and the exit code is 6, MAME's code for a bad option. MAME shows no machine window here. With `-debug` and MAME's debugger windows (no `-debugger none` or `-debugger gdbstub`), the process runs as a regular app so the debugger windows can open. Headless mode does not read `.pinesy` files; the `arguments` array of one is a command line to pass. MAME's list commands take slot options, so a configuration can be explored headless: `Pinesy -- -listmedia spectrum -exp intf1` lists the Interface 1's microdrives.

```sh
Pinesy.app/Contents/MacOS/Pinesy -- tbblue -video none -sound none -seconds_to_run 10
```

## ROMs and MAME's files

Pinesy is not MAME. It reads no `mame.ini` and uses nothing in `~/.mame`. Every run, headless or not, starts with these options, and the caller's own options come after them, so any of them can be overridden for one run (`-cfg_directory <dir>`, for example):

- `-noreadconfig`: no MAME ini file is read.
- `-rompath`: the ROM sets in the bundle (`Contents/Resources/roms`), then `roms` in the data folder from Settings (`~/Library/Application Support/Pinesy/roms` by default), then the ROM folders in Settings. MAME separates these with `;`, so Settings does not take a folder with `;` in its path.
- MAME's other search paths, in `Contents/Resources`: `hash`, `samples`, `artwork`, `ctrlr`, `fonts`, `cheat`, `crosshair`, `plugins`, `language`, `software` and `bgfx`.
- The folders MAME writes, in the data folder from Settings: `ini` (`inipath`), `home` (`homepath`), `cfg`, `nvram`, `inp`, `sta`, `snap`, `diff`, `comments` and `share`.
- `-skip_gameinfo`: MAME does not show its machine information screen at start, because the machine was chosen in the app.

`build.sh` compiles the core's sources in `../core/src` with `clang++` into `libpinesy.dylib`, and puts it in `Contents/Frameworks` with `libmame_pinesy.dylib` and the libraries that one needs. It copies the app icon, `Pinesy.icns`, and the icon of `.pinesy` files, `PinesyDocument.icns`, into `Contents/Resources`. The `.pinesy` type conforms to `public.data`, not `public.json`, because Finder shows a JSON type's Quick Look text preview in place of its icon. It copies `../roms` into the bundle. Each entry there must be a ROM set: a zip, a 7z or a folder of ROM dumps. The script stops on anything else, and on a disk image (SD card, hard disk or floppy) loose in a folder or inside a zip. It cannot list a 7z, so it does not check what one holds. It also copies MAME's `plugins`, `hash`, `bgfx`, `artwork` and `language` folders from `../mame`; the other search paths have no folder in the bundle. It puts `gdb-pinesy` and `objcopy-pinesy`, which `../build-gdb.sh` builds, in `Contents/MacOS`, and stops when either is missing. [../docs/gdb.md](../docs/gdb.md) says what they do.

`build.sh` also puts `Sparkle.framework` in `Contents/Frameworks`. `sparkle.sh` downloads the Sparkle release it names into `../build/sparkle`, once, and checks its SHA-256. A CI build that is published gets its track and version from `PINESY_BUILD_TRACK` and `PINESY_BUILD_VERSION`: they go into the core, and into `Info.plist` as `CFBundleVersion` and `CFBundleShortVersionString`, which Sparkle compares. Without them the bundle is signed ad hoc. With `CODESIGN_IDENTITY` (and `CODESIGN_KEYCHAIN`), `build.sh` signs it with that identity instead, with the hardened runtime, a secure timestamp and `Pinesy.entitlements`: the engine's recompilers and QEMU write code at run time, which the hardened runtime allows only with `com.apple.security.cs.allow-jit` and `com.apple.security.cs.allow-unsigned-executable-memory`.

`Pinesy -- -showconfig` prints every path a run uses.
