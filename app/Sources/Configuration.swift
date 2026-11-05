// A configuration is a MAME command line: a driver, then options. This
// file edits that command line and turns MAME's description of it into
// the rows the configuration window shows.

import Foundation

/// A MAME command line as a driver and its options, in order: "-name
/// value" pairs, "-name" flags, and other tokens kept as they are.
struct Arguments: Equatable {
    struct Option: Equatable {
        /// Empty for a token that is not an option.
        var name: String
        var value: String
        /// False for a flag, which has no value.
        var hasValue = true
    }

    var driver: String
    var options: [Option]

    init(driver: String, options: [Option] = []) {
        self.driver = driver
        self.options = options
    }

    init(_ array: [String]) {
        var tokens = array[...]
        driver = tokens.first.map { $0.hasPrefix("-") ? "" : $0 } ?? ""
        if !driver.isEmpty { tokens = tokens.dropFirst() }
        options = []
        while let token = tokens.first {
            tokens = tokens.dropFirst()
            guard token.hasPrefix("-") else {
                options.append(Option(name: "", value: token))
                continue
            }
            // an option's value never starts with "-": that is the next option
            if let value = tokens.first, !value.hasPrefix("-") {
                tokens = tokens.dropFirst()
                options.append(Option(name: String(token.dropFirst()), value: value))
            } else {
                options.append(Option(name: String(token.dropFirst()), value: "", hasValue: false))
            }
        }
    }

    var array: [String] {
        [driver] + options.flatMap { option -> [String] in
            if option.name.isEmpty { return [option.value] }
            return option.hasValue ? ["-" + option.name, option.value] : ["-" + option.name]
        }
    }

    subscript(name: String) -> String? {
        options.first { !name.isEmpty && $0.name == name && $0.hasValue }?.value
    }

    /// Adds an option, or replaces the one with its name in its place.
    mutating func set(_ option: Option) {
        if !option.name.isEmpty, let index = options.firstIndex(where: { $0.name == option.name }) {
            options[index] = option
        } else {
            options.append(option)
        }
    }

    /// Sets an option, keeping its place; nil removes it.
    mutating func set(_ name: String, _ value: String?) {
        if let value {
            if let index = options.firstIndex(where: { $0.name == name }) {
                options[index].value = value
                options[index].hasValue = true
            } else {
                options.append(Option(name: name, value: value))
            }
        } else {
            options.removeAll { $0.name == name }
        }
    }
}

/// A slot option value: a card and, optionally, its ROM ("intf1,bios=v2").
struct CardChoice: Equatable {
    var card: String
    var bios: String

    init(card: String, bios: String = "") {
        self.card = card
        self.bios = bios
    }

    init(_ value: String) {
        let parts = value.components(separatedBy: ",bios=")
        card = parts[0]
        bios = parts.count > 1 ? parts[1] : ""
    }

    var value: String { bios.isEmpty ? card : card + ",bios=" + bios }
}

extension Array where Element == BiosSet {
    /// MAME's default set: the one marked default, else the first.
    var defaultSet: BiosSet? { first { $0.default } ?? first }
}

/// Edits a configuration's command line. Every edit asks MAME whether
/// the result is valid and drops options the machine no longer has.
@MainActor
enum ConfigurationEditor {
    /// A new configuration for a machine: its newest model, as it shipped.
    static func newConfiguration(_ machine: Catalogue.Machine, catalogue: Catalogue) -> [String] {
        catalogue.startingArguments(machine.defaultDriver)
    }

    /// Runs another driver of the same machine (a model or a ROM variant),
    /// keeping every choice the new driver also has.
    static func setDriver(_ arguments: [String], to driver: String, catalogue: Catalogue) -> [String] {
        let old = Arguments(arguments)
        guard old.driver != driver else { return arguments }
        let oldStart = Arguments(catalogue.startingArguments(old.driver))
        var new = Arguments(catalogue.startingArguments(driver))
        for option in old.options where option.name != "bios" {
            // a port the old model left empty only because it starts empty
            // follows the new model's start instead
            if option.hasValue, option.value.isEmpty, oldStart[option.name] == "", new[option.name] == nil { continue }
            new.set(option)
        }
        if let bios = old["bios"], catalogue.driver(driver)?.biossets.contains(where: { $0.name == bios }) == true {
            new.set("bios", bios)
        }
        return normalise(new.array)
    }

