// Swift wrappers around libpinesy, the core the front ends share
// (pinesy_core.h). The core holds every rule; this file only moves
// values across the C interface. Command lines and the core's results
// cross it as JSON text.

import Foundation
import CPinesy

@MainActor
enum Core {
    /// The app's core. Its resources folder is the bundle's Resources
    /// folder, and its default data folder ~/Library/Application Support/Pinesy.
    private static let core: OpaquePointer = {
        let resources = Bundle.main.resourceURL ?? Bundle.main.bundleURL
        guard let core = pinesy_core_create(resources.path, AppSettings.defaultDataFolder.path) else {
            fatalError("The core could not be made.")
        }
        return core
    }()

    /// The core, with the settings as they are now in UserDefaults.
    private static var current: OpaquePointer {
        _ = settingsJSON.withCString { pinesy_core_set_settings(core, $0) }
        return core
    }

    /// The settings as pinesy_core_set_settings() takes them. The core
    /// gives a missing or unusable value its default.
    private static var settingsJSON: String {
        let defaults = UserDefaults.standard
        var settings: [String: Any] = [
            "romFolders": defaults.stringArray(forKey: AppSettings.Key.romFolders) ?? [],
            "dataFolder": defaults.string(forKey: AppSettings.Key.dataFolder) ?? "",
            "debugger": defaults.string(forKey: AppSettings.Key.debugger) ?? "",
        ]
        if defaults.object(forKey: AppSettings.Key.gdbPort) != nil {
            settings["gdbPort"] = defaults.integer(forKey: AppSettings.Key.gdbPort)
        }
        settings["startPaused"] = defaults.bool(forKey: AppSettings.Key.startPaused)
        if defaults.object(forKey: AppSettings.Key.piGDBPort) != nil {
            settings["piGdbPort"] = defaults.integer(forKey: AppSettings.Key.piGDBPort)
        }
        return encode(settings)
    }

    // MARK: - The catalogue

    static func catalogue() -> [Catalogue.Section] {
        decode(pinesy_core_catalogue(core)) ?? []
    }

    static func search(_ text: String) -> [Catalogue.Machine] {
        decode(pinesy_core_search(core, text)) ?? []
    }

    // MARK: - Configurations

    /// A new configuration for a machine; empty when the catalogue has no
    /// machine with that id.
    static func newConfiguration(_ machine: String) -> [String] {
        decode(pinesy_core_new_configuration(core, machine)) ?? []
    }

    static func setDriver(_ arguments: [String], to driver: String) -> [String] {
        edit(arguments) { pinesy_core_set_driver(current, $0, driver) }
    }

    static func setROM(_ arguments: [String], driver: String, bios: String?) -> [String] {
        edit(arguments) { args in
            if let bios {
                return pinesy_core_set_rom(current, args, driver, bios)
            }
            return pinesy_core_set_rom(current, args, driver, nil)
        }
    }

    static func setSlot(_ arguments: [String], slot: String, card: String) -> [String] {
        edit(arguments) { pinesy_core_set_slot(current, $0, slot, card) }
    }

    static func setCardROM(_ arguments: [String], slot: String, bios: String) -> [String] {
        edit(arguments) { pinesy_core_set_card_rom(current, $0, slot, bios) }
    }

    static func setMedia(_ arguments: [String], media: String, path: String?) -> [String] {
        edit(arguments) { args in
            if let path {
                return pinesy_core_set_media(args, media, path)
            }
            return pinesy_core_set_media(args, media, nil)
        }
    }

    /// What the configuration window shows for a command line.
    static func summary(_ arguments: [String], machineHint: String) -> ConfigurationSummary? {
        decode(pinesy_core_summary(current, encode(arguments), machineHint))
    }

    /// Why MAME's SD card cannot use the image at a path, or nil.
    nonisolated static func sdCardProblem(_ path: String) -> String? {
        let problem = take(pinesy_core_sd_card_problem(path)) ?? ""
        return problem.isEmpty ? nil : problem
    }

    // MARK: - Settings

    nonisolated static func isUsableROMFolder(_ path: String) -> Bool {
        pinesy_core_is_usable_rom_folder(path) != 0
    }

    // MARK: - The machine window

    /// Window sizes: full screen, fit to the screen, or a magnification of 1 or more.
    nonisolated static let windowFullScreen = Int(PINESY_CORE_WINDOW_FULL_SCREEN)
    nonisolated static let windowFit = Int(PINESY_CORE_WINDOW_FIT)
    /// The picker names the magnifications from 1 to this; Custom stands for any other.
    nonisolated static let listedWindowMagnifications = Int(PINESY_CORE_WINDOW_LISTED_MAGNIFICATIONS)
    nonisolated static let windowCustom = Int(PINESY_CORE_WINDOW_CUSTOM)

    /// The picker choice that shows a window size; with `customChosen`,
    /// every magnification shows as Custom.
    nonisolated static func windowChoice(_ windowSize: Int, customChosen: Bool) -> Int {
        Int(pinesy_core_window_choice(Int32(clamping: windowSize), customChosen ? 1 : 0))
    }

