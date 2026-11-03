// The machine chooser, a sheet on a new configuration window: makers
// on the left, their machines on the right, and a search field.

import AppKit
import SwiftUI

struct MachinePicker: View {
    let catalogue: Catalogue
    /// False while a machine runs: a new configuration waits for it.
    let canChoose: Bool
    let onChoose: (Catalogue.Machine) -> Void
    let onCancel: () -> Void

    @State private var search = ""
    @State private var section: String?
    @State private var selection: String?

    private var searching: Bool {
        !search.trimmingCharacters(in: .whitespaces).isEmpty
    }

    private var shownMachines: [Catalogue.Machine] {
        if searching {
            return catalogue.search(search)
        }
        return catalogue.sections.first { $0.name == section }?.machines ?? []
    }

    private var showsMaker: Bool {
        searching || catalogue.sections.first { $0.name == section }?.showsMaker == true
    }

    // While searching, the results come from every maker, so no maker is selected.
    private var sidebarSelection: Binding<String?> {
        Binding(get: { searching ? nil : section },
                set: { if !searching { section = $0 } })
    }

    private var chosen: Catalogue.Machine? {
        shownMachines.first { $0.id == selection }
    }

    var body: some View {
        VStack(spacing: 0) {
            HStack {
                Text("Choose a Machine")
                    .font(.headline)
                Spacer()
                SearchField(text: $search, prompt: "Search machines")
                    .frame(width: 220)
            }
            .padding()

            HStack(spacing: 0) {
                List(catalogue.sections, selection: sidebarSelection) { section in
                    Text(section.name)
                        .tag(section.name)
                }
                .listStyle(.sidebar)
                .frame(width: 210)
                .disabled(searching)

                Divider()

                List(shownMachines, selection: $selection) { machine in
                    MachineRow(machine: machine, showsMaker: showsMaker)
                        .tag(machine.id)
                }
                .contextMenu(forSelectionType: String.self, menu: { _ in EmptyView() }) { ids in
                    if canChoose, let id = ids.first, let machine = shownMachines.first(where: { $0.id == id }) {
                        onChoose(machine)
                    }
                }
                .overlay {
                    if catalogue.sections.isEmpty {
                        ContentUnavailableView("No Machines", systemImage: "exclamationmark.triangle",
                                               description: Text("The emulator reported no machines."))
                    } else if shownMachines.isEmpty && searching {
                        ContentUnavailableView.search(text: search)
                    }
                }
            }

            Divider()

            HStack {
                Spacer()
                Button("Cancel", role: .cancel, action: onCancel)
                    .keyboardShortcut(.cancelAction)
                Button("Choose") {
                    if let chosen { onChoose(chosen) }
                }
                .keyboardShortcut(.defaultAction)
                .disabled(chosen == nil || !canChoose)
                .help(canChoose ? "" : "A machine is running. Choose when it stops.")
            }
            .padding()
        }
        .frame(width: 720, height: 480)
        .onAppear {
            section = catalogue.sections.first?.name
        }
        .onChange(of: section) {
            selection = nil
        }
    }
}

private struct MachineRow: View {
    let machine: Catalogue.Machine
    let showsMaker: Bool

    var body: some View {
        HStack(alignment: .firstTextBaseline) {
            VStack(alignment: .leading, spacing: 2) {
                Text(machine.name)
                if showsMaker, !machine.manufacturer.hasPrefix("<") {
                    Text(machine.manufacturer)
                        .font(.caption)
                        .foregroundStyle(.secondary)
                }
            }
            Spacer()
            if machine.preliminary {
                Text("Partly working")
                    .font(.caption)
                    .foregroundStyle(.orange)
                    .help("The emulation of this machine is not complete. Some software will not run correctly.")
            }
            Text(machine.year.contains("?") ? "" : machine.year)
                .foregroundStyle(.secondary)
                .monospacedDigit()
        }
        .padding(.vertical, 2)
    }
}

/// The standard AppKit search field.
struct SearchField: NSViewRepresentable {
    @Binding var text: String
    let prompt: String

    func makeNSView(context: Context) -> NSSearchField {
        let field = NSSearchField()
        field.placeholderString = prompt
        field.delegate = context.coordinator
        return field
    }

    func updateNSView(_ field: NSSearchField, context: Context) {
        if field.stringValue != text {
            field.stringValue = text
        }
    }

    func makeCoordinator() -> Coordinator {
        Coordinator(text: $text)
    }

    final class Coordinator: NSObject, NSSearchFieldDelegate {
        let text: Binding<String>

        init(text: Binding<String>) {
            self.text = text
        }

        func controlTextDidChange(_ notification: Notification) {
            if let field = notification.object as? NSSearchField {
                text.wrappedValue = field.stringValue
            }
        }
    }
}
