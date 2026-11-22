# pinesy for GNOME

The GNOME app for Pi/Ne-Sy. It is a GTK 4 and libadwaita app, written in C++ with gtkmm. It links `libpinesy`, the core in `../core`, and the MAME library `../mame/libmame_pinesy_gtk.so`, the engine with the GTK debugger. The core holds every rule the app follows (the machine list, configurations, `.pinesy` files, MAME's paths, the settings and headless mode). This folder holds only the user interface.

`../app/README.md`, the README of the macOS app, describes what the app does. This README describes how the GNOME app differs.

To build and run it, see [docs/build-linux.md](../docs/build-linux.md).

The data folder is `$XDG_DATA_HOME/pinesy` (by default `~/.local/share/pinesy`) unless Preferences sets a different one.

## Desktop files

`data/` holds the files a desktop needs:

- `com.stalepixels.Pinesy.gschema.xml`: the settings schema. The keys are `rom-folders`, `data-folder`, `debugger` (`windows` or `gdbStub`), `gdb-port`, `pi-gdb-port` and `start-paused`, as the core's settings schema names them.
- `com.stalepixels.Pinesy.desktop`: the desktop entry, for the MIME type `application/x-pinesy`.
- `com.stalepixels.Pinesy.mime.xml`: the MIME type for `*.pinesy` files, with the app icon.
- `com.stalepixels.Pinesy.service.in`: the D-Bus service file. CMake fills in the installed path of the program.

The app icon is `../icons/hicolor/<size>x<size>/apps/pinesy.png`, installed as `com.stalepixels.Pinesy.png`. The desktop entry and the windows use that icon name.

`cmake --install` installs them with the app ([docs/build-linux.md](../docs/build-linux.md), "Install a build"), and compiles the schema where it puts it. An installed copy reads the schema from `../share/glib-2.0/schemas`, next to its own folder, before GLib's own folders, and its resources folder from `../share/pinesy/resources`. A run from the build folder uses `<build-dir>/resources` and needs `GSETTINGS_SCHEMA_DIR=<build-dir>/schemas`. The desktop entry sets `DBusActivatable=true`, so a file manager opens a `.pinesy` file through D-Bus (`org.freedesktop.Application.Open`), not through the command line, because any command-line argument starts headless mode. The one exception is `--gapplication-service` alone, which D-Bus activation uses to start the app.

## Flatpak

To build the package, see [docs/build-linux.md](../docs/build-linux.md). `flatpak/com.stalepixels.Pinesy.yml` builds it on the GNOME 51 runtime.

In the Flatpak:

- The manifest builds the app and installs it into `/app` with `cmake --install`, so the Flatpak has the same files as a native install.
- The resources folder is `/app/share/pinesy/resources`. `../cmake/install-resources.sh` fills it with the content that `../app/build.sh` puts in the macOS bundle: the ROM sets from `../roms`, with the same check, and MAME's `plugins`, `hash`, `bgfx`, `artwork` and `language` folders.
- The data folder is `~/.var/app/com.stalepixels.Pinesy/data/pinesy`.
- `gdb-pinesy` is `/app/bin/gdb-pinesy`, and `objcopy-pinesy` is `/app/bin/objcopy-pinesy`. The manifest builds `gdb-pinesy` from GDB's release tarball and `objcopy-pinesy` from the binutils release tarball. [../docs/gdb.md](../docs/gdb.md) says what they do.
- The settings schema, the desktop entry, the MIME type, the D-Bus service file and the app icon are installed. Flatpak exports all but the schema to the desktop.
- Copy Command Line gives `/app/bin/pinesy`, the path inside the Flatpak. Outside it, the command is `flatpak run com.stalepixels.Pinesy`.

The permissions:

- `--socket=wayland`, `--socket=fallback-x11`, `--share=ipc` and `--device=dri`: the windows, on Wayland or else X11.
- `--socket=pulseaudio`: MAME's PulseAudio sound module, with `-sound pulse`. MAME's default sound module is PortAudio, which plays to ALSA's default device. `--filesystem=xdg-run/pipewire-0`: its PipeWire module, with `-sound pipewire`.
- `--device=input`: joysticks and gamepads, which MAME reads from `/dev/input/event*`.
- `--filesystem=home`: a `.pinesy` file names its media files by path, and the ROM folders in Preferences are paths. The file chooser portal gives the app only the file chosen, so MAME could not open the media named in a configuration that was opened later. Media outside the home folder is not reachable.
- `--share=network`: the GDB remote stub listens on a TCP port. Without this permission, the Flatpak has its own network, and a GDB on the host cannot connect.

## How it maps to the macOS app

| macOS | GNOME |
| --- | --- |
| File > New (⌘N) | New Configuration in the main menu (Ctrl+N) |
| File > Open | Open… in the main menu (Ctrl+O), with the GTK file dialog |
| File > Open Recent | Open Recent in the main menu, with Clear Menu. The list is GTK's recent files (`recently-used.xbel`), the entries Pinesy added when it opened or saved a configuration. |
| A launch with no document shows the recent configurations | The same: a Recent Configurations window with Open and Create a New Machine…, or a new configuration when there is no recent configuration. GNOME does not restore windows at the next launch. |
| Documents save themselves | Save (Ctrl+S) and Save As… (Ctrl+Shift+S). Closing a window with unsaved changes asks whether to save them. An unsaved configuration has "•" before its title. |
| The machine picker sheet | A dialog. The search button or typing starts a search. A click selects a machine; a double click, Enter or Choose chooses it. |
| Cancel in the picker closes the window | The same. When it was the last window and no machine runs, the app quits. |
| Machine > Run (⌘R) | The Run button (Ctrl+R) |
| Machine > Run with Debugger (⌥⌘R) | Run with Debugger in the Run button's menu (Ctrl+Shift+R) |
| Machine > Stop (⌘.) | The Stop button, shown while a machine runs (Ctrl+.) |
| Machine > Reset, Machine > Hard Reset | Reset and Hard Reset in the main menu, enabled while a machine runs |
| Machine > Release Pointer / Grab Pointer (⇧⌘M) | Release Pointer / Grab Pointer in the main menu (Ctrl+Shift+M), enabled while a machine runs. The pointer is not held on GNOME, so Release Pointer only stops the machine's mouse input, until Grab Pointer or a click in the view. In the machine's window the key goes to MAME, as MAME's UI Release Pointer key. |
| Machine > Clear NVRAM… | Clear NVRAM… in the main menu, under Grab Pointer, not available while a machine runs. An alert dialog asks to confirm, with Clear NVRAM as a destructive button. |
| Machine > Copy Command Line | Copy Command Line in the main menu (Ctrl+Shift+C), and the copy button by the command line |
| Settings (UserDefaults) | Preferences in the main menu (Ctrl+,), stored with GSettings |
| Debugger windows | MAME's GTK debugger |
| An alert when MAME stops with an error | A libadwaita alert dialog with MAME's error lines |

Opening a file in a window that has no machine and no file yet opens it in that window. Otherwise it opens in a new window.

## The machine window

The machine runs in its own window. MAME draws each frame into a buffer, and the window shows it as a texture. Each key goes to the machine by its XKB keycode, and typed characters go to MAME's menus. Every key goes to the machine, also key combinations with Ctrl, so the app's keyboard shortcuts do not work in the machine window. To stop the machine, close its window, or use Stop in a configuration window.

The window takes the size the core gives for the frame and the configuration's window size (`../app/README.md`, "The machine window"). The room on the screen is the monitor's whole area (`gdk_monitor_get_geometry`) below the header bar, because GTK 4 does not give the work area. The configuration window's Window size row chooses the size, and a Magnification row under it takes the number for Custom. With Full screen, the window calls `gtk_window_fullscreen` when the first frame has sized it. Keys go to the machine, so in full screen the header bar shows a Leave Full Screen button; the window then has the Fit to screen size for the current frame. GTK 4 cannot keep a window's aspect ratio, so a window resized to another shape shows the frame at its own shape, centred, with black bars.

The configuration window has a Mouse switch row under the window size for a machine that takes a mouse (`../app/README.md`, "The mouse"). The view sends the mouse to MAME: its movement, from the view's motion events in logical units, every button press and release (left, right, middle, back and forward), and the wheel's notches. A touchpad's smooth scrolling is not sent. When MAME asks to hide the pointer, the view's cursor is `none`. GTK 4 has no call that holds the pointer in a window, so MAME's request to hold it does nothing here. The machine gets the mouse's movement while the pointer is over the view, and while a drag that started there goes on.

## Debugger

Run with Debugger adds `-debug`, and with the GDB remote stub also `-debugger gdbstub -debugger_port <port>`. A configuration with a Pi SD card also gets `-pi_gdb_port <port>` with the Pi GDB port from Preferences. `../app/README.md` ("The debugger") and `../docs/gdb.md` describe the rest.

- MAME's GTK debugger windows: the engine is built with the GTK debugger, so MAME chooses it for its debugger windows. Its windows use the app's GTK display, and the app's windows stay live while the debugger has the machine stopped.
- The GDB remote stub: a GDB client connects on the port from Preferences. The app's windows stay live while the machine is stopped.

Preferences also has Start paused. The configuration window has a "Pi starts paused" row under the Pi's SD card, when the configuration has one. The machine window's header bar shows a Continue button while the Next is stopped in MAME's debugger or the Pi waits.

## Headless mode

Any command-line arguments run MAME with them and no user interface, as on macOS. Pinesy's switches (`--paused`, `--paused-pi`) go before `--`, and MAME's command line after it (`../app/README.md`, "Headless"). The exit code is MAME's.

```sh
GSETTINGS_SCHEMA_DIR=<build-dir>/schemas <build-dir>/pinesy -- tbblue -video none -sound none -seconds_to_run 10
<prefix>/bin/pinesy -- tbblue -video none -sound none -seconds_to_run 10
```

With `-video none`, no window opens and no display is needed. Otherwise the machine gets a window, and closing it stops the machine.