    /// Chooses a ROM: a BIOS set of the machine's driver, or of a variant driver.
    static func setROM(_ arguments: [String], driver: String, bios: String?, catalogue: Catalogue) -> [String] {
        var args = Arguments(setDriver(arguments, to: driver, catalogue: catalogue))
        let isDefault = bios == nil || catalogue.driver(driver)?.biossets.defaultSet?.name == bios
        args.set("bios", isDefault ? nil : bios)
        return normalise(args.array)
    }

    /// Fits a card in a slot; an empty card leaves the slot empty.
    static func setSlot(_ arguments: [String], slot: Slot, card: String, catalogue: Catalogue) -> [String] {
        var args = Arguments(arguments)
        let startsEmpty = Arguments(catalogue.startingArguments(args.driver))[slot.tag] == ""
        if card == slot.default && !startsEmpty {
            args.set(slot.tag, nil)
        } else {
            args.set(slot.tag, CardChoice(card: card).value)
        }
        return normalise(args.array)
    }

    /// Chooses the ROM of the card fitted in a slot.
    static func setCardROM(_ arguments: [String], slot: Slot, bios: String, catalogue: Catalogue) -> [String] {
        var args = Arguments(arguments)
        let isDefault = (slot.biossets ?? []).defaultSet?.name == bios
        let choice = CardChoice(card: slot.current, bios: isDefault ? "" : bios)
        let startsEmpty = Arguments(catalogue.startingArguments(args.driver))[slot.tag] == ""
        if choice.card == slot.default && choice.bios.isEmpty && !startsEmpty {
            args.set(slot.tag, nil)
        } else {
            args.set(slot.tag, choice.value)
        }
        return normalise(args.array)
    }

    /// Puts a file in a media device; nil empties it.
    static func setMedia(_ arguments: [String], media: Media, path: String?) -> [String] {
        var args = Arguments(arguments)
        args.set(media.name, path)
        return args.array
    }

    /// The command line with every option MAME rejects removed. Options
    /// are added back in their order, so a card's own slots and media
    /// are kept once the card is fitted. When MAME rejects the driver
    /// itself, no option can be checked, so the command line stays as it
    /// is and the window shows MAME's error.
    static func normalise(_ arguments: [String]) -> [String] {
        let original = Arguments(arguments)
        guard !MAME.describe(original.array).isValid else { return arguments }

        var result = Arguments(driver: original.driver)
        guard MAME.describe(result.array).isValid else { return arguments }
        var pending = original.options
        var progress = true
        while progress && !pending.isEmpty {
            progress = false
            for option in pending {
                var trial = result
                trial.set(option)
                if MAME.describe(trial.array).isValid {
                    result = trial
                    pending.removeAll { $0 == option }
                    progress = true
                }
            }
        }
        return result.array
    }
}

// MARK: - What the configuration window shows

/// The rows of the configuration window, in words a Spectrum owner uses.
struct ConfigurationSummary {
    struct Choice: Hashable {
        let value: String
        let label: String
    }

    struct HardwareRow: Identifiable {
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

    struct MediaRow: Identifiable {
        let media: Media
        let label: String
        var id: String { media.name }
    }

    struct ROMChoice: Hashable {
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
    let errors: String
    let warnings: String
    let missingMedia: [String]
    /// The media names of files named on the command line that are not there.
    let missingFiles: Set<String>
    /// For SD card media with an image MAME's SD card cannot use: the
    /// reason, by media name.
    let sdCardProblems: [String: String]

    var canRun: Bool { errors.isEmpty && missingMedia.isEmpty }

