// A configuration is a MAME command line: a driver, then options. This
// file edits that command line and turns MAME's description of it into
// the rows the configuration window shows.

import Foundation

/// A MAME command line as a driver and "-name value" pairs.
struct Arguments: Equatable {
    struct Option: Equatable {
        var name: String
        var value: String
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
            guard token.hasPrefix("-") else { continue }
            let value = tokens.first ?? ""
            if !tokens.isEmpty { tokens = tokens.dropFirst() }
            options.append(Option(name: String(token.dropFirst()), value: value))
        }
    }

    var array: [String] {
        [driver] + options.flatMap { ["-" + $0.name, $0.value] }
    }

    subscript(name: String) -> String? {
        options.first { $0.name == name }?.value
    }

    /// Sets an option, keeping its place; nil removes it.
    mutating func set(_ name: String, _ value: String?) {
        if let value {
            if let index = options.firstIndex(where: { $0.name == name }) {
                options[index].value = value
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
            if option.value.isEmpty, oldStart[option.name] == "", new[option.name] == nil { continue }
            new.set(option.name, option.value)
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
    /// are kept once the card is fitted.
    static func normalise(_ arguments: [String]) -> [String] {
        let original = Arguments(arguments)
        guard !MAME.describe(original.array).isValid else { return arguments }

        var result = Arguments(driver: original.driver)
        var pending = original.options
        var progress = true
        while progress && !pending.isEmpty {
            progress = false
            for option in pending {
                var trial = result
                trial.set(option.name, option.value)
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
    let models: [Catalogue.Model]
    let roms: [ROMChoice]
    let variantROMs: [ROMChoice]
    let currentROM: ROMChoice?
    let hardware: [HardwareRow]
    let media: [MediaRow]
    let errors: String
    let missingMedia: [String]

    var canRun: Bool { errors.isEmpty && missingMedia.isEmpty }

    init(arguments: [String], description: MachineDescription, catalogue: Catalogue) {
        let args = Arguments(arguments)
        driver = args.driver
        machine = catalogue.machine(forDriver: args.driver)
        errors = description.errors

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
            let fittedName = slot.options.first { $0.name == slot.current }?.description ?? slot.current
            let fixedText = (slot.fixed || slot.options.isEmpty) ? fittedName : nil

            let sets = slot.biossets ?? []
            let roms = sets.count > 1 ? sets.map { Choice(value: $0.name, label: $0.description) } : []
            let currentROM = (slot.bios?.isEmpty == false ? slot.bios : nil) ?? sets.defaultSet?.name ?? ""

            // Ports of one kind are told apart by the card each was made for.
            var meantFor: String?
            if slot.isPort, siblings.count > 1, slot.current != slot.default,
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

/// The options the app adds to every run from its settings.
enum RunOptions {
    @MainActor
    static func arguments(debug: Bool) -> [String] {
        let settings = AppSettings.current
        var result: [String] = []
        if !settings.romFolders.isEmpty {
            result += ["-rompath", settings.romFolders.joined(separator: ";")]
        }
        let base = settings.dataFolder
        result += ["-cfg_directory", base.appendingPathComponent("cfg").path,
                   "-nvram_directory", base.appendingPathComponent("nvram").path,
                   "-snapshot_directory", base.appendingPathComponent("snap").path]
        if debug {
            result.append("-debug")
            if settings.debugger == .gdbStub {
                result += ["-debugger", "gdbstub", "-debugger_port", String(settings.gdbPort)]
            }
        }
        return result
    }

    /// The headless command for a configuration, quoted for zsh.
    @MainActor
    static func commandLine(_ arguments: [String], debug: Bool = false) -> String {
        let executable = Bundle.main.executablePath ?? "Pinesy"
        return ([executable, "--"] + arguments + Self.arguments(debug: debug)).map(shellQuoted).joined(separator: " ")
    }

    static func shellQuoted(_ text: String) -> String {
        let plain = CharacterSet.alphanumerics.union(CharacterSet(charactersIn: "-_./:=,+@%"))
        if !text.isEmpty, text.unicodeScalars.allSatisfy({ plain.contains($0) }) {
            return text
        }
        return "'" + text.replacingOccurrences(of: "'", with: "'\\''") + "'"
    }
}
