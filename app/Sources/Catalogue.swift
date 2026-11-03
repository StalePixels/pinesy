// The machines the user can choose, built from MAME's driver list by
// rules that use only MAME's data. No driver is named here.
//
// - Variants. An unofficial driver (MAME: "an after-market or end-user
//   modification") is a ROM choice of its machine, not a machine. Its
//   machine is its parent; without a parent, the official driver in the
//   same source file with the same devices.
// - Models. A driver and its clones that share one name before a colon
//   ("ZX Spectrum Next: KS1") are one machine with a choice of model.
//   The parent is MAME's reference for the clones, not a product, so it
//   is not offered as a model; a configuration that names it still opens.
// - Sections. Machines are grouped by their first maker, and a clone
//   joins its parent's section when they share a maker. Makers with one
//   product (a machine with models counts each model) go into "Other
//   makers". Sections and machines are in year
//   order.
// - Starting configuration. A port where MAME fits the same card on
//   every model of a machine family is MAME's convenience, not part of
//   any product, so a new configuration leaves that port empty. Where
//   MAME fits different cards on different models, the card records
//   what each model has, and it stays. Memory, sound chips and drives
//   are parts of the machine and keep MAME's setting.

import Foundation

struct Catalogue: Sendable {
    struct Model: Hashable, Sendable {
        let driver: String
        let label: String
        let year: String
    }

    struct Variant: Hashable, Sendable {
        let driver: String
        let name: String
        let biossets: [BiosSet]
    }

    struct Machine: Identifiable, Hashable, Sendable {
        /// The driver that stands for the machine: the family parent
        /// when the machine has models, else its own driver.
        let id: String
        let name: String
        let manufacturer: String
        let year: String
        let preliminary: Bool
        /// Released models, in year order. Empty for a machine without models.
        let models: [Model]
        /// Models MAME has that are not offered (the family parent).
        let otherModels: [Model]
        /// ROM choices that run another driver.
        let variants: [Variant]
        let searchText: String

        /// The driver a new configuration runs: the newest model.
        var defaultDriver: String { models.last?.driver ?? id }

        var drivers: [String] {
            [id] + models.map(\.driver) + otherModels.map(\.driver) + variants.map(\.driver)
        }

        func model(for driver: String) -> Model? {
            (models + otherModels).first { $0.driver == driver }
        }
    }

    struct Section: Identifiable, Hashable, Sendable {
        let name: String
        let machines: [Machine]
        /// True for "Other makers", where each row also shows the maker.
        let showsMaker: Bool
        var id: String { name }
    }

    static let otherMakers = "Other makers"

    let sections: [Section]
    private let drivers: [String: DriverInfo]
    private let machineByDriver: [String: Machine]

    @MainActor static let shared = Catalogue(MAME.listMachines())

    var machines: [Machine] { sections.flatMap(\.machines) }

    func machine(forDriver driver: String) -> Machine? {
        machineByDriver[driver]
    }

    func driver(_ name: String) -> DriverInfo? {
        drivers[name]
    }

    /// Machines whose name, maker, model or ROM name contains the text.
    func search(_ text: String) -> [Machine] {
        let needle = text.trimmingCharacters(in: .whitespaces)
        guard !needle.isEmpty else { return [] }
        return machines.filter { $0.searchText.localizedCaseInsensitiveContains(needle) }
    }

    // MARK: - Building the catalogue

    init(_ list: [DriverInfo]) {
        let byName = Dictionary(list.map { ($0.name, $0) }, uniquingKeysWith: { first, _ in first })
        drivers = byName

        // variants: unofficial drivers become ROM choices of their machine
        var variantsOf: [String: [DriverInfo]] = [:]
        var variantDrivers = Set<String>()
        for driver in list where driver.unofficial {
            if let parent = Self.variantParent(of: driver, in: list, byName: byName) {
                variantsOf[parent, default: []].append(driver)
                variantDrivers.insert(driver.name)
            }
        }

        let candidates = list.filter { !variantDrivers.contains($0.name) }

        // models: a parent and the clones that share its name before a colon
        var modelsOf: [String: [DriverInfo]] = [:]
        var modelDrivers = Set<String>()
        for parent in candidates {
            guard let family = Self.familyName(parent.description) else { continue }
            let clones = candidates.filter {
                $0.cloneof == parent.name && Self.familyName($0.description) == family
            }
            if !clones.isEmpty {
                modelsOf[parent.name] = clones.sorted { Self.yearValue($0.year) < Self.yearValue($1.year) }
                clones.forEach { modelDrivers.insert($0.name) }
            }
        }

        var machines: [Machine] = []
        for driver in candidates where !modelDrivers.contains(driver.name) {
            let clones = modelsOf[driver.name] ?? []
            let name = clones.isEmpty ? driver.description : (Self.familyName(driver.description) ?? driver.description)
            let models = clones.map { Model(driver: $0.name, label: Self.modelName($0.description), year: $0.year) }
            let other = clones.isEmpty ? [] : [Model(driver: driver.name, label: Self.modelName(driver.description), year: driver.year)]
            let shown = clones.last ?? driver
            let year = clones.first?.year ?? driver.year
            let variants = (variantsOf[driver.name] ?? [])
                .sorted { Self.yearValue($0.year) < Self.yearValue($1.year) }
                .map { Variant(driver: $0.name, name: $0.description, biossets: $0.biossets) }
            // a maker MAME does not know ("<unknown>") is not shown, so it is not searched
            let maker = driver.manufacturer.hasPrefix("<") ? [] : [driver.manufacturer]
            let search = ([name] + maker + models.map(\.label) + variants.map(\.name))
                .joined(separator: " ")
            machines.append(Machine(id: driver.name, name: name, manufacturer: driver.manufacturer, year: year,
                                    preliminary: shown.status == "preliminary",
                                    models: models, otherModels: other, variants: variants, searchText: search))
        }

        sections = Self.sections(machines, byName: byName)

        var lookup: [String: Machine] = [:]
        for machine in machines {
            for name in machine.drivers {
                lookup[name] = machine
            }
        }
        machineByDriver = lookup
    }

