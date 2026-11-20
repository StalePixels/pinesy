// Entry point. With arguments, Pinesy runs MAME headless with that
// command line and exits with MAME's exit code; the core drops a
// leading "--". Without arguments, it starts the app.

import AppKit

let arguments = Array(CommandLine.arguments.dropFirst())

if arguments.isEmpty {
    RecentConfigurations.takeOpenApplicationEvent()
    PinesyApp.main()
} else {
    exit(Headless.run(arguments))
}
