# pinesy

Pi/Ne-Sy: a Pi/Next-System emulator, built on MAME and QEMU

## Get the source

```sh
git clone https://github.com/StalePixels/pinesy.git
```

`build-mame.sh` gets the submodules (`mame`, `qemu` and `vendor/rpi-utils`) itself, at the commits this repo pins. It does this on every build, so a `git pull` needs no other step. To get them without a build, run `git submodule update --init`. Do not use `--recursive` or `--recurse-submodules`: they also get QEMU's firmware sources, which are large, and the build does not use them.

`vendor/gdb` is GDB's source, a shallow checkout of a release tag. `git submodule update --init` skips it. `build-gdb.sh` gets it, and builds `gdb-pinesy` and `objcopy-pinesy`. [docs/gdb.md](docs/gdb.md) says what it debugs and how.
