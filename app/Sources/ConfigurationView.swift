// The configuration window: the machine, its hardware, its media and
// its command line. Everything shown comes from MAME's description of
// the document's command line, asked again after every change.

import AppKit
import SwiftUI
import UniformTypeIdentifiers

struct ConfigurationActions {
    let canRun: Bool
    let run: () -> Void
    let runWithDebugger: () -> Void
    let copyCommandLine: () -> Void
}

extension FocusedValues {
    @Entry var configurationActions: ConfigurationActions?
}

struct ConfigurationView: View {
    @Binding var document: ConfigurationDocument
    let fileURL: URL?

    @ObservedObject private var runner = MachineRunner.shared
    @State private var summary: ConfigurationSummary?
    @State private var choosingMachine = false
    @State private var window: NSWindow?
    @State private var mediaToChoose: Media?
    /// The configuration changed while a machine ran; MAME is asked about
    /// it when the run ends.
    @State private var refreshWaiting = false

    private var catalogue: Catalogue { Catalogue.shared }

    private var title: String {
        fileURL?.deletingPathExtension().lastPathComponent
            ?? window?.title
            ?? summary?.machine?.name
            ?? "Untitled"
    }

    private var canRun: Bool {
        runner.running == nil && summary?.canRun == true
    }

    var body: some View {
        Group {
            if let summary {
                form(summary)
                    .disabled(runner.running != nil)
            } else if refreshWaiting {
                ContentUnavailableView("A Machine Is Running", systemImage: "play.fill",
                                       description: Text("This configuration opens when the running machine stops."))
            } else {
                ContentUnavailableView("No Machine", systemImage: "desktopcomputer",
                                       description: Text("Choose a machine for this configuration."))
            }
        }
        .frame(minWidth: 520, idealWidth: 620, minHeight: 420, idealHeight: 720)
        .toolbar {
            ToolbarItemGroup(placement: .primaryAction) {
                Button("Run with Debugger", systemImage: "ladybug") { run(debug: true) }
                    .disabled(!canRun)
                    .help("Run this configuration with the debugger")
                Button("Run", systemImage: "play.fill") { run(debug: false) }
                    .disabled(!canRun)
                    .help("Run this configuration")
            }
        }
        .focusedSceneValue(\.configurationActions,
                           ConfigurationActions(canRun: canRun,
                                                run: { run(debug: false) },
                                                runWithDebugger: { run(debug: true) },
                                                copyCommandLine: copyCommandLine))
        .background(WindowReader { window = $0 })
        .sheet(isPresented: $choosingMachine) {
            MachinePicker(catalogue: catalogue,
                          canChoose: runner.running == nil,
                          onChoose: { machine in
                              document.machine = machine.id
                              document.arguments = ConfigurationEditor.newConfiguration(machine, catalogue: catalogue)
                              choosingMachine = false
                          },
                          onCancel: {
                              choosingMachine = false
                              window?.close()
                          })
        }
        .fileImporter(isPresented: Binding(get: { mediaToChoose != nil }, set: { if !$0 { mediaToChoose = nil } }),
                      allowedContentTypes: mediaToChoose.map(Self.contentTypes) ?? [.data]) { result in
            if case .success(let url) = result, let media = mediaToChoose {
                document.arguments = ConfigurationEditor.setMedia(document.arguments, media: media, path: url.path)
            }
            mediaToChoose = nil
        }
        .onAppear {
            if document.arguments.isEmpty {
                choosingMachine = true
            }
        }
        .onChange(of: document.arguments, initial: true) {
            refresh()
        }
        .onChange(of: runner.running) {
            if runner.running == nil && refreshWaiting {
                refresh()
            }
        }
    }

    private func refresh() {
        guard !document.arguments.isEmpty else {
            summary = nil
            return
        }
        // pinesy_describe() must not run inside a running MAME
        guard runner.running == nil else {
            refreshWaiting = true
            return
        }
        refreshWaiting = false
        let summary = ConfigurationSummary(arguments: document.arguments,
                                           description: MAME.describe(document.arguments),
                                           catalogue: catalogue,
                                           machineHint: document.machine)
        self.summary = summary
        if let machine = summary.machine?.id, document.machine != machine {
            document.machine = machine
        }
    }

