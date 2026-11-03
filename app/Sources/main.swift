// Entry point. With arguments, Pinesy runs MAME headless with that
// command line and exits with MAME's exit code. A leading "--" is
// dropped. Without arguments, it starts the app.

import AppKit

let arguments = Array(CommandLine.arguments.dropFirst())

if arguments.isEmpty {
    PinesyApp.main()
} else {
    let mameArguments = arguments.first == "--" ? Array(arguments.dropFirst()) : arguments
    exit(Headless.run(mameArguments))
}
