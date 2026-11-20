// The window a launch with no document to open shows when there are
// recent configurations: the recent .pinesy files, and a button to
// create a new machine. With no recent configuration that still exists,
// the launch opens a new configuration, which asks for the machine.

import AppKit
import SwiftUI

@MainActor
enum RecentConfigurations {
    private static var window: NSWindow?
    private static let handler = OpenApplicationHandler()

    /// The recent configurations, newest first, as AppKit's Open Recent
    /// menu lists them, without files that no longer exist.
    static var urls: [URL] {
        NSDocumentController.shared.recentDocumentURLs.filter {
            FileManager.default.fileExists(atPath: $0.path)
        }
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
                if !RecentConfigurations.show() {
                    NSDocumentController.shared.newDocument(nil)
                }
            }
        }
    }

    /// Shows the window. False when there is no recent configuration.
    static func show() -> Bool {
        let urls = urls
        guard !urls.isEmpty else { return false }
        let content = NSHostingController(rootView: RecentConfigurationsView(urls: urls))
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
        return true
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
    let urls: [URL]

    @State private var selection: URL?

    var body: some View {
        VStack(spacing: 0) {
            List(urls, id: \.self, selection: $selection) { url in
                RecentRow(url: url)
                    .tag(url)
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
            selection = urls.first
        }
    }
}

private struct RecentRow: View {
    let url: URL

    var body: some View {
        HStack {
            Image(nsImage: NSWorkspace.shared.icon(forFile: url.path))
                .resizable()
                .frame(width: 32, height: 32)
            VStack(alignment: .leading, spacing: 2) {
                Text(url.deletingPathExtension().lastPathComponent)
                Text(url.deletingLastPathComponent().path(percentEncoded: false))
                    .font(.caption)
                    .foregroundStyle(.secondary)
                    .lineLimit(1)
                    .truncationMode(.middle)
            }
        }
        .padding(.vertical, 2)
    }
}
