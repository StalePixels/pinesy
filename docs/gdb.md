# gdb-pinesy

`gdb-pinesy` is GDB 18.1, built from the upstream release without changes. It debugs both processors of the machine:

- The Next's Z80, through MAME's GDB stub.
- The Pi's ARM1176 (ARMv6), through QEMU's GDB stub, running bare metal or ARM GNU/Linux.

The name differs from `gdb` so it never clashes with a GDB from the system.

`objcopy-pinesy` is GNU binutils' `objcopy`, built for the same targets. The macOS app and a Linux build from source build it from the same source as `gdb-pinesy`, the binutils in `vendor/gdb`. The KDE and GNOME Flatpaks build it from the binutils 2.47 release tarball. It makes the Z80 ELF symbol files that `gdb-pinesy` reads (see [Symbol files for the Z80](#symbol-files-for-the-z80)). No other binutils program is built.

## Where it is

| Build | Path |
|---|---|
| macOS app | `Pinesy.app/Contents/MacOS/gdb-pinesy` and `objcopy-pinesy` |
| KDE and GNOME Flatpak | `/app/bin/gdb-pinesy` and `objcopy-pinesy` |
| Linux build from source | `build/gdb/install/bin/gdb-pinesy` and `objcopy-pinesy` |

## What is built in

`build-gdb.sh` configures GDB with `--target=z80-unknown-elf` and `--enable-targets=arm-none-eabi,arm-linux-gnueabihf`. Those are the only targets. `set architecture` lists the Z80 family (`z80`, `z80n` and others) and `armv6`.

Python is not built in. GDB's Python needs a Python installation and its standard library at run time, and an app bundle cannot rely on Homebrew's. Use GDB's command scripts (`-x`) and the machine interface (`--interpreter=mi`) for automation.

The macOS `gdb-pinesy` loads only libraries that macOS has. GMP and MPFR are linked into it. `objcopy-pinesy` loads only `libSystem` and `libz`.

## Connect to the Next

Start Pinesy with MAME's stub:

```sh
Pinesy -- tbblue -debug -debugger gdbstub -debugger_port 2159
```

With `-debug`, MAME stops the Next at its first instruction. Pinesy continues it there, so the Next runs from the start. To keep the Next waiting at its first instruction until a client continues it, put `--paused` before `--`:

```sh
Pinesy --paused -- tbblue -debug -debugger gdbstub -debugger_port 2159
```

Then:

```
$ gdb-pinesy
(gdb) target remote 127.0.0.1:2159
(gdb) info registers
(gdb) x/8i $pc
(gdb) detach
```

The stub takes one client at a time. After each client it listens again, so clients can connect one after another in the same run. A client that connects stops the Next. `detach` ends the session and lets the Next run, and so does a client that closes its connection without `detach`. Breakpoints and watchpoints that a client leaves set stay in MAME's debugger: when the Next reaches one, it stops and waits for the next client. [MAME's GDB stub document](../mame/docs/source/debugger/gdbstub.rst) has the rest.

## Symbol files for the Z80

The stub reports no relocation, so a symbol file must hold the code at the addresses it runs at.

GDB reads a Z80 ELF file (machine type Zilog Z80) directly, so a symbol file needs no other machine type as a carrier. An ARM ELF file does not work as a carrier:

- `file` makes GDB take the ARM architecture from the file and reject the Z80 target description from the stub.
- `symbol-file` avoids that, but GDB clears bit 0 of ARM code addresses. A symbol at an odd address, such as `0x69`, lands one byte early, and a `function` symbol there is read as Thumb code.

To make a Z80 symbol file from a raw binary, use `objcopy-pinesy`. macOS has no `objcopy`, and `llvm-objcopy` cannot write Z80 ELF files.

```sh
objcopy-pinesy -I binary -O elf32-z80 --binary-architecture=z80 \
    --change-section-address .data=0x8000 \
    --rename-section .data=.text,alloc,load,readonly,code \
    --add-symbol boot_continue=.text:0x69,global \
    boot.bin boot.elf
```

The symbol value after `.text:` is an offset in the section, so this puts `boot_continue` at `0x8069`. The section must be a code section (`.text`), or `break boot_continue` does not find the symbol. Load the file before connecting:

