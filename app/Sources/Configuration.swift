// What the configuration window shows, as the core's summary gives it
// (pinesy_core_summary()). The core checks the command line with MAME
// and turns MAME's description of it into these rows.

import Foundation

struct ConfigurationSummary: Decodable {
    struct Choice: Decodable, Hashable {
        let value: String
        let label: String
    }

    struct HardwareRow: Decodable, Identifiable {
        let slot: Slot
        let label: String
        let depth: Int
        let choices: [Choice]
        /// The fitted card's name, for a slot that cannot be changed.
        let fixedText: String?
        /// For one of several ports of a kind: the card MAME made the port for.
        let meantFor: String?
        /// The fitted card's ROMs, when there is more than one.
        let roms: [Choice]
        let currentROM: String
        var id: String { slot.tag }
    }

    struct MediaRow: Decodable, Identifiable {
        let media: Media
        let label: String
        var id: String { media.name }
    }

    struct ROMChoice: Decodable, Hashable {
        let driver: String
        let bios: String?
        let label: String
    }

    let machine: Catalogue.Machine?
    let driver: String
    /// The year and status of the driver that runs: the chosen model or ROM.
    let year: String
    let preliminary: Bool
    let models: [Catalogue.Model]
    let roms: [ROMChoice]
    let variantROMs: [ROMChoice]
    let currentROM: ROMChoice?
    let hardware: [HardwareRow]
    let media: [MediaRow]
    /// The machine, with this hardware, has mouse inputs, so the mouse
    /// setting is shown and reaches a run.
    let mouse: Bool
    let errors: String
    let warnings: String
    let missingMedia: [String]
    /// The media names of files named on the command line that are not there.
    let missingFiles: Set<String>
    /// For SD card media with an image MAME's SD card cannot use: the
    /// reason, by media name.
    let sdCardProblems: [String: String]
    let canRun: Bool
}