    // MARK: - The form

    @ViewBuilder
    private func form(_ summary: ConfigurationSummary) -> some View {
        Form {
            if !summary.errors.isEmpty {
                Section {
                    Label {
                        Text(summary.errors)
                            .textSelection(.enabled)
                    } icon: {
                        Image(systemName: "exclamationmark.triangle.fill")
                            .foregroundStyle(.yellow)
                    }
                } header: {
                    Text("This configuration cannot run")
                }
            }

            if !summary.warnings.isEmpty {
                Section {
                    Label {
                        Text(summary.warnings)
                            .textSelection(.enabled)
                    } icon: {
                        Image(systemName: "info.circle")
                            .foregroundStyle(.secondary)
                    }
                } header: {
                    Text("The emulator warns")
                }
            }

            machineSection(summary)

            if !summary.hardware.isEmpty {
                Section("Hardware") {
                    ForEach(summary.hardware) { row in
                        hardwareRow(row)
                    }
                }
            }

            if !summary.media.isEmpty {
                Section("Media") {
                    ForEach(summary.media) { row in
                        mediaRow(row, missing: summary.missingFiles.contains(row.media.name))
                    }
                }
            }

            Section {
                Text(RunOptions.commandLine(document.arguments))
                    .font(.system(.callout, design: .monospaced))
                    .textSelection(.enabled)
                    .frame(maxWidth: .infinity, alignment: .leading)
                HStack {
                    Spacer()
                    Button("Copy Command Line", action: copyCommandLine)
                }
            } header: {
                Text("Command line")
            } footer: {
                Text("The same machine, run headless from a terminal.")
                    .foregroundStyle(.secondary)
            }
        }
        .formStyle(.grouped)
    }

    @ViewBuilder
    private func machineSection(_ summary: ConfigurationSummary) -> some View {
        Section {
            VStack(alignment: .leading, spacing: 2) {
                Text(summary.machine?.name ?? summary.driver)
                    .font(.title2.weight(.semibold))
                if let machine = summary.machine {
                    Text([machine.manufacturer.hasPrefix("<") ? nil : machine.manufacturer,
                          summary.year.isEmpty || summary.year.contains("?") ? nil : summary.year]
                        .compactMap { $0 }.joined(separator: ", "))
                        .foregroundStyle(.secondary)
                }
                if summary.preliminary {
                    Text("The emulation of this machine is not complete.")
                        .font(.caption)
                        .foregroundStyle(.orange)
                }
            }

            if summary.models.count > 1 {
                Picker("Model", selection: Binding(
                    get: { summary.driver },
                    set: { document.arguments = ConfigurationEditor.setDriver(document.arguments, to: $0, catalogue: catalogue) })) {
                    ForEach(summary.models, id: \.driver) { model in
                        Text(model.year.contains("?") ? model.label : "\(model.label) (\(model.year))")
                            .tag(model.driver)
                    }
                }
            }

            let roms = summary.roms + summary.variantROMs
            if roms.count > 1 {
                Picker("ROM", selection: Binding(
                    get: { summary.currentROM },
                    set: { choice in
                        guard let choice else { return }
                        document.arguments = ConfigurationEditor.setROM(document.arguments, driver: choice.driver,
                                                                        bios: choice.bios, catalogue: catalogue)
                    })) {
                    ForEach(summary.roms, id: \.self) { rom in
                        Text(rom.label).tag(Optional(rom))
                    }
                    if !summary.variantROMs.isEmpty {
                        Divider()
                        ForEach(summary.variantROMs, id: \.self) { rom in
                            Text(rom.label).tag(Optional(rom))
                        }
                    }
                }
            }
        }
    }

