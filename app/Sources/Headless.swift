// Command-line mode: MAME runs with the given command line, as `mame`
// would, through the core's headless mode (pinesy_core_headless()),
// which puts the app's own paths first, so the caller's options
// override them. With -video none there is no window at all; otherwise
// the machine gets its own window, and closing it stops the machine.

import AppKit

enum Headless {
    @MainActor
    static func run(_ arguments: [String]) -> Int32 {
        // MAME pumps AppKit events while it runs, so NSApp must exist.
        let app = NSApplication.shared

        if Core.videoNone(arguments) {
            // The prohibited policy keeps the process out of the Dock and
            // stops it taking focus. A process with that policy cannot show
            // windows, so a run with MAME's debugger windows is a regular app.
            app.setActivationPolicy(Core.opensDebuggerWindows(arguments) ? .regular : .prohibited)
            app.finishLaunching()
            return Core.headless(arguments, host: nil)
        }

        app.setActivationPolicy(.regular)
        app.finishLaunching()
        let view = MachineView(frame: NSRect(x: 0, y: 0, width: 640, height: 480))
        let window = NSWindow(contentRect: view.frame,
                              styleMask: [.titled, .closable, .miniaturizable, .resizable],
                              backing: .buffered, defer: false)
        window.title = arguments.first { !$0.hasPrefix("-") } ?? "Pinesy"
        window.contentView = view
        window.isReleasedWhenClosed = false
        let closer = WindowCloser()
        window.delegate = closer
        window.center()
        window.makeKeyAndOrderFront(nil)
        window.makeFirstResponder(view)
        app.activate()

        let host = RunHost()
        host.onAspect = { ratio in
            if let ratio {
                window.contentAspectRatio = ratio
            } else {
                window.resizeIncrements = NSSize(width: 1, height: 1)
            }
        }
        var sized = false
        host.onSize = { size in
            MachineRunner.setContentSize(window, size)
            if !sized {
                sized = true
                window.center()
            }
        }
        // the OSD prints MAME's output itself, as usual
        let code = MAME.withHost(view: view, receiver: host) { Core.headless(arguments, host: $0) }
        withExtendedLifetime(closer) {}
        return code
    }
}

/// Closing the machine window stops the machine.
private final class WindowCloser: NSObject, NSWindowDelegate {
    func windowShouldClose(_ sender: NSWindow) -> Bool {
        _ = MAME.requestExit()
        return false
    }
}
