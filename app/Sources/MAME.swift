// Swift wrappers around the pinesy C interface (pinesy.h).

import AppKit
import CPinesy

enum MAME {
    struct Machine: Hashable, Identifiable {
        let name: String
        let description: String
        var id: String { name }
    }

    /// The machines built into the library.
    static let machines: [Machine] = (0..<pinesy_machine_count()).compactMap { index in
        guard let name = pinesy_machine_name(index),
              let description = pinesy_machine_description(index) else { return nil }
        return Machine(name: String(cString: name), description: String(cString: description))
    }

    /// Runs MAME on the main thread with a command line (no program name)
    /// and returns MAME's exit code. `view` shows the first screen; nil
    /// means MAME draws nowhere.
    @MainActor
    static func run(_ arguments: [String], view: NSView?) -> Int32 {
        let cStrings = arguments.map { strdup($0) }
        defer { cStrings.forEach { free($0) } }
        let argv: [UnsafePointer<CChar>?] = cStrings.map { UnsafePointer($0) }

        var host = pinesy_host()
        if let view {
            host.view = Unmanaged.passUnretained(view).toOpaque()
        }
        return argv.withUnsafeBufferPointer { buffer in
            withUnsafePointer(to: &host) { hostPointer in
                pinesy_run(Int32(buffer.count), buffer.baseAddress, view == nil ? nil : hostPointer)
            }
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
}
