// Updates of a CI build, with Sparkle. At startup, when the core says
// this build looks for updates (Core.updateChecks), Sparkle reads the
// appcast of the build's track. When it finds a newer build, the app
// shows the core's out-of-date notice with its three choices. Update
// hands the build back to Sparkle, which downloads, checks and
// installs it with its own windows, and starts the new app.
// A local build has no appcast, and Sparkle never starts.

import AppKit
import Sparkle

@MainActor
final class Updates {
    static let shared = Updates()

    private var updater: SPUUpdater?
    private var driver: UpdateDriver?
    private var delegate: UpdaterDelegate?

    func start() {
        guard updater == nil, Core.updateChecks, let feed = Core.appcastURL else { return }
        let driver = UpdateDriver(hostBundle: .main)
        let delegate = UpdaterDelegate(feed: feed)
        let updater = SPUUpdater(hostBundle: .main, applicationBundle: .main, userDriver: driver, delegate: delegate)
        do {
            try updater.start()
        } catch {
            NSLog("Pinesy: Sparkle did not start: %@", error.localizedDescription)
            return
        }
        // The app checks once, at startup; Sparkle schedules nothing.
        updater.automaticallyChecksForUpdates = false
        updater.automaticallyDownloadsUpdates = false
        self.updater = updater
        self.driver = driver
        self.delegate = delegate
        updater.checkForUpdatesInBackground()
    }
}

/// Gives Sparkle the appcast of the build's track.
private final class UpdaterDelegate: NSObject, SPUUpdaterDelegate {
    let feed: String

    init(feed: String) {
        self.feed = feed
    }

    func feedURLString(for updater: SPUUpdater) -> String? {
        feed
    }
}

/// Sparkle's own windows, except for the notice that a newer build
/// exists: that is the core's notice, the same as in the GNOME and KDE
/// apps.
private final class UpdateDriver: NSObject, SPUUserDriver {
    private let standard: SPUStandardUserDriver

    init(hostBundle: Bundle) {
        standard = SPUStandardUserDriver(hostBundle: hostBundle, delegate: nil)
    }

    func showUpdateFound(with appcastItem: SUAppcastItem, state: SPUUserUpdateState,
                         reply: @escaping (SPUUserUpdateChoice) -> Void) {
        let version = appcastItem.versionString
        guard Core.updateNotice(version), let texts = Core.updateTexts(version) else {
            reply(.dismiss)
            return
        }
        let alert = NSAlert()
        alert.messageText = texts.title
        alert.informativeText = texts.message
        alert.addButton(withTitle: texts.update)
        alert.addButton(withTitle: texts.ignoreBuild)
        alert.addButton(withTitle: texts.neverRemind)
        NSApp.activate()
        switch alert.runModal() {
        case .alertFirstButtonReturn:
            reply(.install)
        case .alertSecondButtonReturn:
            Core.updateChoice(.ignoreBuild, version: version)
            reply(.dismiss)
        default:
            Core.updateChoice(.neverRemind, version: version)
            reply(.dismiss)
        }
    }

    func show(_ request: SPUUpdatePermissionRequest, reply: @escaping (SUUpdatePermissionResponse) -> Void) {
        standard.show(request, reply: reply)
    }

    func showUserInitiatedUpdateCheck(cancellation: @escaping () -> Void) {
        standard.showUserInitiatedUpdateCheck(cancellation: cancellation)
    }

    func showUpdateReleaseNotes(with downloadData: SPUDownloadData) {
        standard.showUpdateReleaseNotes(with: downloadData)
    }

    func showUpdateReleaseNotesFailedToDownloadWithError(_ error: any Error) {
        standard.showUpdateReleaseNotesFailedToDownloadWithError(error)
    }

    func showUpdateNotFoundWithError(_ error: any Error, acknowledgement: @escaping () -> Void) {
        // A check at startup that finds nothing newer says nothing.
        acknowledgement()
    }

    func showUpdaterError(_ error: any Error, acknowledgement: @escaping () -> Void) {
        standard.showUpdaterError(error, acknowledgement: acknowledgement)
    }

    func showDownloadInitiated(cancellation: @escaping () -> Void) {
        standard.showDownloadInitiated(cancellation: cancellation)
    }

    func showDownloadDidReceiveExpectedContentLength(_ expectedContentLength: UInt64) {
        standard.showDownloadDidReceiveExpectedContentLength(expectedContentLength)
    }

    func showDownloadDidReceiveData(ofLength length: UInt64) {
        standard.showDownloadDidReceiveData(ofLength: length)
    }

    func showDownloadDidStartExtractingUpdate() {
        standard.showDownloadDidStartExtractingUpdate()
    }

    func showExtractionReceivedProgress(_ progress: Double) {
        standard.showExtractionReceivedProgress(progress)
    }

    func showReady(toInstallAndRelaunch reply: @escaping (SPUUserUpdateChoice) -> Void) {
        // The user chose Update: install and start the new build. Sparkle
        // asks the app to quit, and AppKit does not close a document
        // window that has a sheet, such as the machine picker, so the
        // sheets end first. A document with unsaved changes still asks.
        for window in NSApp.windows {
            if let sheet = window.attachedSheet {
                window.endSheet(sheet)
            }
        }
        reply(.install)
    }

    func showInstallingUpdate(withApplicationTerminated applicationTerminated: Bool,
                              retryTerminatingApplication: @escaping () -> Void) {
        standard.showInstallingUpdate(withApplicationTerminated: applicationTerminated,
                                      retryTerminatingApplication: retryTerminatingApplication)
    }

    func showUpdateInstalledAndRelaunched(_ relaunched: Bool, acknowledgement: @escaping () -> Void) {
        standard.showUpdateInstalledAndRelaunched(relaunched, acknowledgement: acknowledgement)
    }

    func showUpdateInFocus() {
        standard.showUpdateInFocus()
    }

    func dismissUpdateInstallation() {
        standard.dismissUpdateInstallation()
    }
}

/// The texts of the notice, from the core (pinesy_core_update_texts()).
struct UpdateTexts: Decodable {
    let title: String
    let message: String
    let neverRemind: String
    let ignoreBuild: String
    let update: String
    let packageManager: String
    let newPermissions: String
    let failed: String
}