```
(gdb) file boot.elf
(gdb) target remote 127.0.0.1:2159
(gdb) break boot_continue
(gdb) continue
```

## Connect to the Pi

The Pi runs in QEMU, and QEMU has its own GDB stub. Two options turn it on. They are Pinesy's, not MAME's, and go after `--` with MAME's options:

- `-pi_gdb_port <port>`: the port of the Pi's stub. The stub listens at the address of MAME's `-debugger_host` option, `localhost` unless you set it. `0`, the default, means no stub.
- `-pi_gdb_wait`: the Pi waits at its first instruction until a client or the Continue button continues it. It works with or without `-pi_gdb_port`. `--paused-pi` before `--` adds this option.

```sh
Pinesy --paused-pi -- tbblue -rs232_rpi nextpi -pi nextpi.img -pi_gdb_port 2160
```

Then connect with `target remote 127.0.0.1:2160`.

QEMU starts once in each Pinesy process, when the Pi's SD card image loads, and the options take effect at that moment only. When the port is in use at any address of the host, or is the Next's GDB port, the image does not load, and MAME says why.

A client that connects stops the Pi, and `detach` lets it run. A client that closes its connection without `detach` leaves the Pi stopped until a client or the Continue button continues it.

GDB reads the OS ABI from the ELF file you load. A bare-metal ARM ELF file has the OS ABI `none`. An ELF file with a `.note.ABI-tag` section gets `GNU/Linux`. `set osabi` overrides the choice.

## In the app

Run with Debugger starts the stubs; Run does not. The Debugger settings have:

- The debugger: MAME's debugger windows, or MAME's GDB stub with its port.
- The Pi GDB port. A run with the debugger gets a Pi stub on this port when the configuration has a Pi SD card.
- Start paused: the Next waits at its first instruction, as with `--paused`.

The configuration window shows "Pi starts paused" next to the Pi's SD card, when the configuration has one. It works as `--paused-pi`. Copy Command Line gives the command line of Run, without the debugger: it has no switches and no debugger options.

The machine window shows a Continue button while the Next is stopped in MAME's debugger, or the Pi waits: at its first instruction, or stopped by its GDB stub. It continues both. A GDB client that has the Next or the Pi stopped is not told. A client of the Next finds the Next running when it next asks.

## Limits

- `kill` from a client of the Next makes MAME exit, and the run ends.
- `kill` from a client of the Pi stops QEMU. The Pi stays off until Pinesy starts again. The Next runs on.
- A client of the Pi that closes its connection without `detach` leaves the Pi stopped.
- In the app, the stubs run only with Run with Debugger. On the command line, MAME's stub needs `-debug -debugger gdbstub`, and the Pi's stub needs only `-pi_gdb_port`.
- The client must know the Z80 and ARM. Use `gdb-pinesy`, or another GDB built with both targets.

## Data directory

The data directory (`show data-directory`) holds the system call tables for `catch syscall`. The macOS app does not carry it. Without it, GDB works as usual, and `catch syscall` cannot name system calls. Point `-data-directory` at a copy of `share/gdb` from `build/gdb/install` to get them.

## Build it

```sh
./build-gdb.sh [--prefix DIR]
```

The source is `vendor/gdb`, a shallow checkout of the `gdb-18.1-release` tag. It holds GDB and the binutils from the same snapshot. The script fetches it, builds out of tree in `build/gdb/obj` and installs `gdb-pinesy` and `objcopy-pinesy` into `build/gdb/install/bin` (or `DIR/bin`). What it needs is in [build-macos.md](build-macos.md) and [build-linux.md](build-linux.md). On Linux it first checks that the system has all of it, and names the package of your distribution for each missing part. `./build-gdb.sh --check` runs only that check.

To change the GDB version, move the `vendor/gdb` pin to another `gdb-*-release` tag, and change the `gdb-18.1.tar.xz` archive (its `url` and `sha256`) in `kde/flatpak/com.stalepixels.pinesy.kde.yml` and `gtk/flatpak/com.stalepixels.Pinesy.yml` to the same release. The manifests build `objcopy-pinesy` from `binutils-2.47.tar.xz`, because GDB's tarball has no `binutils` folder. When the pin moves, use the binutils release that is closest to it.
