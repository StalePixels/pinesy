// The app: configuration documents, the Machine menu and Settings.

import SwiftUI

struct PinesyApp: App {
    @NSApplicationDelegateAdaptor(AppDelegate.self) private var appDelegate

    var body: some Scene {
        DocumentGroup(newDocument: ConfigurationDocument()) { file in
            ConfigurationView(document: file.$document, fileURL: file.fileURL)
        }
        .defaultSize(width: 620, height: 720)
        .commands {
            FileCommands()
            MachineCommands(runner: MachineRunner.shared)
            ToolbarCommands()
        }

        Settings {
            SettingsView()
        }
    }
}

struct FileCommands: Commands {
    var body: some Commands {
        CommandGroup(replacing: .newItem) {
            Button("New Configuration…") {
                NSDocumentController.shared.newDocument(nil)
            }
            .keyboardShortcut("n")
            Button("Open…") {
                NSDocumentController.shared.openDocument(nil)
            }
            .keyboardShortcut("o")
            Menu("Open Recent") {
                ForEach(NSDocumentController.shared.recentDocumentURLs, id: \.self) { url in
                    Button(url.deletingPathExtension().lastPathComponent) {
                        NSDocumentController.shared.openDocument(withContentsOf: url, display: true) { _, _, _ in }
                    }
                }
                Divider()
                Button("Clear Menu") {
                    NSDocumentController.shared.clearRecentDocuments(nil)
                }
            }
        }
    }
}

struct MachineCommands: Commands {
    @ObservedObject var runner: MachineRunner
    @FocusedValue(\.configurationActions) private var actions

    var body: some Commands {
        CommandMenu("Machine") {
            Button("Run") { actions?.run() }
                .keyboardShortcut("r")
                .disabled(actions?.canRun != true)
            Button("Run with Debugger") { actions?.runWithDebugger() }
                .keyboardShortcut("r", modifiers: [.command, .option])
                .disabled(actions?.canRun != true)
            Button("Stop") { runner.stop() }
                .keyboardShortcut(".")
                .disabled(runner.running == nil)
            Divider()
            Button("Copy Command Line") { actions?.copyCommandLine() }
                .disabled(actions == nil)
        }
    }
}

final class AppDelegate: NSObject, NSApplicationDelegate {
    func applicationShouldTerminate(_ sender: NSApplication) -> NSApplication.TerminateReply {
        // MAME must leave pinesy_run() first; the runner quits after that.
        MainActor.assumeIsolated {
            MachineRunner.shared.quitWhenStopped() ? .terminateCancel : .terminateNow
        }
    }
}
