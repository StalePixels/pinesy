// App settings: what is the same for every machine. They are added to
// the command line at run time and are not stored in configurations.

import SwiftUI

struct AppSettings {
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
    }

    static let defaultGDBPort = 23946
    static let gdbPorts = 1...65535

    static var defaultDataFolder: URL {
        FileManager.default.urls(for: .applicationSupportDirectory, in: .userDomainMask)[0]
            .appendingPathComponent("Pinesy", isDirectory: true)
    }

    var romFolders: [String]
    var dataFolder: URL
    var debugger: Debugger
    var gdbPort: Int

    static var current: AppSettings {
        let defaults = UserDefaults.standard
        let folder = defaults.string(forKey: Key.dataFolder).flatMap { $0.isEmpty ? nil : URL(fileURLWithPath: $0) }
        let port = defaults.integer(forKey: Key.gdbPort)
        return AppSettings(romFolders: defaults.stringArray(forKey: Key.romFolders) ?? [],
                           dataFolder: folder ?? defaultDataFolder,
                           debugger: Debugger(rawValue: defaults.string(forKey: Key.debugger) ?? "") ?? .windows,
                           gdbPort: gdbPorts.contains(port) ? port : defaultGDBPort)
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

    private var dataFolderURL: URL {
        dataFolder.isEmpty ? AppSettings.defaultDataFolder : URL(fileURLWithPath: dataFolder)
    }

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
                Text("Pinesy looks for machine ROMs in the ROM sets it comes with, then in \(MAMEPaths.userROMs(dataFolderURL).path), then in these folders.")
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
            } footer: {
                Text("MAME keeps each machine's settings, memory contents and snapshots in this folder.")
                    .foregroundStyle(.secondary)
            }
        }
        .formStyle(.grouped)
        .scrollDisabled(true)
        .fixedSize(horizontal: false, vertical: true)
        .fileImporter(isPresented: $choosingROMFolder, allowedContentTypes: [.folder]) { result in
            if case .success(let url) = result, !romFolders.contains(url.path) {
                if MAMEPaths.isUsableROMFolder(url.path) {
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
            } footer: {
                Text("With the GDB remote stub, the machine waits for a GDB client to connect to this port.")
                    .foregroundStyle(.secondary)
            }
        }
        .formStyle(.grouped)
    }
}
