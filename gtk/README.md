# pinesy for GNOME

The GNOME app for Pi/Ne-Sy. It is a GTK 4 and libadwaita app, written in C++ with gtkmm. It links `libpinesy`, the core in `../core`, and the MAME library `../mame/libmame_pinesy_gtk.so`, the engine with the GTK debugger. The core holds every rule the app follows (the machine list, configurations, `.pinesy` files, MAME's paths, the settings and headless mode). This folder holds only the user interface.

What the app does is described in `../app/README.md`, the README of the macOS app. This README describes how the GNOME app differs.

## Build

The build needs CMake 3.20 or later, a C++20 compiler, gtkmm 4.10 or later (`gtkmm-4.0`), libadwaita 1.6 or later and `glib-compile-schemas`. Build the engine first, from the repository root:

```sh
./build-mame.sh --gtk
```

This builds `../mame/libmame_pinesy_gtk.so` in its own build folder, `../mame/build-gtk`. It is the MAME engine with the GTK debugger from `../debuggers/gtk`, a port of MAME's Qt debugger to GTK 4. Then:

```sh
cmake -S gtk -B <build-dir>
cmake --build <build-dir>
```

The build also builds the core from `../core`, without its tests. It makes these in the build folder:

- `pinesy`: the app.
- `schemas/gschemas.compiled`: the compiled settings schema.
- `resources`: the resources folder the app gives the core. It holds links to `../roms` (as `roms`) and to MAME's `plugins`, `hash`, `bgfx`, `artwork` and `language` folders in `../mame`.

The cache variable `PINESY_RESOURCES_DIR` sets a different resources folder. The core's cache variables, such as `PINESY_ENGINE_LIBRARY`, also apply. Here `PINESY_ENGINE_LIBRARY` defaults to `../mame/libmame_pinesy_gtk.so`.

## Run

GLib must find the settings schema. To run from the build folder:

```sh
GSETTINGS_SCHEMA_DIR=<build-dir>/schemas <build-dir>/pinesy
```

When GLib cannot find the schema, `pinesy` prints how to set `GSETTINGS_SCHEMA_DIR` and exits with code 1. This applies to headless mode too, because headless mode also uses the settings.

The data folder is `$XDG_DATA_HOME/pinesy` (by default `~/.local/share/pinesy`) unless Preferences sets a different one.

## Desktop files

`data/` holds the files a desktop needs:

- `com.stalepixels.Pinesy.gschema.xml`: the settings schema. The keys are `rom-folders`, `data-folder`, `debugger` (`windows` or `gdbStub`) and `gdb-port`, as the core's settings schema names them.
- `com.stalepixels.Pinesy.desktop`: the desktop entry, for the MIME type `application/x-pinesy`.
- `com.stalepixels.Pinesy.mime.xml`: the MIME type for `*.pinesy` files.
- `com.stalepixels.Pinesy.service`: the D-Bus service file.

The build does not install these files. The desktop entry sets `DBusActivatable=true`: a file manager opens a `.pinesy` file through D-Bus (`org.freedesktop.Application.Open`), not through the command line, because any command-line argument starts headless mode. The one exception is `--gapplication-service` alone, which D-Bus activation uses to start the app.

## How it maps to the macOS app

| macOS | GNOME |
| --- | --- |
| File > New (⌘N) | New Configuration in the main menu (Ctrl+N) |
| File > Open | Open… in the main menu (Ctrl+O), with the GTK file dialog |
| Documents save themselves | Save (Ctrl+S) and Save As… (Ctrl+Shift+S). Closing a window with unsaved changes asks whether to save them. An unsaved configuration has "•" before its title. |
| The machine picker sheet | A dialog. The search button or typing starts a search. A click selects a machine; a double click, Enter or Choose chooses it. |
| Cancel in the picker closes the window | Cancel leaves the window open with a Choose Machine button, because a GNOME app quits when its last window closes. |
| Machine > Run (⌘R) | The Run button (Ctrl+R) |
| Machine > Run with Debugger (⌥⌘R) | Run with Debugger in the Run button's menu (Ctrl+Shift+R) |
| Machine > Stop (⌘.) | The Stop button, shown while a machine runs (Ctrl+.) |
| Machine > Copy Command Line | Copy Command Line in the main menu (Ctrl+Shift+C), and the copy button by the command line |
| Settings (UserDefaults) | Preferences in the main menu (Ctrl+,), stored with GSettings |
| Debugger windows | MAME's GTK debugger |
| An alert when MAME stops with an error | A libadwaita alert dialog with MAME's error lines |

Opening a file in a window that has no machine and no file yet opens it in that window. Otherwise it opens in a new window.

## The machine window

The machine runs in its own window. MAME draws each frame into a buffer, and the window shows it as a texture. Each key goes to the machine by its XKB keycode, and typed characters go to MAME's menus. Every key goes to the machine, also key combinations with Ctrl, so the app's keyboard shortcuts do not work in the machine window. To stop the machine, close its window, or use Stop in a configuration window.

The window takes the size MAME asks for, scaled down to fit the monitor. GTK 4 cannot keep a window's aspect ratio, so a window resized to another shape shows the picture centred, with black bars.

## Debugger

Run with Debugger adds `-debug`, and with the GDB remote stub also `-debugger gdbstub -debugger_port <port>`.

- MAME's GTK debugger windows: the engine is built with the GTK debugger, so MAME chooses it for its debugger windows. Its windows use the app's GTK display, and the app's windows stay live while the debugger has the machine stopped.
- The GDB remote stub: the machine waits for a GDB client on the port from Preferences. While it waits, and while GDB has the machine stopped, the app does not respond, because the stub's wait loop does not run the app's events.

## Headless mode

Any command-line arguments run MAME with them and no user interface, as on macOS. A leading `--` is dropped and the exit code is MAME's.

```sh
GSETTINGS_SCHEMA_DIR=<build-dir>/schemas <build-dir>/pinesy -- tbblue -video none -sound none -seconds_to_run 10
```

With `-video none`, no window opens and no display is needed. Otherwise the machine gets a window, and closing it stops the machine.
