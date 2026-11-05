// A configuration document: a .pinesy file, a small JSON document.

import SwiftUI
import UniformTypeIdentifiers

extension UTType {
    static let pinesyConfiguration = UTType(exportedAs: "com.stalepixels.pinesy.configuration", conformingTo: .data)
}

struct ConfigurationDocument: FileDocument {
    static let readableContentTypes: [UTType] = [.pinesyConfiguration]
    static let formatVersion = 1

    /// The machine's driver, as the catalogue names the machine; empty
    /// until the user has chosen a machine.
    var machine: String
    /// The MAME command line: the driver to run, then options. Only
    /// options that differ from the starting configuration are written.
    var arguments: [String]

    init() {
        machine = ""
        arguments = []
    }

    private struct Contents: Codable {
        var format: Int
        var machine: String
        var arguments: [String]
    }

    init(configuration: ReadConfiguration) throws {
        guard let data = configuration.file.regularFileContents else {
            throw CocoaError(.fileReadCorruptFile)
        }
        let contents = try JSONDecoder().decode(Contents.self, from: data)
        guard contents.format <= Self.formatVersion, !contents.arguments.isEmpty else {
            throw CocoaError(.fileReadCorruptFile)
        }
        machine = contents.machine
        arguments = contents.arguments
    }

    func fileWrapper(configuration: WriteConfiguration) throws -> FileWrapper {
        let encoder = JSONEncoder()
        encoder.outputFormatting = [.prettyPrinted, .sortedKeys, .withoutEscapingSlashes]
        let data = try encoder.encode(Contents(format: Self.formatVersion, machine: machine, arguments: arguments))
        return FileWrapper(regularFileWithContents: data)
    }
}
