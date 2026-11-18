// Swift wrappers around the engine's C interface (pinesy.h): a run's
// host, and the calls into a running machine. Runs start through the
// core (Core.swift).

import AppKit
import CPinesy

enum MAME {
    /// Calls body with the host a run gives MAME. `view` shows the first
    /// screen; nil means MAME draws nowhere and no host is passed, as in
    /// headless mode. `host` receives MAME's error lines and the frame's
    /// size.
    @MainActor
    static func withHost<R>(view: NSView?, receiver: RunHost? = nil, _ body: (UnsafePointer<pinesy_host>?) -> R) -> R {
        guard let view else {
            return body(nil)
        }
        var host = pinesy_host()
        host.view = Unmanaged.passUnretained(view).toOpaque()
        if let receiver {
            host.output = RunHost.outputCallback
            host.aspect = RunHost.aspectCallback
            host.size = RunHost.sizeCallback
            host.context = receiver.context
        }
        return withUnsafePointer(to: &host) { body($0) }
    }

    /// Asks the running machine to exit; a machine still starting exits
    /// once it has started. Returns true if a machine is running.
    static func requestExit() -> Bool {
        pinesy_request_exit() != 0
    }

    /// Resets the running machine: soft keeps RAM, hard starts again
    /// from power-on. Returns true if a machine took the request.
    static func reset(hard: Bool) -> Bool {
        pinesy_reset(hard ? 1 : 0) != 0
    }

    static func key(_ keyCode: UInt16, down: Bool) {
        pinesy_key(Int32(keyCode), down ? 1 : 0)
    }

    static func character(_ scalar: UInt32) {
        pinesy_char(scalar)
    }
}

/// What MAME tells the app during a run: the error lines it prints, and
/// the frame's size, which sets the machine window's shape and size. MAME calls on the main thread.
@MainActor
final class RunHost {
    /// MAME's osd_output_channel values.
    enum Channel: Int32 {
        case error = 0, warning, info, debug, verbose, log
    }

    private(set) var errors: [String] = []
    /// The ratio to keep when the user resizes; nil for any shape.
    var onAspect: (NSSize?) -> Void = { _ in }
    /// The frame's size, in pixels, with the first frame and when it changes.
    var onSize: (NSSize) -> Void = { _ in }

    nonisolated var context: UnsafeMutableRawPointer { Unmanaged.passUnretained(self).toOpaque() }

    nonisolated private static func host(_ context: UnsafeMutableRawPointer) -> RunHost {
        Unmanaged<RunHost>.fromOpaque(context).takeUnretainedValue()
    }

    nonisolated static let outputCallback: pinesy_output_fn = { context, channel, text in
        guard let context, let text, Channel(rawValue: channel) == .error else { return }
        let line = String(cString: text)
        let receiver = host(context)
        MainActor.assumeIsolated { receiver.errors.append(line) }
    }

    nonisolated static let aspectCallback: pinesy_size_fn = { context, width, height in
        guard let context else { return }
        let ratio = (width > 0 && height > 0) ? NSSize(width: Int(width), height: Int(height)) : nil
        let receiver = host(context)
        MainActor.assumeIsolated { receiver.onAspect(ratio) }
    }

    nonisolated static let sizeCallback: pinesy_size_fn = { context, width, height in
        guard let context, width > 0, height > 0 else { return }
        let receiver = host(context)
        let size = NSSize(width: Int(width), height: Int(height))
        MainActor.assumeIsolated { receiver.onSize(size) }
    }
}
