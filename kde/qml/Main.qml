// The app: one window per configuration, and the Settings window.

import QtQuick
import Pinesy

QtObject {
    id: app

    property var windows: []
    property var settingsWindow: null

    property Component documentWindow: Component {
        DocumentWindow {}
    }

    property Component settingsComponent: Component {
        SettingsWindow {}
    }

    // A new configuration, which starts with the machine picker.
    function newDocument() {
        return addWindow(null)
    }

    // A window for a .pinesy file. A file already open comes to the front.
    function openDocument(url) {
        for (const window of windows) {
            if (window.document.url.toString() === url.toString()) {
                window.raise()
                window.requestActivate()
                return
            }
        }
        addWindow(url)
    }

    // A file the desktop hands over, as from a file manager. A file
    // already open comes to the front; an untouched new configuration
    // takes the file; otherwise it opens in a new window.
    function openFromDesktop(url) {
        for (const window of windows) {
            if (window.document.url.toString() === url.toString()) {
                window.raise()
                window.requestActivate()
                return
            }
        }
        for (const window of windows) {
            if (window.takeFile(url))
                return
        }
        addWindow(url)
    }

    // The desktop asks for the app: the newest window comes to the
    // front, or a new configuration opens when there is none.
    function activate() {
        if (windows.length === 0) {
            newDocument()
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
        if (!MachineRunner.quitWhenStopped())
            Qt.quit()
    }

    Component.onCompleted: newDocument()
}
