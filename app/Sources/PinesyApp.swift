// The app: a Machine menu and the Pi emulation settings.

import SwiftUI

struct PinesyApp: App {
    @NSApplicationDelegateAdaptor(AppDelegate.self) private var appDelegate

    var body: some Scene {
        Settings {
            SettingsView()
        }
        .commands {
            MachineCommands(runner: MachineRunner.shared)
        }
    }
}

struct MachineCommands: Commands {
    @ObservedObject var runner: MachineRunner

    var body: some Commands {
        CommandMenu("Machine") {
            ForEach(MAME.machines) { machine in
                Button(machine.description) {
                    runner.start(machine)
                }
                .disabled(runner.running != nil)
            }
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
