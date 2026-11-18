// Runs one machine at a time in its own window. MAME draws into the
// window's view and pumps the app's events while it runs.

import AppKit
import Carbon.HIToolbox

@MainActor
final class MachineRunner: NSObject, ObservableObject, NSWindowDelegate {
    static let shared = MachineRunner()

    /// The title of the running machine's window, or nil when none runs.
    @Published private(set) var running: String?

    private var window: NSWindow?
    private var quitAfterRun = false

    /// Starts a configuration. `arguments` is its command line; the
    /// app's paths and the debugger options are added here.
    func start(title: String, arguments: [String], debug: Bool) {
        guard running == nil else { return }
        running = title

        // MAME sets the window's size and shape once it has the view
        let view = MachineView(frame: NSRect(x: 0, y: 0, width: 640, height: 480))
        let window = NSWindow(contentRect: view.frame,
                              styleMask: [.titled, .closable, .miniaturizable, .resizable],
                              backing: .buffered, defer: false)
        window.title = title
        window.contentView = view
        window.collectionBehavior.insert(.fullScreenPrimary)
        window.isReleasedWhenClosed = false
        window.delegate = self
        window.center()
        window.makeKeyAndOrderFront(nil)
        window.makeFirstResponder(view)
        self.window = window

        MAMEPaths.createFolders()
        let commandLine = MAMEPaths.options() + arguments + RunOptions.debugArguments(debug)

        // pinesy_run() keeps its own loop and pumps AppKit events. Start it
        // from a run loop block, not from the main dispatch queue: a nested
        // loop inside a main-queue block would hold back every other
        // main-queue job (SwiftUI's included) until MAME exits.
        RunLoop.main.perform(inModes: [.default]) {
            MainActor.assumeIsolated {
                let host = RunHost()
                // the host lives for the run only, so it holds the window strongly
                host.onAspect = { ratio in
                    if let ratio {
                        window.contentAspectRatio = ratio
                    } else {
                        // the aspect ratio and resize increments share one setting
                        window.resizeIncrements = NSSize(width: 1, height: 1)
                    }
                }
                var sized = false
                host.onSize = { size in
                    Self.setContentSize(window, size)
                    // the first size is MAME's window size; the window starts centred at it
                    if !sized {
                        sized = true
                        window.center()
                    }
                }
                let code = MAME.run(commandLine, view: view, host: host)
                self.finish(code: code, errors: host.errors)
            }
        }
    }

    /// Gives the window the content size MAME asks for, scaled down to fit
    /// the screen, keeping its top-left corner where it is.
    static func setContentSize(_ window: NSWindow, _ size: NSSize) {
        var size = size
        if let screen = (window.screen ?? NSScreen.main)?.visibleFrame.size {
            let chrome = window.frame.height - window.contentLayoutRect.height
            let scale = min(1, (screen.height - chrome) / size.height, screen.width / size.width)
            size = NSSize(width: (size.width * scale).rounded(), height: (size.height * scale).rounded())
        }
        var content = window.contentRect(forFrameRect: window.frame)
        content.origin.y += content.height - size.height
        content.size = size
        window.setFrame(window.frameRect(forContentRect: content), display: true)
    }

    private func finish(code: Int32, errors: [String]) {
        let title = running
        running = nil
        window?.delegate = nil
        window?.close()
        window = nil

        if quitAfterRun {
            NSApp.terminate(nil)
            return
        }
        if code != 0, let title {
            let alert = NSAlert()
            alert.messageText = "\(title) stopped with an error"
            let text = errors.joined().trimmingCharacters(in: .whitespacesAndNewlines)
            alert.informativeText = text.isEmpty ? "The emulator exited with code \(code)." : text
            alert.runModal()
        }
    }

    /// Asks the running machine to stop. The window closes when it has.
    func stop() {
        _ = MAME.requestExit()
    }

    /// Resets the running machine. A hard reset starts it again from power-on.
    func reset(hard: Bool) {
        _ = MAME.reset(hard: hard)
    }

    /// Asks the running machine to exit and quits when it has.
    /// Returns false if no machine is running.
    func quitWhenStopped() -> Bool {
        guard running != nil else { return false }
        quitAfterRun = true
        _ = MAME.requestExit()
        return true
    }

    // The window closes when MAME has exited.
    func windowShouldClose(_ sender: NSWindow) -> Bool {
        _ = MAME.requestExit()
        return false
    }
}

/// The view MAME draws on. Keys go to MAME; key combinations the menus
/// own reach the menus first.
final class MachineView: NSView {
    override var acceptsFirstResponder: Bool { true }

    override func keyDown(with event: NSEvent) {
        if !event.isARepeat {
            MAME.key(event.keyCode, down: true)
        }
        for scalar in (event.characters ?? "").unicodeScalars {
            var value = scalar.value
            if value == 0x7f { value = 0x08 }        // delete key types backspace
            if value < 0xf700 { MAME.character(value) } // skip function-key characters
        }
    }

    override func keyUp(with event: NSEvent) {
        MAME.key(event.keyCode, down: false)
    }

    override func flagsChanged(with event: NSEvent) {
        let flags = event.modifierFlags
        let down: Bool
        switch Int(event.keyCode) {
        case kVK_Shift, kVK_RightShift: down = flags.contains(.shift)
        case kVK_Control, kVK_RightControl: down = flags.contains(.control)
        case kVK_Option, kVK_RightOption: down = flags.contains(.option)
        case kVK_Command, kVK_RightCommand: down = flags.contains(.command)
        case kVK_CapsLock: down = flags.contains(.capsLock)
        default: return
        }
        MAME.key(event.keyCode, down: down)
    }
}
