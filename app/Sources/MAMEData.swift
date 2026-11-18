// MAME's slots and media, as the core's summary gives them
// (pinesy_core_summary()). Field names are MAME's, as pinesy.h
// documents them. Only the fields the app shows are read.

struct Slot: Decodable, Hashable, Sendable {
    /// The option name on the command line, such as "exp" or "exp:intf1:exp".
    let tag: String
    /// The fitted card; empty for none.
    let current: String
    /// The fitted card's description.
    let currentdescription: String?
}

struct Media: Decodable, Hashable, Sendable {
    /// The option name on the command line, such as "cass" or "pi".
    let name: String
    /// The file extensions MAME lists for the device, without dots.
    let extensionList: [String]
    let interface: String
    let mandatory: Bool
    /// The file named on the command line; empty for none.
    let current: String
}
