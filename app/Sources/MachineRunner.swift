// Runs one machine at a time in its own window. MAME draws into the
// window's view and pumps the app's events while it runs.

import AppKit
import Carbon.HIToolbox

@MainActor
final class MachineRunner: NSObject, ObservableObject, NSWindowDelegate {
    static let shared = MachineRunner()

    /// The title of the running machine's window, or nil when none runs.
    @Published private(set) var running: String?

    /// The running machine holds the pointer in its view.
    @Published private(set) var pointerHeld = false

    private var window: NSWindow?
    private var quitAfterRun = false
    /// The running configuration's window size, and the last frame size
    /// MAME gave, to size the window again when it leaves full screen.
    private var windowSize = Core.windowFit
    private var frame: NSSize?

    /// Starts a configuration. `arguments` is its command line; the core
    /// adds the app's paths and the debugger options. `windowSize` is
    /// the configuration's window size. `takesMouse` is the summary's
    /// mouse and `mouse` the configuration's mouse setting. `piPaused` is
    /// the configuration's Pi pause setting.
    func start(title: String, arguments: [String], debug: Bool, windowSize: Int, takesMouse: Bool, mouse: Bool, piPaused: Bool) {
        guard running == nil else { return }
        running = title
        self.windowSize = windowSize
        frame = nil

        // the window takes its size and shape from the first frame
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
        let continueButton = ContinueAccessory()
        window.addTitlebarAccessoryViewController(continueButton)
        window.makeKeyAndOrderFront(nil)
        window.makeFirstResponder(view)
        self.window = window

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
                host.onSize = { frame in
                    self.frame = frame
                    Self.fit(window, frame: frame, windowSize: windowSize)
                    // the window starts centred at its first size, and
                    // goes to full screen from there when the size says so
                    if !sized {
                        sized = true
                        window.center()
                        if Core.windowOpensFullScreen(windowSize) {
                            window.toggleFullScreen(nil)
                        }
                    }
                }
                host.onPointer = { hide, capture in
                    view.setPointer(hide: hide, capture: capture)
                    self.pointerHeld = capture
                }
                host.onStopped = { stopped in
                    continueButton.isHidden = !stopped
                }
                let code = MAME.withHost(view: view, receiver: host) {
                    Core.run(arguments, debug: debug, takesMouse: takesMouse, mouse: mouse, piPaused: piPaused, host: $0)
                }
                self.finish(code: code, errors: host.errors)
            }
        }
    }

    /// Gives the window the content size the core decides for a frame of
    /// `frame` pixels and the window size, on the window's screen,
    /// keeping its top-left corner where it is. A window in full screen
    /// keeps the screen's size.
    static func fit(_ window: NSWindow, frame: NSSize, windowSize: Int) {
        guard !window.styleMask.contains(.fullScreen) else { return }
        var usable = NSSize.zero
        if let visible = (window.screen ?? NSScreen.main)?.visibleFrame {
            let chrome = window.frame.height - window.contentLayoutRect.height
            usable = NSSize(width: visible.width, height: visible.height - chrome)
        }
        guard let size = Core.windowContentSize(frame: frame, windowSize: windowSize,
                                                pixelRatio: window.backingScaleFactor, usable: usable) else { return }
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

    /// Releases a held pointer, or grabs it again.
    func togglePointer() {
        MAME.togglePointer()
    }

    /// Asks the running machine to exit and quits when it has.
    /// Returns false if no machine is running.
    func quitWhenStopped() -> Bool {
        guard running != nil else { return false }
        quitAfterRun = true
        _ = MAME.requestExit()
        return true
    }

    // A window that leaves full screen takes the size for the last frame.
    func windowDidExitFullScreen(_ notification: Notification) {
        guard let window, let frame else { return }
        Self.fit(window, frame: frame, windowSize: windowSize)
    }

    // The window closes when MAME has exited.
    func windowShouldClose(_ sender: NSWindow) -> Bool {
        _ = MAME.requestExit()
        return false
    }
}

