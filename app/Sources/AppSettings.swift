// App settings: what is the same for every machine. They are stored in
// UserDefaults and given to the core (Core.swift), which adds them to
// the command line at run time; they are not stored in configurations.

import SwiftUI
import CPinesy

enum AppSettings {
    enum Debugger: String, CaseIterable, Identifiable {
        case windows, gdbStub
        var id: String { rawValue }

        var label: String {
            switch self {
            case .windows: return "Debugger windows"
            case .gdbStub: return "GDB remote stub"
            }
        }
    }

    enum Key {
        static let romFolders = "romFolders"
        static let dataFolder = "dataFolder"
        static let debugger = "debugger"
        static let gdbPort = "gdbPort"
        static let startPaused = "startPaused"
        static let piGDBPort = "piGdbPort"
    }

    static let defaultGDBPort = Int(PINESY_CORE_DEFAULT_GDB_PORT)
    static let defaultPiGDBPort = Int(PINESY_CORE_DEFAULT_PI_GDB_PORT)
    static let gdbPorts = Int(PINESY_CORE_MIN_GDB_PORT)...Int(PINESY_CORE_MAX_GDB_PORT)

    /// Where MAME's files go when Settings names no folder.
    static var defaultDataFolder: URL {
        FileManager.default.urls(for: .applicationSupportDirectory, in: .userDomainMask)[0]
            .appendingPathComponent("Pinesy", isDirectory: true)
    }
}

struct SettingsView: View {
    var body: some View {
        TabView {
            Tab("General", systemImage: "gearshape") {
                GeneralSettings()
            }
            Tab("Debugger", systemImage: "ladybug") {
                DebuggerSettings()
            }
        }
        .frame(width: 520)
    }
}

private struct GeneralSettings: View {
    @AppStorage(AppSettings.Key.dataFolder) private var dataFolder = ""
    @State private var romFolders = UserDefaults.standard.stringArray(forKey: AppSettings.Key.romFolders) ?? []
    @State private var selection: String?
    @State private var choosingROMFolder = false
    @State private var choosingDataFolder = false
    @State private var rejectedFolder: String?

    var body: some View {
        Form {
            Section {
                List(romFolders, id: \.self, selection: $selection) { folder in
                    Text(folder)
                        .lineLimit(1)
                        .truncationMode(.middle)
                }
                .frame(minHeight: 90)
                HStack {
                    Button("Add Folder…") { choosingROMFolder = true }
                    Button("Remove") {
                        romFolders.removeAll { $0 == selection }
                        save()
                    }
                    .disabled(selection == nil)
                }
            } header: {
                Text("ROM folders")
            } footer: {
                Text("Pinesy also looks for ROMs in these folders.")
                    .foregroundStyle(.secondary)
            }

            Section {
                LabeledContent("Folder") {
                    HStack {
                        Text(dataFolder.isEmpty ? AppSettings.defaultDataFolder.path : dataFolder)
                            .lineLimit(1)
                            .truncationMode(.middle)
                        Button("Choose…") { choosingDataFolder = true }
                        if !dataFolder.isEmpty {
                            Button("Use Default") { dataFolder = "" }
                        }
                    }
                }
            } header: {
                Text("Machine settings and snapshots")
            }
        }
        .formStyle(.grouped)
        .scrollDisabled(true)
        .fixedSize(horizontal: false, vertical: true)
        .fileImporter(isPresented: $choosingROMFolder, allowedContentTypes: [.folder]) { result in
            if case .success(let url) = result, !romFolders.contains(url.path) {
                if Core.isUsableROMFolder(url.path) {
                    romFolders.append(url.path)
                    save()
                } else {
                    rejectedFolder = url.path
                }
            }
        }
        .alert("This folder cannot be a ROM folder", isPresented: Binding(get: { rejectedFolder != nil },
                                                                          set: { if !$0 { rejectedFolder = nil } })) {
            Button("OK") { rejectedFolder = nil }
        } message: {
            Text("The emulator cannot use a folder with \";\" in its path: \(rejectedFolder ?? "")")
        }
        .fileImporter(isPresented: $choosingDataFolder, allowedContentTypes: [.folder]) { result in
            if case .success(let url) = result {
                dataFolder = url.path
            }
        }
    }

    private func save() {
        UserDefaults.standard.set(romFolders, forKey: AppSettings.Key.romFolders)
    }
}

private struct DebuggerSettings: View {
    @AppStorage(AppSettings.Key.debugger) private var debugger = AppSettings.Debugger.windows
    @AppStorage(AppSettings.Key.gdbPort) private var gdbPort = AppSettings.defaultGDBPort
    @AppStorage(AppSettings.Key.startPaused) private var startPaused = false
    @AppStorage(AppSettings.Key.piGDBPort) private var piGDBPort = AppSettings.defaultPiGDBPort

    var body: some View {
        Form {
            Section {
                Picker("Run with Debugger uses", selection: $debugger) {
                    ForEach(AppSettings.Debugger.allCases) { kind in
                        Text(kind.label).tag(kind)
                    }
                }
                .pickerStyle(.radioGroup)
                TextField("GDB port", value: $gdbPort, format: .number.grouping(.never))
                    .disabled(debugger != .gdbStub)
                    .onChange(of: gdbPort) {
                        let clamped = min(max(gdbPort, AppSettings.gdbPorts.lowerBound), AppSettings.gdbPorts.upperBound)
                        if clamped != gdbPort { gdbPort = clamped }
                    }
                TextField("Pi GDB port", value: $piGDBPort, format: .number.grouping(.never))
                    .onChange(of: piGDBPort) {
                        let clamped = min(max(piGDBPort, AppSettings.gdbPorts.lowerBound), AppSettings.gdbPorts.upperBound)
                        if clamped != piGDBPort { piGDBPort = clamped }
                    }
                Toggle("Start paused", isOn: $startPaused)
            } footer: {
                Text("GDB clients connect to these ports: the Next's, and the Pi's when the configuration has a Pi SD card. With Start paused, the Next waits at its first instruction until you continue it.")
                    .foregroundStyle(.secondary)
            }
        }
        .formStyle(.grouped)
    }
}