    /// The machine an unofficial driver is a ROM choice of.
    private static func variantParent(of driver: DriverInfo, in list: [DriverInfo], byName: [String: DriverInfo]) -> String? {
        if let parent = driver.cloneof, byName[parent] != nil {
            return parent
        }
        let matches = list.filter {
            !$0.unofficial && $0.sourcefile == driver.sourcefile && $0.devices == driver.devices
        }
        return matches.count == 1 ? matches[0].name : nil
    }

    /// "ZX Spectrum Next" for "ZX Spectrum Next: KS1".
    private static func familyName(_ description: String) -> String? {
        guard let range = description.range(of: ": ") else { return nil }
        return String(description[..<range.lowerBound])
    }

    /// "KS1" for "ZX Spectrum Next: KS1".
    private static func modelName(_ description: String) -> String {
        guard let range = description.range(of: ": ") else { return description }
        return String(description[range.upperBound...])
    }

    /// A sortable year; unknown digits ("198?") sort after the known ones.
    static func yearValue(_ year: String) -> Int {
        Int(year.replacingOccurrences(of: "?", with: "9")) ?? 9999
    }

    /// The makers named in a manufacturer field, keeping company suffixes
    /// with their names: "Scorpion, Ltd." is one maker.
    static func makers(_ manufacturer: String) -> [String] {
        let suffixes: Set<String> = ["ltd", "ltd.", "inc", "inc.", "co", "co.", "plc", "gmbh", "s.a.", "corp", "corp."]
        var result: [String] = []
        for part in manufacturer.components(separatedBy: ", ") {
            let trimmed = part.trimmingCharacters(in: .whitespaces)
            if let last = result.last, suffixes.contains(trimmed.lowercased()) {
                result[result.count - 1] = last + ", " + trimmed
            } else if !trimmed.isEmpty {
                result.append(trimmed)
            }
        }
        return result
    }

    private static func sections(_ machines: [Machine], byName: [String: DriverInfo]) -> [Section] {
        let firstMaker: [String: String] = Dictionary(uniqueKeysWithValues: machines.map {
            ($0.id, makers($0.manufacturer).first ?? $0.manufacturer)
        })

        // a clone joins its parent's section when the two share a maker
        var sectionOf: [String: String] = [:]
        for machine in machines {
            var section = firstMaker[machine.id] ?? machine.manufacturer
            if let parent = byName[machine.id]?.cloneof, let parentMachine = machines.first(where: { $0.id == parent }),
               let parentSection = firstMaker[parentMachine.id],
               makers(machine.manufacturer).contains(where: { makers(parentMachine.manufacturer).contains($0) }) {
                section = parentSection
            }
            sectionOf[machine.id] = section
        }

        // a machine with models counts once for each model
        let grouped = Dictionary(grouping: machines) { machine -> String in
            let section = sectionOf[machine.id] ?? ""
            let count = machines.filter { sectionOf[$0.id] == section }.map { max(1, $0.models.count) }.reduce(0, +)
            return (count < 2 || section.hasPrefix("<")) ? otherMakers : section
        }

        func earliest(_ list: [Machine]) -> Int {
            list.map { yearValue($0.year) }.min() ?? 9999
        }

        return grouped
            .map { name, list in
                Section(name: name,
                        machines: list.sorted {
                            (yearValue($0.year), $0.name) < (yearValue($1.year), $1.name)
                        },
                        showsMaker: name == otherMakers)
            }
            .sorted { a, b in
                if a.showsMaker != b.showsMaker { return !a.showsMaker }
                return (earliest(a.machines), a.name) < (earliest(b.machines), b.name)
            }
    }

    // MARK: - Starting configuration

    /// The command line of a new configuration for a driver: MAME's
    /// defaults, with the ports MAME fills only for convenience left empty.
    func startingArguments(_ driver: String) -> [String] {
        var arguments = [driver]
        for tag in conveniencePorts(driver) {
            arguments += ["-" + tag, ""]
        }
        return arguments
    }

    /// The ports of a driver's default configuration that MAME fills with
    /// the same card on every model of its family.
    func conveniencePorts(_ driver: String) -> [String] {
        guard let info = drivers[driver] else { return [] }
        let family = familyMembers(of: driver)
        return info.slots.compactMap { slot in
            guard slot.isPort, !slot.fixed, !slot.default.isEmpty else { return nil }
            let defaults = family.compactMap { member in
                member.slots.first { $0.tag == slot.tag }?.default
            }
            guard defaults.count >= 2, Set(defaults).count == 1 else { return nil }
            return slot.tag
        }
    }

    /// Every driver in MAME's clone family of a driver, the driver included.
    private func familyMembers(of driver: String) -> [DriverInfo] {
        let root = familyRoot(driver)
        return drivers.values.filter { familyRoot($0.name) == root }
    }

    private func familyRoot(_ driver: String) -> String {
        var name = driver
        var seen = Set<String>()
        while let parent = drivers[name]?.cloneof, drivers[parent] != nil, !seen.contains(parent) {
            seen.insert(name)
            name = parent
        }
        return name
    }
}
