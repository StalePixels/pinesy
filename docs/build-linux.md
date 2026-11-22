# Build Pinesy on Linux

There are two apps: one for GNOME and one for KDE Plasma. Build the one for your desktop.

## You need

For both apps:

- `clang` and `ccache`
- GNU `make`, `cmake` 3.20 or later, `ninja`, `pkg-config` and `git`
- `python3` with its `venv` module
- The development files for GLib 2, ALSA, PulseAudio and PipeWire
- For `gdb-pinesy` and `objcopy-pinesy`: `gcc` and `g++` (GDB's build does not look for clang), `bison` 3.0 or later, `flex`, and the development files for GMP, MPFR, expat, zlib and ncurses
- Network access on the first build

For the GNOME app:

- The development files for GTK 4, gtkmm 4.10 or later, libadwaita 1.6 or later and libsoup 3

For the KDE app:

- Qt 6.8 or later: the development files for the Qt modules qtbase (Core, DBus, Gui, Network, Widgets) and qtdeclarative (Qml, Quick, QuickControls2), and `qmake6` on the `PATH`
- extra-cmake-modules (CMake calls it ECM)
- KDE Frameworks 6.8 or later: Kirigami, KConfig, KCoreAddons and KI18n

## Get the source

```sh
git clone https://github.com/StalePixels/pinesy.git
cd pinesy
```

The first build downloads the submodules before it compiles, and prints nothing while it does.

Before it builds anything, `build.sh` checks that the system has what the engine and `gdb-pinesy` need. If something is missing, it names the package of your distribution for each missing part, and stops. Otherwise it builds the engine, then the app, and `gdb-pinesy` and `objcopy-pinesy` last. They go in `build/gdb/install/bin`.

## Build the GNOME app

```sh
./build.sh gtk
```

Run it:

```sh
GSETTINGS_SCHEMA_DIR=build/gtk/schemas build/gtk/pinesy
```

## Build the KDE app

```sh
./build.sh kde
```

If your Qt has no `qmake6`, build in steps and give Qt's install prefix:

```sh
./build-mame.sh --qt QT_HOME=<prefix>
cmake -S kde -B build/kde
cmake --build build/kde
```

Run it:

```sh
build/kde/bin/pinesy-kde
```

## Install a build

`cmake --install` copies a built app into an install prefix:

```sh
cmake --install build/gtk --prefix <prefix>
cmake --install build/kde --prefix <prefix>
```

It installs:

- the app, in `<prefix>/bin`: `pinesy` (GNOME) or `pinesy-kde` (KDE)
- `libpinesy.so` and the engine library, in `<prefix>/lib/pinesy` (GNOME) or `<prefix>/lib/pinesy-kde` (KDE)
- the resources folder, in `<prefix>/share/pinesy/resources` (GNOME) or `<prefix>/share/pinesy-kde/resources` (KDE): the ROM sets from `roms` and MAME's `plugins`, `hash`, `bgfx`, `artwork` and `language` folders. A file in `roms` that is not a ROM set (a zip, 7z or folder of ROM dumps), or a ROM set that holds a disk image, stops the install.
- the desktop entry, the MIME type for `*.pinesy` files, the D-Bus service file and the app icon, under `<prefix>/share`
- for the GNOME app, the settings schema, compiled in `<prefix>/share/glib-2.0/schemas`

Each app has its own library and resources folders, so both can be installed in one prefix. The installed app loads its libraries from its library folder and reads its resources folder from beside itself. The GNOME app also reads its settings schema from `<prefix>/share/glib-2.0/schemas`, so it needs no `GSETTINGS_SCHEMA_DIR`. Nothing in the build folder is used, and the prefix can move as a whole.

The desktop finds the desktop entry, the MIME type and the D-Bus service file only when `<prefix>/share` is in `XDG_DATA_DIRS`, as `/usr/local/share` and `/usr/share` are. The D-Bus service file holds the program's full path under `CMAKE_INSTALL_PREFIX` as it was when CMake configured the build. `--prefix` does not change that path, so configure with `-DCMAKE_INSTALL_PREFIX=<prefix>` when the desktop starts the app from that prefix.

## Update

```sh
git pull
```

Then run the same build command again.

## Run the core tests

```sh
./build-mame.sh
cmake -S core -B build/core
cmake --build build/core
ctest --test-dir build/core --output-on-failure
```

## Flatpak packages

A Flatpak package needs Flatpak 1.15.6 or later and `flatpak-builder`. Older versions of Flatpak cannot build it.

```sh
git submodule update --init
flatpak --user remote-add --if-not-exists flathub https://dl.flathub.org/repo/flathub.flatpakrepo
```

GNOME:

```sh
flatpak-builder --user --install --install-deps-from=flathub --force-clean --ccache \
    --state-dir=<state-dir> <build-dir> gtk/flatpak/com.stalepixels.Pinesy.yml
flatpak run com.stalepixels.Pinesy
```

KDE:

```sh
flatpak-builder --user --install --install-deps-from=flathub --force-clean --ccache \
    --state-dir=<state-dir> <build-dir> kde/flatpak/com.stalepixels.pinesy.kde.yml
flatpak run com.stalepixels.pinesy.kde
```

Put `<state-dir>` and `<build-dir>` outside the `pinesy` folder. Run `git submodule update --init` again after every `git pull`.

Each package has `gdb-pinesy` and `objcopy-pinesy` in `/app/bin`. The manifest builds `gdb-pinesy` from GDB's release tarball, with GMP and MPFR built beside it, and `objcopy-pinesy` from the binutils release tarball. A package build needs no `bison` or `flex`.

A package runs only on processors like the one that built it, because the build optimises for that processor.

## Debian packages

On Debian, `cmake/make-deb.sh` makes a `.deb` from a build that `build.sh` made, without compiling. It also needs `dpkg-dev`, for `dpkg-shlibdeps`. There are three packages: `pinesy` (the GNOME app, from `build/gtk`), `pinesy-kde` (the KDE app, from `build/kde`) and `pinesy-gdb` (`gdb-pinesy` and `objcopy-pinesy`), which both apps depend on. Build the apps first, as above, then:

```sh
cmake/make-deb.sh gtk <out-dir>
cmake/make-deb.sh kde <out-dir>
cmake/make-deb.sh gdb <out-dir>
```

The script installs the build into a staging folder with the prefix `/usr`, and leaves out the compiled schemas file: the system compiles its schemas when the package is installed. A package made like this has version 0 and no track, so it never looks for updates. CI gives its packages a version and a track (`../app/README.md`, "Updates"): `cmake/make-deb.sh <app> <out-dir> <version> <track>`.
