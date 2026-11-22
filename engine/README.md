# The engine

The engine is MAME built as a library: `libmame_pinesy.dylib` on macOS, `libmame_pinesy.so` on Linux. It is MAME from `../mame` with the `pinesy` OSD from this folder. Its C interface is `include/pinesy.h`, which documents every call. The core (`../core`) and the front ends call it; it holds no rules of its own about machines or configurations.

To build it, see [docs/build-macos.md](../docs/build-macos.md) or [docs/build-linux.md](../docs/build-linux.md).

## Outside MAME's source tree

The OSD and its build rules live here, not in MAME's tree. The fork's `scripts/src/osd/pinesy.lua` only loads `pinesy.lua` from this folder, which loads `pinesy_cfg.lua`. The OSD stays thin: it runs a machine, describes machines, takes input and hands out frames, sound and output. Everything else is in the core.

The OSD is the Cocoa OSD (`src/osd/mac`) with its window layer replaced. The Cocoa OSD reaches the platform only through a set of `Mac*` C functions. `osd/hostview.mm` gives them a body on a view the macOS app owns, and `osd/hostlinux.cpp` gives them a body on a GTK or Qt host. No upstream file changes for this. The header comment of each file in `osd/` says what it supplies.

## The main loop

MAME keeps its own main loop on the main thread, so its debugger works as it does in MAME. On Linux, MAME calls the host's `pump` callback from its event processing and from the debugger's wait loop, and the front end runs its toolkit's events there: `g_main_context_iteration` on GTK, `QCoreApplication::processEvents` on Qt. On macOS the OSD runs AppKit's events itself.

## Linux

The Linux OSD does not use MAME's SDL OSD, because SDL owns its windows and cannot draw inside a GTK or Qt window on Wayland.

- Video: MAME renders in software and passes each frame to the host's `frame` callback, and the front end draws it on the GPU. The frame has a fixed size for the machine's screen, not the window's size, so the CPU work does not grow with the window or the screen's pixel density. `../app/README.md` ("The machine window") gives the size rule.
- Keyboard: `pinesy_key()` takes the XKB keycode (the evdev code plus 8). GTK and Qt give this code on X11 and on Wayland, so one table in `osd/hostlinux.cpp` serves both front ends.
- Sound: MAME's PortAudio module by default, which plays to ALSA's default device. Its PulseAudio module with `-sound pulse`, and its PipeWire module with `-sound pipewire`.
- Joysticks: `osd/input_evdev.cpp` reads `/dev/input/event*` and needs no library. It is the default joystick provider (`-joystickprovider evdev`; `none` turns joysticks off). The user needs read access to the device nodes; udev gives it to the user at the active local session through the `uaccess` tag.

## Debugger windows

The macOS engine has MAME's Cocoa debugger. On Linux, a build without debugger windows links neither GTK nor Qt. `build-mame.sh --gtk` builds `libmame_pinesy_gtk.so`, with the GTK debugger in `../debuggers/gtk`, a GTK port of MAME's Qt debugger. `build-mame.sh --qt` builds `libmame_pinesy_qt.so`, with MAME's Qt debugger, which shares the KDE app's `QApplication`.

The fork registers the GTK debugger in `src/osd/modules/lib/osdobj_common.cpp` inside `#ifdef PINESY_DEBUG_GTK`, which only the GTK build defines. New debugger features go into the GTK debugger first, then into the Qt and Cocoa debuggers.

## Machine descriptions

`pinesy_list_machines()` and `pinesy_describe()` return JSON and start no machine. `pinesy_describe()` builds the machine configuration from a command line, slot options included, as a run would, and reports its slots, BIOS sets and media. MAME's `-listxml` cannot do this: it takes no slot options, so it cannot describe a machine with devices fitted, and it gives a slot only its name, with no description for a label.
