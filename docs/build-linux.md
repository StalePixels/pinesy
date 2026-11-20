# Build Pinesy on Linux

There are two apps: one for GNOME and one for KDE Plasma. Build the one for your desktop.

## You need

For both apps:

- `clang` and `ccache`
- GNU `make`, `cmake` 3.20 or later, `ninja`, `pkg-config` and `git`
- `python3` with its `venv` module
- The development files for GLib 2, ALSA, PulseAudio and PipeWire
- For `gdb-pinesy` and `objcopy-pinesy`: a C++17 compiler, `bison` 3.0 or later, `flex`, and the development files for GMP, MPFR, expat, zlib and ncurses
- Network access on the first build

For the GNOME app:

- The development files for GTK 4, gtkmm 4.10 or later and libadwaita 1.6 or later

For the KDE app:

- Qt 6.8 or later: the development files for the Qt modules qtbase (Core, Gui, Widgets) and qtdeclarative (Qml, Quick, QuickControls2), and `qmake6` on the `PATH`
- extra-cmake-modules (CMake calls it ECM)
- KDE Frameworks 6.8 or later: Kirigami, KConfig, KCoreAddons and KI18n

## Get the source

```sh
git clone https://github.com/StalePixels/pinesy.git
cd pinesy
```

The first build downloads the submodules before it compiles, and prints nothing while it does.

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
