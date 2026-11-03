// Runs one machine at a time in its own window. MAME draws into the
// window's view and pumps the app's events while it runs.

import AppKit
import Carbon.HIToolbox

@MainActor
final class MachineRunner: NSObject, ObservableObject, NSWindowDelegate {
    static let shared = MachineRunner()

    /// The machine that is running, or nil.
    @Published private(set) var running: MAME.Machine?

    private var window: NSWindow?
    private var quitAfterRun = false

    func start(_ machine: MAME.Machine) {
        guard running == nil else { return }
        running = machine

        let view = MachineView(frame: NSRect(x: 0, y: 0, width: 720, height: 576))
        let window = NSWindow(contentRect: view.frame,
                              styleMask: [.titled, .closable, .miniaturizable, .resizable],
                              backing: .buffered, defer: false)
        window.title = machine.description
        window.contentView = view
        window.contentAspectRatio = view.frame.size
        window.isReleasedWhenClosed = false
        window.delegate = self
        window.center()
        window.makeKeyAndOrderFront(nil)
        window.makeFirstResponder(view)
        self.window = window

        let arguments = [machine.name] + piArguments()

        // pinesy_run() keeps its own loop and pumps AppKit events. Start it
        // from a run loop block, not from the main dispatch queue: a nested
        // loop inside a main-queue block would hold back every other
        // main-queue job (SwiftUI's included) until MAME exits.
        RunLoop.main.perform(inModes: [.default]) {
            MainActor.assumeIsolated {
                let code = MAME.run(arguments, view: view)
                self.finish(code: code)
            }
        }
    }

    /// MAME options for the Pi port, from the settings.
    private func piArguments() -> [String] {
        let defaults = UserDefaults.standard
        guard defaults.bool(forKey: SettingsKey.piEnabled) else {
            // an empty slot option unplugs the card
            return ["-rs232_rpi", ""]
        }
        let path = defaults.string(forKey: SettingsKey.piImagePath) ?? ""
        return path.isEmpty ? [] : ["-pi", path]
    }

    private func finish(code: Int32) {
        let machine = running
        running = nil
        window?.delegate = nil
        window?.close()
        window = nil

        if quitAfterRun {
            NSApp.terminate(nil)
            return
        }
        if code != 0, let machine {
            let alert = NSAlert()
            alert.messageText = "\(machine.description) stopped with an error"
            alert.informativeText = "MAME exited with code \(code)."
            alert.runModal()
        }
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

/// The view MAME draws on. Keys go to MAME.
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
