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

The core is C++20. Its C interface is `include/pinesy_core.h`. That interface wraps the C++ interface in `include/pinesy_core.hpp`.

Every front end uses the core:

- The macOS app (`../app`) imports `pinesy_core.h` into Swift. `../app/build.sh` compiles the core's sources with `clang++` into `libpinesy.dylib` and bundles it in `Contents/Frameworks`, next to the engine.
- The GNOME app (`../gtk`) uses the C++ interface, and the KDE app (`../kde`) the C interface. Their CMake builds build the core from `CMakeLists.txt`.

To build it and run its tests, see [docs/build-macos.md](../docs/build-macos.md) or [docs/build-linux.md](../docs/build-linux.md).

## The interface

`pinesy_core.h` documents every function. In outline:

- **A core.** `pinesy_core_create(resources_folder, default_data_folder)` makes one, and `pinesy_core_destroy()` frees it. The two folders are the platform's part. The resources folder holds the bundled ROM sets in its `roms` folder, and MAME's search folders (`hash`, `plugins` and others). On macOS it is the bundle's `Resources` folder. The default data folder is `~/Library/Application Support/Pinesy` on macOS and `$XDG_DATA_HOME/pinesy` (by default `~/.local/share/pinesy`) on Linux.
- **Text.** A command line is a JSON array of strings: the driver, then options, as in a `.pinesy` file. Calls that return `char *` return text that the caller frees with `pinesy_core_free()`. They return `NULL` when an argument is `NULL` or is not the JSON it must be.
- **Settings.** The front end stores the settings and gives them to the core with `pinesy_core_set_settings()`, as JSON with `romFolders`, `dataFolder`, `debugger` (`windows` or `gdbStub`) and `gdbPort`. A missing or unusable value takes its default.
- **Paths.** `pinesy_core_fixed_options()` gives the options every run and every check starts with: `-noreadconfig`, `-rompath`, MAME's search paths, the folders MAME writes, and `-skip_gameinfo`. `pinesy_core_rom_folders()` gives the ROM folders in search order. `pinesy_core_create_folders()` creates the folders MAME writes.
- **The catalogue.** `pinesy_core_catalogue()` gives the machine list in sections. `pinesy_core_search()` finds machines by name, maker, model or ROM variant name. `pinesy_core_machine_for_driver()` gives the machine a driver belongs to. `pinesy_core_starting_arguments()` gives the command line a new configuration for a driver starts with.
- **Configurations.** `pinesy_core_new_configuration()` starts a configuration for a machine. `pinesy_core_set_driver()`, `pinesy_core_set_rom()`, `pinesy_core_set_slot()`, `pinesy_core_set_card_rom()` and `pinesy_core_set_media()` edit one. Each edit except `pinesy_core_set_media()` checks the result with the engine and removes the options the machine no longer has, as `pinesy_core_normalise()` does.
- **Checking.** `pinesy_core_summary()` asks the engine about a configuration and gives what the configuration window shows: the machine, its models and ROMs, a row for each slot and each media device, the engine's errors and warnings, mandatory media with no file, media files that are not there, SD card images that MAME's SD card cannot use, and whether it can run. A configuration with such an image cannot run. `pinesy_core_sd_card_problem()` gives the reason for one image, so a front end can refuse it when it is chosen.
- **Files.** `pinesy_core_read_document()` reads a `.pinesy` file's text, and `pinesy_core_write_document()` writes it.
- **Runs.** `pinesy_core_run()` runs a configuration with `pinesy_run()`, with the fixed options first and the debugger options from the settings last. `pinesy_core_copy_command_line()` gives the headless command for a configuration, quoted for zsh.
- **Headless mode.** `pinesy_core_headless()` runs the executable's arguments as headless mode does. `pinesy_core_video_none()` and `pinesy_core_opens_debugger_windows()` tell the front end whether MAME needs a window, and whether MAME's debugger windows will open.

Calls that ask the engine, which the header marks, run on the main thread and never while a machine runs.
