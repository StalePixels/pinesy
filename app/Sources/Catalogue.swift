// The machines the user can choose, as the core's catalogue gives them
// (pinesy_core_catalogue()). The rules that build the list are the
// core's.

import Foundation

struct Catalogue: Sendable {
    struct Model: Decodable, Hashable, Sendable {
        let driver: String
        let label: String
        let year: String
    }

    struct Machine: Decodable, Identifiable, Hashable, Sendable {
        /// The driver that stands for the machine: the family parent
        /// when the machine has models, else its own driver.
        let id: String
        let name: String
        let manufacturer: String
        let year: String
        let preliminary: Bool
    }

    struct Section: Decodable, Identifiable, Hashable, Sendable {
        let name: String
        let machines: [Machine]
        /// True for "Other makers", where each row also shows the maker.
        let showsMaker: Bool
        var id: String { name }
    }

    let sections: [Section]

    @MainActor static let shared = Catalogue(sections: Core.catalogue())

    /// Machines whose name, maker, model or ROM variant name contains the text.
    @MainActor
    func search(_ text: String) -> [Machine] {
        Core.search(text)
    }
}