    /// `machineHint` is the document's machine, used when the catalogue
    /// does not have the driver, so the user can choose another model.
    init(arguments: [String], description: MachineDescription, catalogue: Catalogue, machineHint: String = "") {
        let args = Arguments(arguments)
        driver = args.driver
        machine = catalogue.machine(forDriver: args.driver) ?? catalogue.machine(forDriver: machineHint)
        let running = catalogue.driver(args.driver)
        year = running?.year ?? machine?.year ?? ""
        preliminary = running.map { $0.status == "preliminary" } ?? machine?.preliminary ?? false
        errors = description.errors
        warnings = description.warnings ?? ""

        // the models offered, plus the reference model when a document names it
        if let machine {
            var list = machine.models
            if let other = machine.otherModels.first(where: { $0.driver == args.driver }) {
                list.append(other)
            }
            models = list
        } else {
            models = []
        }

        // ROMs: the running model's sets, then the variants'
        let modelDriver = machine?.variants.contains { $0.driver == args.driver } == true
            ? (machine?.defaultDriver ?? args.driver) : args.driver
        let modelSets = catalogue.driver(modelDriver)?.biossets ?? []
        roms = modelSets.map { ROMChoice(driver: modelDriver, bios: $0.name, label: $0.description) }
        variantROMs = (machine?.variants ?? []).flatMap { variant -> [ROMChoice] in
            if variant.biossets.isEmpty {
                return [ROMChoice(driver: variant.driver, bios: nil, label: variant.name)]
            }
            return variant.biossets.map {
                ROMChoice(driver: variant.driver, bios: $0.name, label: variant.name + ", " + $0.description)
            }
        }
        let currentSets = catalogue.driver(args.driver)?.biossets ?? []
        let currentBios = args["bios"] ?? currentSets.defaultSet?.name
        currentROM = (roms + variantROMs).first {
            $0.driver == args.driver && ($0.bios == currentBios || ($0.bios == nil && currentSets.isEmpty))
        }

        let slots = description.slots ?? []
        hardware = Self.hardwareRows(slots)
        let labelBySlot = Dictionary(hardware.map { ($0.slot.tag, $0.label) }, uniquingKeysWith: { a, _ in a })
        media = Self.mediaRows(description.media ?? [], slots: slots, labelBySlot: labelBySlot)
        missingMedia = media.filter { $0.media.mandatory && $0.media.current.isEmpty }.map(\.label)
        missingFiles = Set(media.filter {
            !$0.media.current.isEmpty && !FileManager.default.fileExists(atPath: $0.media.current)
        }.map(\.media.name))
        var problems: [String: String] = [:]
        for row in media where row.media.interface == "sdcard" && !row.media.current.isEmpty {
            if let problem = SDCardImage.problem(path: row.media.current) {
                problems[row.media.name] = problem
            }
        }
        sdCardProblems = problems
    }

    private static func hardwareRows(_ slots: [Slot]) -> [HardwareRow] {
        let byTag = Dictionary(slots.map { ($0.tag, $0) }, uniquingKeysWith: { a, _ in a })
        func depth(_ slot: Slot) -> Int {
            var count = 0
            var parent = slot.parent
            while !parent.isEmpty, let next = byTag[parent], count < 16 {
                count += 1
                parent = next.parent
            }
            return count
        }

        let shown = slots.filter { slot in
            if slot.fixed || slot.options.isEmpty { return !slot.current.isEmpty }
            return true
        }

        var rows: [HardwareRow] = []
        for slot in shown {
            let siblings = shown.filter { $0.parent == slot.parent && baseLabel($0) == baseLabel(slot) }
            var label = baseLabel(slot)
            if siblings.count > 1 || slot.kind == "drive", let index = siblings.firstIndex(of: slot) {
                label += " \(index + 1)"
            }

            var choices = slot.options.map { Choice(value: $0.name, label: optionLabel(slot, $0)) }
            if slot.kind != "memory" {
                choices.insert(Choice(value: "", label: "None"), at: 0)
            }
            let fittedName = slot.options.first { $0.name == slot.current }?.description
                ?? slot.currentdescription ?? slot.current
            let fixedText = (slot.fixed || slot.options.isEmpty) ? fittedName : nil

            let sets = slot.biossets ?? []
            let roms = sets.count > 1 ? sets.map { Choice(value: $0.name, label: $0.description) } : []
            let currentROM = (slot.bios?.isEmpty == false ? slot.bios : nil) ?? sets.defaultSet?.name ?? ""

            // Ports of one kind are told apart by the card each was made for,
            // whatever is fitted now.
            var meantFor: String?
            if slot.isPort, siblings.count > 1,
               let made = slot.options.first(where: { $0.name == slot.default }) {
                meantFor = made.description
            }

            rows.append(HardwareRow(slot: slot, label: label, depth: depth(slot), choices: choices,
                                    fixedText: fixedText, meantFor: meantFor, roms: roms, currentROM: currentROM))
        }
        return rows
    }

    private static func baseLabel(_ slot: Slot) -> String {
        switch slot.kind {
        case "memory": return "Memory"
        case "sound": return "Sound chip"
        case "drive": return "Disk drive"
        default: return slot.typedescription
        }
    }

