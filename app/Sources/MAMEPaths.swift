// Where MAME finds and keeps its files. Pinesy is not MAME: it reads no
// mame.ini and uses nothing in ~/.mame. Every call into MAME that parses
// options starts with these, so a later option on the same command line
// (an agent's -cfg_directory for one run, say) still takes precedence.

import Foundation

enum MAMEPaths {
    /// The ROM sets that ship with the app.
    static var bundledROMs: URL {
        resources.appendingPathComponent("roms", isDirectory: true)
    }

    /// The user's own ROM folder, in the data folder, searched after the app's.
    static func userROMs(_ dataFolder: URL) -> URL {
        dataFolder.appendingPathComponent("roms", isDirectory: true)
    }

    private static var resources: URL {
        Bundle.main.resourceURL ?? Bundle.main.bundleURL
    }

    /// Folders MAME searches for its own files, in the app's resources.
    private static let searchPaths: [(option: String, folder: String)] = [
        ("hashpath", "hash"), ("samplepath", "samples"), ("artpath", "artwork"),
        ("ctrlrpath", "ctrlr"), ("fontpath", "fonts"), ("cheatpath", "cheat"),
        ("crosshairpath", "crosshair"), ("pluginspath", "plugins"),
        ("languagepath", "language"), ("swpath", "software"), ("bgfx_path", "bgfx"),
    ]

    /// Folders MAME writes, in the data folder from Settings.
    private static let dataFolders: [(option: String, folder: String)] = [
        ("inipath", "ini"), ("homepath", "home"),
        ("cfg_directory", "cfg"), ("nvram_directory", "nvram"),
        ("input_directory", "inp"), ("state_directory", "sta"),
        ("snapshot_directory", "snap"), ("diff_directory", "diff"),
        ("comment_directory", "comments"), ("share_directory", "share"),
    ]

    /// The ROM folders, in search order: the app's, the user's, then
    /// the folders added in Settings. MAME separates the folders with
    /// ";", so a folder with ";" in its path is left out.
    static func romFolders(_ settings: AppSettings) -> [String] {
        ([bundledROMs.path, userROMs(settings.dataFolder).path] + settings.romFolders).filter(isUsableROMFolder)
    }

    static func isUsableROMFolder(_ path: String) -> Bool {
        !path.contains(";")
    }

    /// The options that give MAME the app's own paths.
    static func options(_ settings: AppSettings = .current) -> [String] {
        var result = ["-noreadconfig", "-rompath", romFolders(settings).joined(separator: ";")]
        for path in searchPaths {
            result += ["-" + path.option, resources.appendingPathComponent(path.folder).path]
        }
        for path in dataFolders {
            result += ["-" + path.option, settings.dataFolder.appendingPathComponent(path.folder).path]
        }
        return result
    }

    /// Creates the user ROM folder and the folders MAME writes.
    static func createFolders(_ settings: AppSettings = .current) {
        let folders = [userROMs(settings.dataFolder)] + dataFolders.map { settings.dataFolder.appendingPathComponent($0.folder) }
        for folder in folders {
            try? FileManager.default.createDirectory(at: folder, withIntermediateDirectories: true)
        }
    }
}
