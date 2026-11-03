// Swift wrappers around the pinesy C interface (pinesy.h).

import AppKit
import CPinesy

enum MAME {
    /// Every driver in the library, as pinesy_list_machines() reports it.
    static func listMachines() -> [DriverInfo] {
        let text = TextReceiver()
        _ = pinesy_list_machines(TextReceiver.callback, text.context)
        return (try? JSONDecoder().decode([DriverInfo].self, from: Data(text.value.utf8))) ?? []
    }

    /// The machine a command line describes: its slots, ROMs and media,
    /// or MAME's error text. MAME reads the command line with the app's
    /// own paths, as for a run.
    static func describe(_ arguments: [String]) -> MachineDescription {
        let text = TextReceiver()
        withCStrings(MAMEPaths.options() + arguments) { argc, argv in
            _ = pinesy_describe(argc, argv, TextReceiver.callback, text.context)
        }
        if let description = try? JSONDecoder().decode(MachineDescription.self, from: Data(text.value.utf8)) {
            return description
        }
        return MachineDescription(errors: "The emulator gave no description of this configuration.")
    }

    /// Runs MAME on the main thread with a command line (no program name)
    /// and returns MAME's exit code. `view` shows the first screen; nil
    /// means MAME draws nowhere and no host is passed, as in headless mode.
    /// `host` receives MAME's error lines and the window shape it asks for.
    @MainActor
    static func run(_ arguments: [String], view: NSView?, host receiver: RunHost? = nil) -> Int32 {
        withCStrings(arguments) { argc, argv in
            guard let view else {
                return pinesy_run(argc, argv, nil)
            }
            var host = pinesy_host()
            host.view = Unmanaged.passUnretained(view).toOpaque()
            if let receiver {
                host.output = RunHost.outputCallback
                host.aspect = RunHost.aspectCallback
                host.size = RunHost.sizeCallback
                host.context = receiver.context
            }
            return withUnsafePointer(to: &host) { pinesy_run(argc, argv, $0) }
        }
    }

    /// Asks the running machine to exit; a machine still starting exits
    /// once it has started. Returns true if a machine is running.
    static func requestExit() -> Bool {
        pinesy_request_exit() != 0
    }

    static func key(_ keyCode: UInt16, down: Bool) {
        pinesy_key(Int32(keyCode), down ? 1 : 0)
    }

    static func character(_ scalar: UInt32) {
        pinesy_char(scalar)
    }

    private static func withCStrings<R>(_ strings: [String], _ body: (Int32, UnsafePointer<UnsafePointer<CChar>?>?) -> R) -> R {
        let cStrings = strings.map { strdup($0) }
        defer { cStrings.forEach { free($0) } }
        let argv: [UnsafePointer<CChar>?] = cStrings.map { UnsafePointer($0) }
        return argv.withUnsafeBufferPointer { body(Int32($0.count), $0.baseAddress) }
    }
}

/// Collects the JSON text a pinesy call passes to its callback.
private final class TextReceiver {
    var value = ""
    var context: UnsafeMutableRawPointer { Unmanaged.passUnretained(self).toOpaque() }

    static let callback: pinesy_text_fn = { context, text in
        guard let context, let text else { return }
        Unmanaged<TextReceiver>.fromOpaque(context).takeUnretainedValue().value = String(cString: text)
    }
}

/// What MAME tells the app during a run: the error lines it prints, and
/// the shape it wants for the machine window. MAME calls on the main thread.
@MainActor
final class RunHost {
    /// MAME's osd_output_channel values.
    enum Channel: Int32 {
        case error = 0, warning, info, debug, verbose, log
    }

    private(set) var errors: [String] = []
    /// The ratio to keep when the user resizes; nil for any shape.
    var onAspect: (NSSize?) -> Void = { _ in }
    /// The size MAME wants for the picture, in points.
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
