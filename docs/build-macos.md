# Build Pinesy on macOS

## You need

- An Apple silicon Mac with macOS 26 or later.
- Xcode, or Xcode's command line tools, version 26 or later.
- From Homebrew: `python`, `ccache`, `ninja`, `glib` and `pkgconf`. For `gdb-pinesy`: `gmp`, `mpfr` and `bison` (macOS has bison 2.3, and GDB needs 3.0 or later).
- Network access on the first build.
- For the core tests only: `cmake`, from Homebrew.

## Build

```sh
git clone https://github.com/StalePixels/pinesy.git
cd pinesy
./build.sh
```

The first build downloads the submodules before it compiles, and prints nothing while it does. `build-gdb.sh` also downloads GDB's source, about 100 MB, and builds `gdb-pinesy` and `objcopy-pinesy`, which go in the app. `app/sparkle.sh` downloads Sparkle, the app's updater, from Sparkle's GitHub releases. Later builds get only what changed.

The app is `app/build/Pinesy.app`. To install it, copy it to `/Applications`.

## Update

```sh
git pull
./build.sh
```

## Run the core tests

```sh
cmake -S core -B build/core
cmake --build build/core
ctest --test-dir build/core --output-on-failure
```