    @ViewBuilder
    private func hardwareRow(_ row: ConfigurationSummary.HardwareRow) -> some View {
        let indent = CGFloat(row.depth) * 16
        if let fixedText = row.fixedText {
            LabeledContent {
                Text(fixedText)
            } label: {
                Text(row.label).padding(.leading, indent)
            }
        } else {
            Picker(selection: Binding(
                get: { row.slot.current },
                set: { document.arguments = ConfigurationEditor.setSlot(document.arguments, slot: row.slot, card: $0, catalogue: catalogue) })) {
                ForEach(row.choices, id: \.value) { choice in
                    Text(choice.label).tag(choice.value)
                }
                if !row.choices.contains(where: { $0.value == row.slot.current }) {
                    Text(row.slot.currentdescription ?? row.slot.current).tag(row.slot.current)
                }
            } label: {
                VStack(alignment: .leading, spacing: 1) {
                    Text(row.label)
                    if let meantFor = row.meantFor {
                        Text("Made for: \(meantFor)")
                            .font(.caption)
                            .foregroundStyle(.secondary)
                    }
                }
                .padding(.leading, indent)
            }
        }
        if !row.roms.isEmpty {
            Picker(selection: Binding(
                get: { row.currentROM },
                set: { document.arguments = ConfigurationEditor.setCardROM(document.arguments, slot: row.slot, bios: $0, catalogue: catalogue) })) {
                ForEach(row.roms, id: \.value) { rom in
                    Text(rom.label).tag(rom.value)
                }
            } label: {
                Text("ROM").padding(.leading, indent + 16)
            }
        }
    }

    @ViewBuilder
    private func mediaRow(_ row: ConfigurationSummary.MediaRow, missing: Bool) -> some View {
        let media = row.media
        LabeledContent {
            HStack(spacing: 6) {
                if media.mandatory && media.current.isEmpty {
                    Image(systemName: "exclamationmark.triangle.fill")
                        .foregroundStyle(.yellow)
                        .help("This machine needs a file here before it can run.")
                }
                if missing {
                    Image(systemName: "exclamationmark.triangle.fill")
                        .foregroundStyle(.yellow)
                        .help("This file is not there: \(media.current)")
                }
                Text(media.current.isEmpty ? "Empty" : (media.current as NSString).lastPathComponent)
                    .foregroundStyle(media.current.isEmpty ? .secondary : .primary)
                    .lineLimit(1)
                    .truncationMode(.middle)
                    .help(media.current)
                Button("Choose…") { mediaToChoose = media }
                if !media.current.isEmpty {
                    Button("Eject", systemImage: "eject") {
                        document.arguments = ConfigurationEditor.setMedia(document.arguments, media: media, path: nil)
                    }
                    .labelStyle(.iconOnly)
                    .buttonStyle(.borderless)
                    .help("Remove the file")
                }
            }
        } label: {
            VStack(alignment: .leading, spacing: 1) {
                Text(row.label)
                Text(media.extensionList.map { "." + $0 }.joined(separator: " "))
                    .font(.caption)
                    .foregroundStyle(.secondary)
                    .lineLimit(1)
                    .truncationMode(.tail)
            }
        }
    }

    private static func contentTypes(_ media: Media) -> [UTType] {
        let types = media.extensionList.compactMap { UTType(filenameExtension: $0) }
        return types.isEmpty ? [.data] : types
    }

    // MARK: - Actions

    private func run(debug: Bool) {
        guard canRun else { return }
        runner.start(title: title, arguments: document.arguments, debug: debug)
    }

    private func copyCommandLine() {
        guard !document.arguments.isEmpty else { return }
        NSPasteboard.general.clearContents()
        NSPasteboard.general.setString(RunOptions.commandLine(document.arguments), forType: .string)
    }
}

/// Reports the window a view is in.
private struct WindowReader: NSViewRepresentable {
    let onWindow: (NSWindow?) -> Void

    func makeNSView(context: Context) -> NSView {
        let view = NSView()
        DispatchQueue.main.async { onWindow(view.window) }
        return view
    }

    func updateNSView(_ view: NSView, context: Context) {
        DispatchQueue.main.async { onWindow(view.window) }
    }
}
