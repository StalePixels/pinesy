// The configuration window: the machine, its hardware, its media and
// its command line. Everything shown comes from the core's summary of
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
    /// The media device the file chooser is for. The chooser has its own
    /// flag, so closing the chooser does not clear the device before the
    /// chosen file is set.
    @State private var mediaToChoose: Media?
    @State private var choosingMedia = false
    /// Why the chosen file was refused, shown in an alert.
    @State private var refusedFile: String?
    /// The configuration changed while a machine ran; MAME is asked about
    /// it when the run ends.
    @State private var refreshWaiting = false

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
            MachinePicker(catalogue: Catalogue.shared,
                          canChoose: runner.running == nil,
                          onChoose: { machine in
                              document.machine = machine.id
                              document.arguments = Core.newConfiguration(machine.id)
                              choosingMachine = false
                          },
                          onCancel: {
                              choosingMachine = false
                              window?.close()
                          })
        }
        // Any file can be chosen: MAME opens files with extensions that its
        // list for the device leaves out, such as .img SD card images.
        .fileImporter(isPresented: $choosingMedia, allowedContentTypes: [.data]) { result in
            if case .success(let url) = result, let media = mediaToChoose {
                if media.interface == "sdcard", let problem = Core.sdCardProblem(url.path) {
                    refusedFile = problem
                } else {
                    document.arguments = Core.setMedia(document.arguments, media: media.name, path: url.path)
                }
            }
            mediaToChoose = nil
        }
        .alert("This SD card image cannot be used",
               isPresented: Binding(get: { refusedFile != nil }, set: { if !$0 { refusedFile = nil } })) {
            Button("OK") {}
        } message: {
            Text(refusedFile ?? "")
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
        let summary = Core.summary(document.arguments, machineHint: document.machine)
        self.summary = summary
        if let machine = summary?.machine?.id, document.machine != machine {
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
                        mediaRow(row, missing: summary.missingFiles.contains(row.media.name), sdCardProblem: summary.sdCardProblems[row.media.name])
                    }
                }
            }

            Section {
                Text(Core.copyCommandLine(document.arguments))
                    .font(.system(.callout, design: .monospaced))
                    .textSelection(.enabled)
                    .frame(maxWidth: .infinity, alignment: .leading)
                HStack {
                    Spacer()
                    Button("Copy Command Line", action: copyCommandLine)
                }
            } header: {
                Text("Command line")
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
                    set: { document.arguments = Core.setDriver(document.arguments, to: $0) })) {
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
                        document.arguments = Core.setROM(document.arguments, driver: choice.driver, bios: choice.bios)
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
                set: { document.arguments = Core.setSlot(document.arguments, slot: row.slot.tag, card: $0) })) {
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
                set: { document.arguments = Core.setCardROM(document.arguments, slot: row.slot.tag, bios: $0) })) {
                ForEach(row.roms, id: \.value) { rom in
                    Text(rom.label).tag(rom.value)
                }
            } label: {
                Text("ROM").padding(.leading, indent + 16)
            }
        }
    }

    @ViewBuilder
    private func mediaRow(_ row: ConfigurationSummary.MediaRow, missing: Bool, sdCardProblem: String?) -> some View {
        let media = row.media
        let problem: String? = media.mandatory && media.current.isEmpty
            ? "This machine needs a file here before it can run."
            : missing ? "File not found: \(media.current)"
            : sdCardProblem
        LabeledContent {
            HStack(spacing: 6) {
                if problem != nil {
                    Image(systemName: "exclamationmark.triangle.fill")
                        .foregroundStyle(.red)
                }
                Text(media.current.isEmpty ? "Empty" : (media.current as NSString).lastPathComponent)
                    .foregroundStyle(media.current.isEmpty ? .secondary : .primary)
                    .lineLimit(1)
                    .truncationMode(.middle)
                    .help(media.current)
                Button("Choose…") { mediaToChoose = media; choosingMedia = true }
                if !media.current.isEmpty {
                    Button("Eject", systemImage: "eject") {
                        document.arguments = Core.setMedia(document.arguments, media: media.name, path: nil)
                    }
                    .labelStyle(.iconOnly)
                    .buttonStyle(.borderless)
                    .help("Remove the file")
                }
            }
        } label: {
            VStack(alignment: .leading, spacing: 1) {
                Text(row.label)
                if let problem {
                    Text(problem)
                        .font(.caption)
                        .foregroundStyle(.red)
                        .fixedSize(horizontal: false, vertical: true)
                        .textSelection(.enabled)
                }
                Text(media.extensionList.map { "." + $0 }.joined(separator: " "))
                    .font(.caption)
                    .foregroundStyle(.secondary)
                    .lineLimit(1)
                    .truncationMode(.tail)
            }
        }
    }

    // MARK: - Actions

    private func run(debug: Bool) {
        guard canRun else { return }
        runner.start(title: title, arguments: document.arguments, debug: debug)
    }

    private func copyCommandLine() {
        guard !document.arguments.isEmpty else { return }
        NSPasteboard.general.clearContents()
        NSPasteboard.general.setString(Core.copyCommandLine(document.arguments), forType: .string)
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
