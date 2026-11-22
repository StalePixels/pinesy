# Pi/Ne-Sy for KDE Plasma

The KDE Plasma front end for Pi/Ne-Sy. It is a Qt 6 and KDE Frameworks 6 app with a Kirigami (QML) user interface. It links `libpinesy`, the core library in `../core`, and the engine `libmame_pinesy_qt.so`, the MAME library built from `../mame` with the `pinesy` OSD and MAME's Qt debugger.

The app holds only the user interface. Every rule (the machine list, configurations, `.pinesy` files, MAME's paths, the settings, the copied command line and headless mode) comes from the core. `../app/README.md` describes those rules.

To build and run it, see [docs/build-linux.md](../docs/build-linux.md).

## The resources folder

The core needs a resources folder. It holds the bundled ROM sets in `roms`, and MAME's search folders (`hash`, `plugins` and the others the core names).

An installed copy uses `../share/pinesy/resources`, next to its own folder. `cmake --install` fills it with `../cmake/install-resources.sh`, and installs the app, `libpinesy.so`, the engine library, the `.desktop` file, the MIME type, the D-Bus service file and the app icon ([docs/build-linux.md](../docs/build-linux.md), "Install a build").

When that folder does not exist, as in a run from the build folder, the app uses the folder in the cache variable `PINESY_RESOURCES_FOLDER`. Its default is `<build-dir>/resources`, which CMake makes at configure time with symbolic links to `../roms` and to MAME's `plugins`, `hash`, `bgfx`, `artwork` and `language` folders in `../mame`. These are the folders `../app/build.sh` copies into the macOS bundle.

## Flatpak

To build the package, see [docs/build-linux.md](../docs/build-linux.md). `flatpak/com.stalepixels.pinesy.kde.yml` builds it on the KDE 6.11 runtime.

In the Flatpak:

- The manifest builds the app and installs it into `/app` with `cmake --install`, so the Flatpak has the same files as a native install.
- The resources folder is `/app/share/pinesy/resources`. `../cmake/install-resources.sh` fills it with the content that `../app/build.sh` puts in the macOS bundle: the ROM sets from `../roms`, with the same check, and MAME's `plugins`, `hash`, `bgfx`, `artwork` and `language` folders.
- The data folder is `~/.var/app/com.stalepixels.pinesy.kde/data/pinesy`.
- `gdb-pinesy` is `/app/bin/gdb-pinesy`, and `objcopy-pinesy` is `/app/bin/objcopy-pinesy`. The manifest builds `gdb-pinesy` from GDB's release tarball and `objcopy-pinesy` from the binutils release tarball. [../docs/gdb.md](../docs/gdb.md) says what they do.
- Copy Command Line gives `/app/bin/pinesy-kde`, the path inside the Flatpak. Outside it, the command is `flatpak run com.stalepixels.pinesy.kde`.
- The desktop entry, the MIME type, the D-Bus service file and the app icon are installed, and Flatpak exports them to the desktop.

The permissions:

- `--socket=wayland`, `--socket=fallback-x11`, `--share=ipc` and `--device=dri`: the windows, on Wayland or else X11.
- `--socket=pulseaudio`: MAME's PulseAudio sound module, with `-sound pulse`. MAME's default sound module is PortAudio, which plays to ALSA's default device. `--filesystem=xdg-run/pipewire-0`: its PipeWire module, with `-sound pipewire`.
- `--device=input`: joysticks and gamepads, which MAME reads from `/dev/input/event*`.
- `--filesystem=home`: a `.pinesy` file names its media files by path, and the ROM folders in Settings are paths. The file dialog portal gives the app only the file chosen, so MAME could not open the media named in a configuration that was opened later. Media outside the home folder is not reachable.
- `--share=network`: the GDB remote stub listens on a TCP port. Without this permission, the Flatpak has its own network, and a GDB on the host cannot connect.

## Updates

`../app/README.md` ("Updates") says when the app looks for a newer build and what the notice offers. The notice is a `QMessageBox`. The rest depends on how the app was installed:

- A Flatpak from a track's repository asks the Flatpak portal (`org.freedesktop.portal.Flatpak`, version 2 or later). At startup the app makes an update monitor with `CreateUpdateMonitor`. Its `UpdateAvailable` signal means that the repository has a newer commit, and the notice shows; Never Remind for This Version ignores that commit. Update calls the monitor's `Update`. When its `Progress` signal reports that the update is done, the app starts the new build with the portal's `Spawn` and the flag `FLATPAK_SPAWN_FLAGS_LATEST_VERSION`, and quits. The new build gets `PINESY_REPLACE=1`, and takes the app's name on the session bus from the old one, which allows replacement (`DesktopService`). When the update failed, an alert shows the error. When the new build needs more permissions, the portal refuses with `org.freedesktop.DBus.Error.NotSupported`, and the alert says to update with the system's software tools.
- A Debian package from a track's apt repository reads the track's index for its architecture, `https://pinesy.zx.immo/<track>/dists/sid/main/binary-<arch>/Packages`, and compares the newest `pinesy-kde` there with its own version. Update shows how to update: with the system's package manager.

## Run

With no arguments the app starts and asks for a machine. With any arguments it runs headless (see below).

## How it maps to the macOS app

| macOS app | KDE app |
|---|---|
| A configuration window for each document | A Kirigami window for each configuration |
| File > New (Cmd+N) opens the machine picker | File > New (Ctrl+N) opens a window with the machine picker. Cancel closes the window. |
| A launch with no document shows the recent configurations | The same: a Recent Configurations window with Open and Create a New Machine…, or a new configuration when there is no recent configuration. |
| File > Open Recent | File > Open Recent, with Clear List. The list is in `pinesyrc`, group `Recent Files`, with the keys KDE's `KRecentFilesAction` uses (`File1`, `Name1`, …). |
| Open and Save panels | File > Open (Ctrl+O), Save (Ctrl+S) and Save As (Ctrl+Shift+S) use Qt's file dialogs. On a Plasma desktop, Plasma's Qt platform integration supplies the KDE file dialog for these. An untouched new window takes the opened file; otherwise the file opens in a new window. |
| Closing a document with changes | The window asks: Save, Discard or Cancel. |
| Machine picker sheet | A dialog: makers on the left, their machines on the right, a search field above. Down moves from the search field into the list; Return or a double click chooses. |
| Configuration window | A page with the machine, its model and ROM, its window size, a Mouse switch for a machine that takes a mouse, a Hardware section, a Media section and the command line. MAME's errors show in an error message above the form, and Run is not available then. MAME's warnings show in an information message. A media row with a problem shows it in red under the row: a file the machine needs, a file that is not found (the row shows its full path), or an SD card image that MAME's SD card cannot use. Choose refuses such an SD card image and says why, and a configuration that holds one cannot run. The file dialog shows the device's file extensions first, and All files. |
| Machine > Run (Cmd+R) | Machine > Run (Ctrl+R), also in the page's toolbar |
| Machine > Run with Debugger (Opt+Cmd+R) | Machine > Run with Debugger (Ctrl+Shift+R), also in the page's toolbar |
| Machine > Stop (Cmd+.) | Machine > Stop (Ctrl+.) |
| Machine > Reset, Machine > Hard Reset | Machine > Reset, Machine > Hard Reset |
| Machine > Release Pointer / Grab Pointer (⇧⌘M) | Machine > Release Pointer / Grab Pointer (Ctrl+Shift+M). The pointer is not held on KDE, so Release Pointer only stops the machine's mouse input, until Grab Pointer or a click in the view. In the machine's window the key goes to MAME, as MAME's UI Release Pointer key. |
| Machine > Clear NVRAM… | Machine > Clear NVRAM…, not available while a machine runs. A dialog asks to confirm. |
| Machine > Copy Command Line | Machine > Copy Command Line, and a button under the command line |
| Settings window | Settings > Configure Pi/Ne-Sy: a window with a General page and a Debugger page |
| Settings in UserDefaults | Settings in KConfig, in `pinesyrc` |
| Data folder `~/Library/Application Support/Pinesy` | Data folder `~/.local/share/pinesy` (`QStandardPaths::AppDataLocation`) |

### Running a machine

The machine runs in its own window. A Qt Quick item shows each frame MAME draws as a texture. Keys go to MAME by their position (`QKeyEvent::nativeScanCode`, the XKB keycode), and typed characters go to MAME's own menus. The window tells MAME when it gains or loses the keyboard focus, and when its size changes. It takes the size the core gives for the frame and the configuration's window size (`../app/README.md`, "The machine window"), with the screen's available area less the window frame as the room, centred the first time. The configuration page's Window size box chooses the size, and a Magnification spin box under it takes the number for Custom. With Full screen, the window calls `showFullScreen()` when the first frame has sized it. The user leaves full screen through the window manager, such as KWin's window menu, and the window then has the Fit to screen size for the current frame. Qt has no way to keep a window's shape while the user resizes it, so a window of another shape shows the frame at its own shape, centred, with black bars.

The mouse over the machine's item goes to MAME (`../app/README.md`, "The mouse"): its movement, from hover and mouse move events in logical units, the left, right, middle, back and forward buttons, and the wheel (`angleDelta`, 120 to a notch, in the direction the wheel turns). When MAME asks to hide the pointer, the item's cursor is `Qt::BlankCursor`. Qt has no call that holds the pointer in a window, so MAME's request to hold it does nothing here. The machine gets the mouse's movement while the pointer is over the item, and while a drag that started there goes on.

One machine runs at a time. While it runs, configurations cannot be changed, and a configuration opened then waits for the run to end. Stop, or closing the machine window, asks MAME to exit; the window closes when MAME has. When MAME stops with an error, a message shows MAME's error lines. Quitting the app while a machine runs stops the machine first.

### Settings

- General: extra ROM folders, and the folder where MAME keeps its files. A folder with `;` in its path cannot be a ROM folder.
- Debugger: what Run with Debugger uses. The choices are the core's: MAME's debugger windows, or the GDB remote stub with its port (23946 by default, from 1 to 65535). The engine is built with MAME's Qt debugger, so MAME's own choice for its debugger windows is the Qt debugger. Its windows use the app's `QApplication`, and the app's windows stay live while the debugger has the machine stopped. The page also has the Pi GDB port (23947 by default) and Start paused.

The configuration window has a "Pi starts paused" check box under the Pi's SD card, when the configuration has one. The machine window shows a Continue button at its top right while the Next is stopped in MAME's debugger or the Pi waits. `../app/README.md` ("The debugger") and `../docs/gdb.md` describe both.

Settings apply as they change.

## Headless mode

Any arguments make the executable run MAME with them and no app user interface, through the core's headless call. Pinesy's switches (`--paused`, `--paused-pi`) go before `--`, and MAME's command line after it, as on macOS (`../app/README.md`, "Headless"). The exit code is MAME's.

```sh
<build-dir>/bin/pinesy-kde -- tbblue -video none -sound none -seconds_to_run 10
```

With `-video none` there is no window and Qt is not started. Otherwise the machine gets its own window, as in a run from the app, and closing it stops the machine. Qt sees only the program name, so no MAME option reaches it.

## Desktop file and MIME type

`com.stalepixels.pinesy.kde.desktop` starts the app. The app icon is `../icons/hicolor/<size>x<size>/apps/pinesy.png`, installed as `com.stalepixels.pinesy.kde.png`; the desktop file and the windows use that icon name. `com.stalepixels.pinesy.kde.xml` defines the MIME type `application/x-pinesy` for `*.pinesy` files, a kind of `application/json`, with the app icon. The GNOME app defines the same MIME type.

The desktop file lists the MIME type and sets `DBusActivatable=true`, so a file manager opens `.pinesy` files with the app over D-Bus, not on the command line, where any argument starts headless mode. CMake installs the D-Bus service file, `com.stalepixels.pinesy.kde.service`, made from `com.stalepixels.pinesy.kde.service.in` with the installed path of the executable; the Flatpak exports it. The app registers `org.freedesktop.Application` under the app id on the session bus (`src/desktopservice.cpp`). Open brings a file that is already open to the front; otherwise an untouched new configuration window takes the file, or the file opens in a new window. Activate brings the newest window to the front. With no window, it shows the recent configurations, or opens a new configuration when there is none. When another instance has the name, the app runs without it.