    private static func optionLabel(_ slot: Slot, _ option: SlotOption) -> String {
        slot.kind == "memory" ? memorySize(option.name) : option.description
    }

    /// "48K" for MAME's "48k", "2 MB" for "2m".
    static func memorySize(_ name: String) -> String {
        let lower = name.lowercased()
        if lower.hasSuffix("k"), let number = Int(lower.dropLast()) { return "\(number)K" }
        if lower.hasSuffix("m"), let number = Int(lower.dropLast()) { return "\(number) MB" }
        if lower.hasSuffix("g"), let number = Int(lower.dropLast()) { return "\(number) GB" }
        return name
    }

    private static func mediaRows(_ media: [Media], slots: [Slot], labelBySlot: [String: String]) -> [MediaRow] {
        let drives = Set(slots.filter { $0.kind == "drive" }.map(\.tag))
        func baseLabel(_ item: Media) -> String {
            if !item.owner.partof.isEmpty { return item.owner.partof }
            if drives.contains(item.owner.slot), let label = labelBySlot[item.owner.slot] { return label }
            return item.owner.description
        }
        return media.map { item in
            var label = baseLabel(item)
            let same = media.filter { baseLabel($0) == label }
            if same.count > 1, !drives.contains(item.owner.slot), let index = same.firstIndex(of: item) {
                label += " \(index + 1)"
            }
            return MediaRow(media: item, label: label)
        }
    }
}

// MARK: - Running

/// The options the app adds to a run with the debugger, from Settings.
enum RunOptions {
    @MainActor
    static func debugArguments(_ debug: Bool) -> [String] {
        guard debug else { return [] }
        let settings = AppSettings.current
        var result = ["-debug"]
        if settings.debugger == .gdbStub {
            result += ["-debugger", "gdbstub", "-debugger_port", String(settings.gdbPort)]
        }
        return result
    }

    /// The headless command for a configuration, quoted for zsh. Headless
    /// mode adds the app's paths itself.
    @MainActor
    static func commandLine(_ arguments: [String], debug: Bool = false) -> String {
        let executable = Bundle.main.executablePath ?? "Pinesy"
        return ([executable, "--"] + arguments + debugArguments(debug)).map(shellQuoted).joined(separator: " ")
    }

    static func shellQuoted(_ text: String) -> String {
        let plain = CharacterSet.alphanumerics.union(CharacterSet(charactersIn: "-_./:=,+@%"))
        if !text.isEmpty, text.unicodeScalars.allSatisfy({ plain.contains($0) }) {
            return text
        }
        return "'" + text.replacingOccurrences(of: "'", with: "'\\''") + "'"
    }
}

// MARK: - SD card images

/// The sizes MAME's SD card takes (spi_sdcard.cpp, device_start): an SD
/// card of up to 1 GiB, or an SDHC card of up to 32 GiB that is a
/// multiple of 512 KiB. A raw image has 512-byte blocks. A CHD is not
/// checked, because its file size is not its disk size.
enum SDCardImage {
    static let largestSDHC: UInt64 = 1 << 26   // blocks: 32 GiB

    /// Whether MAME's SD card takes an image of this many 512-byte blocks.
    static func usable(blocks: UInt64) -> Bool {
        guard blocks > 0 else { return false }
        var mantissa = blocks
        var exponent: UInt64 = 0
        while mantissa & 1 == 0 {
            mantissa >>= 1
            exponent += 1
        }
        let sd = exponent >= 2 && mantissa << (exponent > 9 ? exponent - 9 : 0) <= 1 << 12
        let sdhc = exponent >= 10 && mantissa << (exponent - 10) <= 1 << 16
        return sd || sdhc
    }

    /// Why MAME's SD card cannot use an image of this many bytes, or nil.
    static func problem(bytes: UInt64) -> String? {
        let blocks = bytes / 512
        if usable(blocks: blocks) { return nil }
        if blocks > largestSDHC {
            return "Too big: SD cards hold up to 32 GiB."
        }
        return "Unusable size: use a multiple of 512 KiB."
    }

    /// The problem with the image at a path, or nil for an image that
    /// works, a CHD, or a file that is not there.
    static func problem(path: String) -> String? {
        guard (path as NSString).pathExtension.lowercased() != "chd",
              let size = (try? FileManager.default.attributesOfItem(atPath: path))?[.size] as? UInt64
        else { return nil }
        return problem(bytes: size)
    }
}
