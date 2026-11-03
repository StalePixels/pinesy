// Command-line mode: MAME runs with the given command line, as `mame`
// would. The app's own paths come first, so the caller's options
// override them. With -video none there is no window at all; otherwise
// the machine gets its own window, and closing it stops the machine.

import AppKit

enum Headless {
    @MainActor
    static func run(_ arguments: [String]) -> Int32 {
        // MAME pumps AppKit events while it runs, so NSApp must exist.
        let app = NSApplication.shared
        MAMEPaths.createFolders()
        let commandLine = MAMEPaths.options() + arguments

        if videoNone(arguments) {
            // The prohibited policy keeps the process out of the Dock and
            // stops it taking focus. A process with that policy cannot show
            // windows, so a run with MAME's debugger windows is a regular app.
            app.setActivationPolicy(opensDebuggerWindows(arguments) ? .regular : .prohibited)
            app.finishLaunching()
            return MAME.run(commandLine, view: nil)
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
        let code = MAME.run(commandLine, view: view, host: host)
        withExtendedLifetime(closer) {}
        return code
    }

    /// True when the last -video option is "none", as in MAME.
    static func videoNone(_ arguments: [String]) -> Bool {
        var video = ""
        for (index, argument) in arguments.enumerated() where argument == "-video" && index + 1 < arguments.count {
            video = arguments[index + 1]
        }
        return video == "none"
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

/// Closing the machine window stops the machine.
private final class WindowCloser: NSObject, NSWindowDelegate {
    func windowShouldClose(_ sender: NSWindow) -> Bool {
        _ = MAME.requestExit()
        return false
    }
}
