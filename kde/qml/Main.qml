// The app: one window per configuration, and the Settings window.

import QtQuick
import Pinesy

QtObject {
    id: app

    property var windows: []
    property var settingsWindow: null
    // the recent configurations window, while it is open
    property var recentWindow: null

    property Component documentWindow: Component {
        DocumentWindow {}
    }

    property Component settingsComponent: Component {
        SettingsWindow {}
    }

    property Component recentComponent: Component {
        RecentWindow {}
    }

    // A launch with no file to open: the recent configurations, or a
    // new configuration when there is none (the core's rule).
    function launch() {
        if (RecentConfigurations.launchShowsRecent()) {
            recentWindow = recentComponent.createObject(null, { app: app, entries: RecentConfigurations.entries })
            recentWindow.closing.connect(() => {
                const closed = recentWindow
                recentWindow = null
                Qt.callLater(() => closed.destroy())
            })
            recentWindow.show()
        } else {
            newDocument()
        }
    }

    // The recent configurations window gives way to a configuration.
    function closeRecent() {
        if (recentWindow)
            recentWindow.close()
    }

    // A new configuration, which starts with the machine picker.
    function newDocument() {
        const window = addWindow(null)
        closeRecent()
        return window
    }

    // A recent configuration, from the recent configurations window or
    // File > Open Recent: as a file the desktop hands over.
    function openRecent(url) {
        openFromDesktop(url)
    }

    // A window for a .pinesy file. A file already open comes to the front.
    function openDocument(url) {
        for (const window of windows) {
            if (window.document.url.toString() === url.toString()) {
                window.raise()
                window.requestActivate()
                closeRecent()
                return
            }
        }
        openWindow(url)
    }

    // A window for a file; the recent configurations window closes when
    // the file opens.
    function openWindow(url) {
        const window = addWindow(url)
        if (!window.document.empty)
            closeRecent()
    }

    // A file the desktop hands over, as from a file manager. A file
    // already open comes to the front; an untouched new configuration
    // takes the file; otherwise it opens in a new window.
    function openFromDesktop(url) {
        for (const window of windows) {
            if (window.document.url.toString() === url.toString()) {
                window.raise()
                window.requestActivate()
                closeRecent()
                return
            }
        }
        for (const window of windows) {
            if (window.takeFile(url))
                return
        }
        openWindow(url)
    }

    // The desktop asks for the app: the newest window comes to the
    // front. With none, it is as a launch with no file to open.
    function activate() {
        if (recentWindow) {
            recentWindow.raise()
            recentWindow.requestActivate()
            return
        }
        if (windows.length === 0) {
            launch()
            return
        }
        const window = windows[windows.length - 1]
        window.raise()
        window.requestActivate()
    }

    function addWindow(url) {
        const window = documentWindow.createObject(null, { app: app, fileToOpen: url ?? "" })
        windows = windows.concat([window])
        window.closing.connect(close => {
            if (close.accepted)
                windows = windows.filter(w => w !== window)
        })
        window.show()
        return window
    }

    function showSettings() {
        if (!settingsWindow)
            settingsWindow = settingsComponent.createObject(null)
        settingsWindow.show()
        settingsWindow.raise()
        settingsWindow.requestActivate()
    }

    // Quits once every window agrees to close. A running machine is
    // stopped first; the app quits when MAME has exited.
    function quit() {
        for (const window of windows.slice()) {
            if (!window.close())
                return
        }
        closeRecent()
        if (!MachineRunner.quitWhenStopped())
            Qt.quit()
    }

    Component.onCompleted: launch()
}
