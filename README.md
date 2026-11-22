# pinesy

Pi/Ne-Sy: a Pi/Next-System emulator, built on MAME and QEMU

Pinesy is one desktop app for running, testing and debugging ZX software. Each app holds MAME, QEMU for the Next's Raspberry Pi, its user interface, and `gdb-pinesy` and `objcopy-pinesy` for the GDB stubs. It has no assembler: bring your own.

## The parts

- `engine`: MAME as a library, with the `pinesy` OSD. See [engine/README.md](engine/README.md).
- `core`: `libpinesy`, every rule the apps share. See [core/README.md](core/README.md).
- `app`: the macOS app, in SwiftUI. Its README describes what every app does. See [app/README.md](app/README.md).
- `gtk`: the GNOME app, in GTK 4 and libadwaita. See [gtk/README.md](gtk/README.md).
- `kde`: the KDE Plasma app, in Qt 6 and Kirigami. See [kde/README.md](kde/README.md).
- `debuggers/gtk`: MAME's debugger windows for the GNOME app.

Each app follows its own desktop's guidelines: Apple's Human Interface Guidelines, the GNOME HIG and the KDE HIG.

## Get the source

```sh
git clone https://github.com/StalePixels/pinesy.git
```

`build-mame.sh` gets the submodules (`mame`, `qemu` and `vendor/rpi-utils`) itself, at the commits this repo pins. It does this on every build, so a `git pull` needs no other step. To get them without a build, run `git submodule update --init`. Do not use `--recursive` or `--recurse-submodules`: they also get QEMU's firmware sources, which are large, and the build does not use them.

`vendor/gdb` is GDB's source, a shallow checkout of a release tag. `git submodule update --init` skips it. `build-gdb.sh` gets it, and builds `gdb-pinesy` and `objcopy-pinesy`. [docs/gdb.md](docs/gdb.md) says what it debugs and how.
