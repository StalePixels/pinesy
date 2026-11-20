// The app: configuration documents, the Machine menu and Settings.

import SwiftUI

struct PinesyApp: App {
    @NSApplicationDelegateAdaptor(AppDelegate.self) private var appDelegate

    init() {
        // At a launch with no document to open, AppKit shows an Open panel
        // instead of a new document while this default is true, which it is
        // when iCloud Drive is on. Pinesy shows its recent configurations
        // or a new configuration (RecentConfigurations). A value the user
        // sets still wins.
        UserDefaults.standard.register(defaults: ["NSShowAppCentricOpenPanelInsteadOfUntitledFile": false])
    }

    var body: some Scene {
        DocumentGroup(newDocument: ConfigurationDocument()) { file in
            ConfigurationView(document: file.$document, fileURL: file.fileURL)
        }
        .defaultSize(width: 620, height: 720)
        .commands {
            MachineCommands(runner: MachineRunner.shared)
            ToolbarCommands()
        }

        Settings {
            SettingsView()
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
            Button("Reset") { runner.reset(hard: false) }
                .disabled(runner.running == nil)
            Button("Hard Reset") { runner.reset(hard: true) }
                .disabled(runner.running == nil)
            Button(runner.pointerHeld ? "Release Pointer" : "Grab Pointer") { runner.togglePointer() }
                .keyboardShortcut("m", modifiers: [.command, .shift])
                .disabled(runner.running == nil)
            Button("Clear NVRAM…") { actions?.clearNVRAM() }
                .disabled(actions?.canClearNVRAM != true)
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
