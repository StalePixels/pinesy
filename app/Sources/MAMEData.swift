// The JSON documents pinesy_list_machines() and pinesy_describe()
// return. Field names are MAME's, as pinesy.h documents them.

struct BiosSet: Decodable, Hashable, Sendable {
    let name: String
    let description: String
    let `default`: Bool
}

struct SlotOption: Decodable, Hashable, Sendable {
    let name: String
    let description: String
}

struct Slot: Decodable, Hashable, Sendable {
    /// The option name on the command line, such as "exp" or "exp:intf1:exp".
    let tag: String
    /// The slot holding the card this slot belongs to; empty for the machine.
    let parent: String
    /// "memory", "drive", "sound" or "port", from the slot itself, not
    /// from the card fitted now.
    let kind: String
    let type: String
    let typedescription: String
    let fixed: Bool
    let `default`: String
    /// The fitted card; empty for none.
    let current: String
    /// The fitted card's description.
    let currentdescription: String?
    let options: [SlotOption]
    /// The fitted card's ROMs, when a card is fitted.
    let biossets: [BiosSet]?
    let bios: String?

    var isPort: Bool { kind == "port" }
}

struct MediaOwner: Decodable, Hashable, Sendable {
    let tag: String
    let description: String
    /// The slot of the card that holds the media device; empty for the machine.
    let slot: String
    /// The device the media device is a part of, such as an SD card slot;
    /// empty when the media device stands alone.
    let partof: String
}

struct Media: Decodable, Hashable, Sendable {
    /// The option name on the command line, such as "cass" or "pi".
    let name: String
    let instance: String
    let brief: String
    let typename: String
    let owner: MediaOwner
    /// Comma-separated file extensions, without dots.
    let extensions: String
    let interface: String
    let mandatory: Bool
    /// The file named on the command line; empty for none.
    let current: String

    var extensionList: [String] {
        extensions.split(separator: ",").map(String.init)
    }
}

struct MachineDescription: Decodable, Sendable {
    var driver: String?
    var bios: String?
    var biossets: [BiosSet]?
    var slots: [Slot]?
    var media: [Media]?
    var errors: String
    /// MAME's warnings; they do not stop a run.
    var warnings: String?

    init(errors: String) {
        self.errors = errors
    }

    var isValid: Bool { errors.isEmpty && driver != nil }
}

struct DriverInfo: Decodable, Hashable, Sendable {
    let name: String
    let description: String
    let manufacturer: String
    let year: String
    let cloneof: String?
    let sourcefile: String
    let unofficial: Bool
    let status: String
    let biossets: [BiosSet]
    let devices: [String]
    let slots: [Slot]
}
