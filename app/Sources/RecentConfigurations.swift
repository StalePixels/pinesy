// The window a launch with no document to open shows when there are
// recent configurations: the recent .pinesy files, and a button to
// create a new machine. With no recent configuration that still exists,
// the launch opens a new configuration, which asks for the machine.
// The core decides which files show and what the launch opens
// (pinesy_core_recent_configurations(), pinesy_core_launch()); the list
// is AppKit's, the one File > Open Recent shows.

import AppKit
import SwiftUI

@MainActor
enum RecentConfigurations {
    private static var window: NSWindow?
    private static let handler = OpenApplicationHandler()

    /// The recent configurations to show, from AppKit's Open Recent
    /// list, newest first.
    static var entries: [Core.RecentEntry] {
        Core.recentConfigurations(NSDocumentController.shared.recentDocumentURLs.map { $0.path(percentEncoded: false) })
    }

    /// A launch with no document to open gets the Open Application Apple
    /// event, and AppKit's handler for it opens a new document. SwiftUI
    /// does not pass applicationShouldOpenUntitledFile(_:) on to the app's
    /// delegate, so Pinesy takes the event itself. AppKit installs its
    /// handlers before it posts willFinishLaunchingNotification.
    static func takeOpenApplicationEvent() {
        _ = NotificationCenter.default.addObserver(forName: NSApplication.willFinishLaunchingNotification,
                                                   object: nil, queue: nil) { _ in
            MainActor.assumeIsolated {
                NSAppleEventManager.shared().setEventHandler(handler,
                                                             andSelector: #selector(OpenApplicationHandler.openApplication(_:reply:)),
                                                             forEventClass: AEEventClass(kCoreEventClass),
                                                             andEventID: AEEventID(kAEOpenApplication))
            }
        }
    }

    final class OpenApplicationHandler: NSObject {
        @objc func openApplication(_ event: NSAppleEventDescriptor, reply: NSAppleEventDescriptor) {
            MainActor.assumeIsolated {
                RecentConfigurations.launch()
            }
        }
    }

    /// A launch with no document to open: the recent configurations, or
    /// a new configuration when there is none.
    static func launch() {
        let entries = entries
        switch Core.launch(filesToOpen: false, recentCount: entries.count) {
        case .recent: show(entries)
        default: NSDocumentController.shared.newDocument(nil)
        }
    }

    static func show(_ entries: [Core.RecentEntry]) {
        let content = NSHostingController(rootView: RecentConfigurationsView(entries: entries))
        if let window {
            window.contentViewController = content
        } else {
            let window = NSWindow(contentViewController: content)
            window.title = "Recent Configurations"
            window.styleMask = [.titled, .closable]
            window.isReleasedWhenClosed = false
            window.setContentSize(content.view.fittingSize)
            window.center()
            self.window = window
        }
        window?.makeKeyAndOrderFront(nil)
    }

    static func open(_ url: URL) {
        window?.close()
        NSDocumentController.shared.openDocument(withContentsOf: url, display: true) { _, _, error in
            if let error {
                NSApp.presentError(error)
            }
        }
    }

    static func newMachine() {
        window?.close()
        NSDocumentController.shared.newDocument(nil)
    }
}

private struct RecentConfigurationsView: View {
    let entries: [Core.RecentEntry]

    @State private var selection: URL?

    var body: some View {
        VStack(spacing: 0) {
            List(entries, id: \.self, selection: $selection) { entry in
                RecentRow(entry: entry)
                    .tag(URL(filePath: entry.path))
            }
            .contextMenu(forSelectionType: URL.self, menu: { _ in EmptyView() }) { urls in
                if let url = urls.first {
                    RecentConfigurations.open(url)
                }
            }

            Divider()

            HStack {
                Button("Create a New Machine…") { RecentConfigurations.newMachine() }
                Spacer()
                Button("Open") {
                    if let selection { RecentConfigurations.open(selection) }
                }
                .keyboardShortcut(.defaultAction)
                .disabled(selection == nil)
            }
            .padding()
        }
        .frame(width: 560, height: 400)
        .onAppear {
            selection = entries.first.map { URL(filePath: $0.path) }
        }
    }
}

private struct RecentRow: View {
    let entry: Core.RecentEntry

    var body: some View {
        HStack {
            Image(nsImage: NSWorkspace.shared.icon(forFile: entry.path))
                .resizable()
                .frame(width: 32, height: 32)
            VStack(alignment: .leading, spacing: 2) {
                Text(entry.name)
                Text(entry.folder)
                    .font(.caption)
                    .foregroundStyle(.secondary)
                    .lineLimit(1)
                    .truncationMode(.middle)
            }
        }
        .padding(.vertical, 2)
    }
}
