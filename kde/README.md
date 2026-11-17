# Pi/Ne-Sy for KDE Plasma

The KDE Plasma front end for Pi/Ne-Sy. It is a Qt 6 and KDE Frameworks 6 app with a Kirigami (QML) user interface. It links `libpinesy`, the core library in `../core`, and the engine `libmame_pinesy_qt.so`, the MAME library built from `../mame` with the `pinesy` OSD and MAME's Qt debugger.

The app holds only the user interface. Every rule (the machine list, configurations, `.pinesy` files, MAME's paths, the settings, the copied command line and headless mode) comes from the core. `../app/README.md` describes those rules.

## Build

Build the engine first, from the repository root:

```sh
./build-mame.sh --qt
```

This builds `../mame/libmame_pinesy_qt.so` in its own build folder, `../mame/build-qt`. `./build-mame.sh` without `--qt` builds `libmame_pinesy.so`, the engine without Qt, and `./build-mame.sh --gtk` builds `libmame_pinesy_gtk.so`, the engine with the GTK debugger that the GNOME front end uses. The builds do not share objects. Then:

```sh
cmake -S kde -B <build-dir>
cmake --build <build-dir>
```

The build needs CMake 3.20 or later, a C++20 compiler, Qt 6.5 or later (Core, Gui, Qml, Quick, QuickControls2, Widgets), extra-cmake-modules, and KDE Frameworks 6: Kirigami, KConfig, KCoreAddons and KI18n. The core is built as part of the same build, without its tests. The core's cache variables (`PINESY_ENGINE_LIBRARY`, `PINESY_ENGINE_INCLUDE_DIR`) apply here too. Here `PINESY_ENGINE_LIBRARY` defaults to `../mame/libmame_pinesy_qt.so`.

The executable is `<build-dir>/bin/pinesy-kde`.

## The resources folder

The core needs a resources folder. It holds the bundled ROM sets in `roms`, and MAME's search folders (`hash`, `plugins` and the others the core names). The cache variable `PINESY_RESOURCES_FOLDER` sets it. The default is `<build-dir>/resources`, which CMake makes at configure time with symbolic links to `../roms` and to MAME's `plugins`, `hash`, `bgfx`, `artwork` and `language` folders in `../mame`. These are the folders `../app/build.sh` copies into the macOS bundle. The path is compiled into the executable, so a build runs from its build folder.

`cmake --install` installs the executable, the `.desktop` file, the MIME type and the app icon, but the executable still reads the resources folder in the build folder. The Flatpak (below) installs its own resources folder.

## Flatpak

`flatpak/com.stalepixels.pinesy.kde.yml` builds the app as a Flatpak on the KDE 6.11 runtime (`org.kde.Platform`). It builds what is checked out in this repository: QEMU and the engine with MAME's Qt debugger (with `../build-mame.sh --qt QT_HOME=/usr`, because the SDK has `qmake` and not `qmake6`), then the core and the app. The engine is compiled with clang from the SDK extension `org.freedesktop.Sdk.Extension.llvm22`, without the SDK's default compiler flags.

The build needs `flatpak`, `flatpak-builder` and the Flathub remote. From the repository root:

```sh
flatpak --user remote-add --if-not-exists flathub https://dl.flathub.org/repo/flathub.flatpakrepo
flatpak-builder --user --install --install-deps-from=flathub --force-clean --ccache \
    --state-dir=<state-dir> <build-dir> kde/flatpak/com.stalepixels.pinesy.kde.yml
```

Put `<state-dir>` and `<build-dir>` outside the repository, because the manifest copies the repository folder into the build. The copy leaves out git and MAME's build folders and engine libraries.

`build-mame.sh` compiles the engine with `-march=native`, so the Flatpak runs only on processors that have the instruction set of the machine that built it.

Then:

```sh
flatpak run com.stalepixels.pinesy.kde
flatpak run com.stalepixels.pinesy.kde -- tbblue -video none -sound none -seconds_to_run 10
```

In the Flatpak:

- The resources folder is `/app/share/pinesy/resources`. `../flatpak/install-resources.sh` fills it with the content that `../app/build.sh` puts in the macOS bundle: the ROM sets from `../roms`, with the same check, and MAME's `plugins`, `hash`, `bgfx`, `artwork` and `language` folders.
- The data folder is `~/.var/app/com.stalepixels.pinesy.kde/data/pinesy`.
- Copy Command Line gives `/app/bin/pinesy-kde`, the path inside the Flatpak. Outside it, the command is `flatpak run com.stalepixels.pinesy.kde`.

The permissions:

- `--socket=wayland`, `--socket=fallback-x11`, `--share=ipc` and `--device=dri`: the windows, on Wayland or else X11.
- `--socket=pulseaudio`: MAME's PulseAudio sound module, which MAME chooses by default. `--filesystem=xdg-run/pipewire-0`: its PipeWire module, with `-sound pipewire`.
- `--device=input`: joysticks and gamepads, which MAME reads from `/dev/input/event*`.
- `--filesystem=home`: a `.pinesy` file names its media files by path, and the ROM folders in Settings are paths. The file dialog portal gives the app only the file chosen, so MAME could not open the media named in a configuration that was opened later. Media outside the home folder is not reachable.
- `--share=network`: the GDB remote stub listens on a TCP port. Without this permission, the Flatpak has its own network, and a GDB on the host cannot connect.

## Run

```sh
<build-dir>/bin/pinesy-kde
```

With no arguments the app starts and asks for a machine. With any arguments it runs headless (see below).

## How it maps to the macOS app

| macOS app | KDE app |
|---|---|
| A configuration window for each document | A Kirigami window for each configuration |
| File > New (Cmd+N) opens the machine picker | File > New (Ctrl+N) opens a window with the machine picker. A start with no document does the same. Cancel closes the window. |
| Open and Save panels | File > Open (Ctrl+O), Save (Ctrl+S) and Save As (Ctrl+Shift+S) use Qt's file dialogs. On a Plasma desktop, Plasma's Qt platform integration supplies the KDE file dialog for these. An untouched new window takes the opened file; otherwise the file opens in a new window. |
| Closing a document with changes | The window asks: Save, Discard or Cancel. |
| Machine picker sheet | A dialog: makers on the left, their machines on the right, a search field above. Down moves from the search field into the list; Return or a double click chooses. |
| Configuration window | A page with the machine, its model and ROM, a Hardware section, a Media section and the command line. MAME's errors show in an error message above the form, and Run is not available then. MAME's warnings show in an information message. A media row with a problem shows it in red under the row: a file the machine needs, a file that is not found (the row shows its full path), or an SD card image that MAME's SD card cannot use. Choose refuses such an SD card image and says why, and a configuration that holds one cannot run. The file dialog shows the device's file extensions first, and All files. |
| Machine > Run (Cmd+R) | Machine > Run (Ctrl+R), also in the page's toolbar |
| Machine > Run with Debugger (Opt+Cmd+R) | Machine > Run with Debugger (Ctrl+Shift+R), also in the page's toolbar |
| Machine > Stop (Cmd+.) | Machine > Stop (Ctrl+.) |
| Machine > Copy Command Line | Machine > Copy Command Line, and a button under the command line |
| Settings window | Settings > Configure Pi/Ne-Sy: a window with a General page and a Debugger page |
| Settings in UserDefaults | Settings in KConfig, in `pinesyrc` |
| Data folder `~/Library/Application Support/Pinesy` | Data folder `~/.local/share/pinesy` (`QStandardPaths::AppDataLocation`) |

### Running a machine

The machine runs in its own window. Each frame MAME draws is shown as a texture in a Qt Quick item. Keys go to MAME by their position (`QKeyEvent::nativeScanCode`, the XKB keycode), and typed characters go to MAME's own menus. The window tells MAME when it gains or loses the keyboard focus, and when its size changes. It takes the size MAME asks for, made smaller to fit the screen, centred the first time. Qt has no way to keep a window's shape while the user resizes it, so MAME draws the picture centred, with black bars.

One machine runs at a time. While it runs, configurations cannot be changed, and a configuration opened then waits for the run to end. Stop, or closing the machine window, asks MAME to exit; the window closes when MAME has. When MAME stops with an error, a message shows MAME's error lines. Quitting the app while a machine runs stops the machine first.

### Settings

- General: extra ROM folders, and the folder where MAME keeps its files. A folder with `;` in its path cannot be a ROM folder.
- Debugger: what Run with Debugger uses. The choices are the core's: MAME's debugger windows, or the GDB remote stub with its port (23946 by default, from 1 to 65535). The engine is built with MAME's Qt debugger, so MAME's own choice for its debugger windows is the Qt debugger. Its windows use the app's `QApplication`, and the app's windows stay live while the debugger has the machine stopped.

Settings apply as they change.

## Headless mode

Any arguments make the executable run MAME with them and no app user interface, through the core's headless call. A leading `--` is dropped. The exit code is MAME's.

```sh
<build-dir>/bin/pinesy-kde -- tbblue -video none -sound none -seconds_to_run 10
```

With `-video none` there is no window and Qt is not started. Otherwise the machine gets its own window, as in a run from the app, and closing it stops the machine. Qt sees only the program name, so no MAME option reaches it.

## Desktop file and MIME type

`com.stalepixels.pinesy.kde.desktop` starts the app. The app icon is `../icons/hicolor/<size>x<size>/apps/pinesy.png`, installed as `com.stalepixels.pinesy.kde.png`; the desktop file and the windows use that icon name. `com.stalepixels.pinesy.kde.xml` defines the MIME type `application/x-pinesy-configuration` for `*.pinesy` files, a kind of `application/json`, with the app icon.

The desktop file does not list the MIME type, so a file manager does not open `.pinesy` files with the app. Any argument starts headless mode, so a file path from a file manager would go to MAME as its command line.

## Known limits

- While the GDB remote stub waits for a client, Stop does not end the run.
- The engine has no pointer input, so the mouse does not reach the machine.