    /// The window size a picker choice gives, from the current window
    /// size; for Custom, the current size is the number the user entered.
    nonisolated static func windowSize(choice: Int, current: Int) -> Int {
        Int(pinesy_core_window_size_for_choice(Int32(clamping: choice), Int32(clamping: current)))
    }

    /// Whether the machine window opens in full screen.
    nonisolated static func windowOpensFullScreen(_ windowSize: Int) -> Bool {
        pinesy_core_window_full_screen(Int32(clamping: windowSize)) != 0
    }

    /// The machine window's content size, in points, for a frame of
    /// `frame` pixels: the core's rule, from the configuration's window
    /// size, the screen's pixels per point and the room the content has
    /// on the screen, in points (zero when the screen is not known).
    nonisolated static func windowContentSize(frame: NSSize, windowSize: Int, pixelRatio: CGFloat, usable: NSSize) -> NSSize? {
        var width: Int32 = 0
        var height: Int32 = 0
        let multiple = pinesy_core_window_size(Int32(frame.width), Int32(frame.height), Int32(clamping: windowSize), Double(pixelRatio),
                                               Int32(usable.width), Int32(usable.height), &width, &height)
        guard multiple >= 0 else { return nil }
        return NSSize(width: Int(width), height: Int(height))
    }

    // MARK: - NVRAM

    /// Deletes the NVRAM MAME saved for the configuration's driver, and
    /// nothing else. False when something could not be deleted.
    static func clearNVRAM(_ arguments: [String]) -> Bool {
        pinesy_core_clear_nvram(current, encode(arguments)) == 0
    }

    // MARK: - Runs

    /// Runs a configuration with MAME's fixed options, the mouse option
    /// and, with debug, the debugger options and switches from the
    /// settings. `takesMouse` is the summary's mouse and `mouse` the
    /// configuration's mouse setting; the core turns them into the mouse
    /// option. `piPaused` is the configuration's Pi pause setting.
    /// Returns MAME's exit code.
    static func run(_ arguments: [String], debug: Bool, takesMouse: Bool, mouse: Bool, piPaused: Bool,
                    host: UnsafePointer<pinesy_host>?) -> Int32 {
        pinesy_core_run(current, encode(arguments), debug ? 1 : 0, takesMouse ? 1 : 0, mouse ? 1 : 0, piPaused ? 1 : 0, host)
    }

    /// The headless command for a configuration, quoted for zsh, with
    /// the mouse option as run() has it.
    static func copyCommandLine(_ arguments: [String], debug: Bool = false, takesMouse: Bool, mouse: Bool) -> String {
        let executable = Bundle.main.executablePath ?? "Pinesy"
        return take(pinesy_core_copy_command_line(current, executable, encode(arguments), debug ? 1 : 0,
                                                  takesMouse ? 1 : 0, mouse ? 1 : 0)) ?? ""
    }

    // MARK: - Headless mode

    static func videoNone(_ arguments: [String]) -> Bool {
        withCStrings(arguments) { pinesy_core_video_none($0, $1) != 0 }
    }

    static func opensDebuggerWindows(_ arguments: [String]) -> Bool {
        withCStrings(arguments) { pinesy_core_opens_debugger_windows($0, $1) != 0 }
    }

    /// Runs headless mode with the executable's arguments. Returns MAME's exit code.
    static func headless(_ arguments: [String], host: UnsafePointer<pinesy_host>?) -> Int32 {
        let core = current
        return withCStrings(arguments) { pinesy_core_headless(core, $0, $1, host) }
    }

    // MARK: - Crossing the C interface

    /// Takes text the core returned, and frees it.
    nonisolated static func take(_ text: UnsafeMutablePointer<CChar>?) -> String? {
        guard let text else { return nil }
        defer { pinesy_core_free(text) }
        return String(cString: text)
    }

    nonisolated static func decode<T: Decodable>(_ text: UnsafeMutablePointer<CChar>?) -> T? {
        guard let string = take(text) else { return nil }
        return try? JSONDecoder().decode(T.self, from: Data(string.utf8))
    }

    nonisolated static func encode(_ value: Any) -> String {
        guard let data = try? JSONSerialization.data(withJSONObject: value, options: [.withoutEscapingSlashes]) else {
            return "null"
        }
        return String(decoding: data, as: UTF8.self)
    }

    /// An edit of a command line; the command line stays as it is when
    /// the core refuses the edit.
    private static func edit(_ arguments: [String], _ body: (String) -> UnsafeMutablePointer<CChar>?) -> [String] {
        decode(body(encode(arguments))) ?? arguments
    }

    private static func withCStrings<R>(_ strings: [String], _ body: (Int32, UnsafePointer<UnsafePointer<CChar>?>?) -> R) -> R {
        let cStrings = strings.map { strdup($0) }
        defer { cStrings.forEach { free($0) } }
        let argv: [UnsafePointer<CChar>?] = cStrings.map { UnsafePointer($0) }
        return argv.withUnsafeBufferPointer { body(Int32($0.count), $0.baseAddress) }
    }
}
