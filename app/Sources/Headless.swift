// Headless mode: MAME runs with the given command line and no UI.

import AppKit

enum Headless {
    @MainActor
    static func run(_ arguments: [String]) -> Int32 {
        // MAME pumps AppKit events while it runs, so NSApp must exist.
        // The prohibited policy keeps the process out of the Dock and
        // stops it taking focus.
        let app = NSApplication.shared
        app.setActivationPolicy(.prohibited)
        app.finishLaunching()
        return MAME.run(arguments, view: nil)
    }
}
