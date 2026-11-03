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
    /// or MAME's error text.
    static func describe(_ arguments: [String]) -> MachineDescription {
        let text = TextReceiver()
        withCStrings(arguments) { argc, argv in
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
    /// `output` receives each line MAME prints, with its channel.
    @MainActor
    static func run(_ arguments: [String], view: NSView?, output: OutputReceiver? = nil) -> Int32 {
        withCStrings(arguments) { argc, argv in
            guard let view else {
                return pinesy_run(argc, argv, nil)
            }
            var host = pinesy_host()
            host.view = Unmanaged.passUnretained(view).toOpaque()
            if let output {
                host.output = OutputReceiver.callback
                host.context = output.context
            }
            return withUnsafePointer(to: &host) { pinesy_run(argc, argv, $0) }
        }
    }

    /// Asks the running machine to exit. Returns true if a machine is running.
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

/// Collects the lines MAME prints during a run.
final class OutputReceiver {
    /// MAME's osd_output_channel values.
    enum Channel: Int32 {
        case error = 0, warning, info, debug, verbose, log
    }

    private(set) var errors: [String] = []

    var context: UnsafeMutableRawPointer { Unmanaged.passUnretained(self).toOpaque() }

    static let callback: pinesy_output_fn = { context, channel, text in
        guard let context, let text, Channel(rawValue: channel) == .error else { return }
        let receiver = Unmanaged<OutputReceiver>.fromOpaque(context).takeUnretainedValue()
        receiver.errors.append(String(cString: text))
    }
}
