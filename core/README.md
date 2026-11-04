# libpinesy

libpinesy is the core library of the Pi/Ne-Sy front ends. It holds every rule the front ends share, so each front end holds only its UI:

- the machine catalogue, built from the engine's driver list
- configurations: the command line model, its edits, and checking it with the engine
- `.pinesy` files
- the starting configuration of a new machine
- the options and search paths every run starts with
- the settings schema, and the options settings add to a run
- the headless command line that Copy Command Line gives, quoted for zsh
- headless mode

The rules are the ones `../app/README.md` describes. The core calls the engine, `libmame_pinesy`, through its C interface `../engine/include/pinesy.h`. It opens no windows.

The core is C++20. Its C interface is `include/pinesy_core.h`, so Swift can import it. That interface wraps the C++ interface in `include/pinesy_core.hpp`.

## Build

Build the engine first (`../mame/libmame_pinesy.so` on Linux, `../mame/libmame_pinesy.dylib` on macOS). Then, from the repository root:

```sh
cmake -S core -B <build-dir>
cmake --build <build-dir>
```

This needs CMake 3.20 or later and a C++20 compiler. The result is `libpinesy` in the build folder.

The cache variables are:

- `PINESY_ENGINE_LIBRARY`: the engine library. The default is `../mame/libmame_pinesy.so`, or `.dylib` on macOS.
- `PINESY_ENGINE_INCLUDE_DIR`: the folder with `pinesy.h`. The default is `../engine/include`.
- `PINESY_CORE_TESTS`: build the tests. On by default.
- `PINESY_TEST_RESOURCES`: the resources folder the tests use. The default is the repository root, so the ROM sets in `../roms` are the bundled ROMs.

## Test

```sh
ctest --test-dir <build-dir> --output-on-failure
```

The tests run the rules against the real engine:

- `rules` checks the catalogue, new configurations, `.pinesy` files, checking with the engine, the fixed options, settings and the copied command line.
- `headless` runs `tbblue` in headless mode with `-video none -sound none -seconds_to_run 2`.

Each check prints `ok` or `FAIL` and names the line of `../app/README.md` or of the Swift sources that its expected value comes from. MAME writes only in `testdata` in the build folder, which is the tests' data folder. When `timeout` (or `gtimeout` on macOS) is installed, each test runs under it.

## The interface

`pinesy_core.h` documents every function. In outline:

- **A core.** `pinesy_core_create(resources_folder, default_data_folder)` makes one, and `pinesy_core_destroy()` frees it. The two folders are the platform's part. The resources folder holds the bundled ROM sets in its `roms` folder, and MAME's search folders (`hash`, `plugins` and others). On macOS it is the bundle's `Resources` folder. The default data folder is `~/Library/Application Support/Pinesy` on macOS and `$XDG_DATA_HOME/pinesy` (by default `~/.local/share/pinesy`) on Linux.
- **Text.** A command line is a JSON array of strings: the driver, then options, as in a `.pinesy` file. Calls that return `char *` return text that the caller frees with `pinesy_core_free()`. They return `NULL` when an argument is `NULL` or is not the JSON it must be.
- **Settings.** The front end stores the settings and gives them to the core with `pinesy_core_set_settings()`, as JSON with `romFolders`, `dataFolder`, `debugger` (`windows` or `gdbStub`) and `gdbPort`. A missing or unusable value takes its default.
- **Paths.** `pinesy_core_fixed_options()` gives the options every run and every check starts with: `-noreadconfig`, `-rompath`, MAME's search paths, and the folders MAME writes. `pinesy_core_rom_folders()` gives the ROM folders in search order. `pinesy_core_create_folders()` creates the folders MAME writes.
- **The catalogue.** `pinesy_core_catalogue()` gives the machine list in sections. `pinesy_core_search()` finds machines by name, maker, model or ROM variant name. `pinesy_core_machine_for_driver()` gives the machine a driver belongs to. `pinesy_core_starting_arguments()` gives the command line a new configuration for a driver starts with.
- **Configurations.** `pinesy_core_new_configuration()` starts a configuration for a machine. `pinesy_core_set_driver()`, `pinesy_core_set_rom()`, `pinesy_core_set_slot()`, `pinesy_core_set_card_rom()` and `pinesy_core_set_media()` edit one. Each edit except `pinesy_core_set_media()` checks the result with the engine and removes the options the machine no longer has, as `pinesy_core_normalise()` does.
- **Checking.** `pinesy_core_summary()` asks the engine about a configuration and gives what the configuration window shows: the machine, its models and ROMs, a row for each slot and each media device, the engine's errors and warnings, mandatory media with no file, media files that are not there, and whether it can run.
- **Files.** `pinesy_core_read_document()` reads a `.pinesy` file's text, and `pinesy_core_write_document()` writes it.
- **Runs.** `pinesy_core_run()` runs a configuration with `pinesy_run()`, with the fixed options first and the debugger options from the settings last. `pinesy_core_copy_command_line()` gives the headless command for a configuration, quoted for zsh.
- **Headless mode.** `pinesy_core_headless()` runs the executable's arguments as headless mode does. `pinesy_core_video_none()` and `pinesy_core_opens_debugger_windows()` tell the front end whether MAME needs a window, and whether MAME's debugger windows will open.

Calls that ask the engine, which the header marks, run on the main thread and never while a machine runs.
