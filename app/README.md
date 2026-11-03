# Pinesy.app

The macOS app for Pi/Ne-Sy. It is a SwiftUI app that links `libmame_pinesy.dylib`, the MAME library built from `../mame` with the `pinesy` OSD. The C interface is `../mame/src/osd/pinesy/pinesy.h`.

## Build

Build `../mame/libmame_pinesy.dylib` first, then:

```sh
app/build.sh [output-dir]
```

The default output directory is `app/build`. `MAME_LIB=<path>` bundles a different library. The script needs Xcode's command line tools (`swiftc`, `install_name_tool`, `codesign`) and nothing else.

The script copies the MAME library and every library it loads from outside `/usr/lib` and `/System` (from Homebrew: glib, libintl and pcre2) into `Contents/Frameworks`. It rewrites their install names to `@rpath/<name>` and removes their search paths. The executable searches `@executable_path/../Frameworks`. The build fails if a binary in the bundle still loads a library from outside it. The bundle is signed ad hoc.

## Run

Open `Pinesy.app`. The Machine menu lists the machines in the library and runs the one you choose in a window. Close the window to stop the machine. Pi emulation is in Settings: when it is on, the machine gets `-pi <SD image>`; when it is off, it gets `-rs232_rpi ""`, which leaves the Pi port empty.

## Headless

Any arguments make the executable run MAME with them and no UI. A leading `--` is dropped. Every option goes to MAME unchanged, and the exit code is MAME's.

```sh
Pinesy.app/Contents/MacOS/Pinesy -- tbblue -video none -sound none -seconds_to_run 10
```

## ROMs and settings

MAME looks for `mame.ini` in `~/Library/Application Support/mame`, then `~/.mame`, then the current directory. ROMs come from the `rompath` that file sets; without one, from `roms` in the current directory.
