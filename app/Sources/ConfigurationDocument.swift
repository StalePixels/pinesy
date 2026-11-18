// A configuration document: a .pinesy file. The core reads and writes
// its text (pinesy_core_read_document(), pinesy_core_write_document()).

import SwiftUI
import UniformTypeIdentifiers
import CPinesy

extension UTType {
    static let pinesyConfiguration = UTType(exportedAs: "com.stalepixels.pinesy.configuration", conformingTo: .data)
}

struct ConfigurationDocument: FileDocument {
    static let readableContentTypes: [UTType] = [.pinesyConfiguration]

    /// The machine's driver, as the catalogue names the machine; empty
    /// until the user has chosen a machine.
    var machine: String
    /// The MAME command line: the driver to run, then options. Only
    /// options that differ from the starting configuration are written.
    var arguments: [String]
    /// The machine window's size: Core.windowFullScreen, Core.windowFit,
    /// or a magnification of the frame (pinesy_core_window_size()).
    var windowSize: Int

    init() {
        machine = ""
        arguments = []
        windowSize = Core.windowFit
    }

    private struct Contents: Decodable {
        var machine: String
        var arguments: [String]
        var windowSize: Int
    }

    init(configuration: ReadConfiguration) throws {
        guard let data = configuration.file.regularFileContents,
              let text = String(data: data, encoding: .utf8),
              let contents: Contents = Core.decode(pinesy_core_read_document(text))
        else {
            throw CocoaError(.fileReadCorruptFile)
        }
        machine = contents.machine
        arguments = contents.arguments
        windowSize = contents.windowSize
    }

    func fileWrapper(configuration: WriteConfiguration) throws -> FileWrapper {
        guard let text = Core.take(pinesy_core_write_document(machine, Core.encode(arguments), Int32(clamping: windowSize))) else {
            throw CocoaError(.fileWriteUnknown)
        }
        return FileWrapper(regularFileWithContents: Data(text.utf8))
    }
}
