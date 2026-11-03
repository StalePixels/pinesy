// Headless mode: MAME runs with the given command line and no UI. The
// app's own paths come first, so the caller's options override them.

import AppKit

enum Headless {
    @MainActor
    static func run(_ arguments: [String]) -> Int32 {
        // MAME pumps AppKit events while it runs, so NSApp must exist.
        // The prohibited policy keeps the process out of the Dock and
        // stops it taking focus. A process with that policy cannot show
        // windows, so a run with MAME's debugger windows is a regular app.
        let app = NSApplication.shared
        app.setActivationPolicy(opensDebuggerWindows(arguments) ? .regular : .prohibited)
        app.finishLaunching()
        MAMEPaths.createFolders()
        return MAME.run(MAMEPaths.options() + arguments, view: nil)
    }

    /// True when the command line asks for the debugger and does not pick
    /// one without windows (none, or the GDB stub). The last -debugger wins,
    /// as in MAME.
    static func opensDebuggerWindows(_ arguments: [String]) -> Bool {
        let debug = arguments.contains { $0 == "-debug" || $0 == "-d" }
        guard debug else { return false }
        var debugger = "auto"
        for (index, argument) in arguments.enumerated() where argument == "-debugger" && index + 1 < arguments.count {
            debugger = arguments[index + 1]
        }
        return debugger != "none" && debugger != "gdbstub"
    }
}