/// The machine window's Continue button, at the right of its title bar.
/// It shows only while a CPU is stopped and waits (the Next in MAME's
/// debugger, or the Pi), and continues them.
final class ContinueAccessory: NSTitlebarAccessoryViewController {
    init() {
        super.init(nibName: nil, bundle: nil)
        let button = NSButton(title: "Continue", target: nil, action: #selector(continueMachine))
        button.target = self
        button.image = NSImage(systemSymbolName: "play.fill", accessibilityDescription: nil)
        button.imagePosition = .imageLeading
        button.bezelStyle = .accessoryBarAction
        button.toolTip = "Continue the stopped Next or Pi"
        let holder = NSView(frame: NSRect(x: 0, y: 0, width: button.fittingSize.width + 12, height: button.fittingSize.height))
        button.frame.origin = NSPoint(x: 4, y: 0)
        button.frame.size = button.fittingSize
        holder.addSubview(button)
        view = holder
        layoutAttribute = .trailing
        isHidden = true
    }

    required init?(coder: NSCoder) {
        fatalError("init(coder:) is not used")
    }

    @objc private func continueMachine() {
        MAME.continueMachine()
    }
}

/// The view MAME draws on. Keys go to MAME; key combinations the menus
/// own reach the menus first. The mouse over the view goes to MAME,
/// which takes it with -mouse.
final class MachineView: NSView {
    override var acceptsFirstResponder: Bool { true }

    /// MAME's last pointer request: the pointer hidden, the pointer held.
    private var pointerHidden = false
    private var pointerCaptured = false

    /// Hides the pointer over the view, and holds it at the view's centre
    /// while the mouse's movement still comes as events, as MAME asks.
    func setPointer(hide: Bool, capture: Bool) {
        // NSCursor's hide and unhide calls must balance
        if hide != pointerHidden {
            if hide { NSCursor.hide() } else { NSCursor.unhide() }
            pointerHidden = hide
        }
        if capture != pointerCaptured {
            if capture, let window {
                // the pointer is at the view's centre when it comes back
                let centre = window.convertPoint(toScreen: convert(NSPoint(x: bounds.midX, y: bounds.midY), to: nil))
                let top = NSScreen.screens.first?.frame.maxY ?? 0
                CGWarpMouseCursorPosition(CGPoint(x: centre.x, y: top - centre.y))
            }
            CGAssociateMouseAndMouseCursorPosition(capture ? 0 : 1)
            pointerCaptured = capture
        }
    }

    override func updateTrackingAreas() {
        super.updateTrackingAreas()
        trackingAreas.forEach(removeTrackingArea)
        addTrackingArea(NSTrackingArea(rect: .zero,
                                       options: [.mouseEnteredAndExited, .mouseMoved, .activeInKeyWindow, .inVisibleRect],
                                       owner: self))
    }

    override func mouseExited(with event: NSEvent) {
        MAME.mouseInside(false)
    }

    /// The pointer is over the view when a move ends inside it; a drag
    /// that leaves the view still moves MAME's mouse.
    private func moved(_ event: NSEvent) {
        MAME.mouseInside(bounds.contains(convert(event.locationInWindow, from: nil)))
        MAME.mouseMove(dx: event.deltaX, dy: event.deltaY)
    }

    override func mouseMoved(with event: NSEvent) { moved(event) }
    override func mouseDragged(with event: NSEvent) { moved(event) }
    override func rightMouseDragged(with event: NSEvent) { moved(event) }
    override func otherMouseDragged(with event: NSEvent) { moved(event) }

    override func mouseDown(with event: NSEvent) { MAME.mouseButton(0, down: true) }
    override func mouseUp(with event: NSEvent) { MAME.mouseButton(0, down: false) }
    override func rightMouseDown(with event: NSEvent) { MAME.mouseButton(1, down: true) }
    override func rightMouseUp(with event: NSEvent) { MAME.mouseButton(1, down: false) }
    override func otherMouseDown(with event: NSEvent) { MAME.mouseButton(event.buttonNumber, down: true) }
    override func otherMouseUp(with event: NSEvent) { MAME.mouseButton(event.buttonNumber, down: false) }

    override func scrollWheel(with event: NSEvent) {
        // with natural scrolling the deltas are reversed from the wheel's turn
        MAME.mouseWheel(event.isDirectionInvertedFromDevice ? -event.deltaY : event.deltaY)
    }

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
