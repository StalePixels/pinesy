// A configuration's window: its menus, its page, and its dialogs.

import QtCore
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Dialogs
import org.kde.kirigami as Kirigami
import Pinesy

Kirigami.ApplicationWindow {
    id: window

    required property var app
    property url fileToOpen
    property bool closeConfirmed: false

    readonly property ConfigurationDocument document: ConfigurationDocument {
        onRunFailed: (title, message) => {
            runFailedDialog.title = i18nc("@title:dialog", "%1 stopped with an error", title)
            runFailedDialog.subtitle = message
            runFailedDialog.open()
        }
    }
    readonly property bool running: MachineRunner.running !== ""

    title: document.title + (document.modified ? " *" : "")
    width: Kirigami.Units.gridUnit * 42
    height: Kirigami.Units.gridUnit * 40
    minimumWidth: Kirigami.Units.gridUnit * 26
    minimumHeight: Kirigami.Units.gridUnit * 20

    pageStack.initialPage: ConfigurationPage {
        document: window.document
        onChooseMachine: picker.open()
        onRun: debug => window.document.run(debug)
        onChooseMedia: (name, extensions) => {
            mediaDialog.media = name
            mediaDialog.nameFilters = [
                i18nc("@item:inlistbox file type", "Media files (%1)", extensions.map(e => "*." + e).join(" ")),
                i18nc("@item:inlistbox file type", "All files (*)")
            ]
            mediaDialog.open()
        }
    }

    // Actions shared by the menus and the page
    readonly property Kirigami.Action runAction: Kirigami.Action {
        text: i18nc("@action", "Run")
        icon.name: "media-playback-start"
        shortcut: "Ctrl+R"
        enabled: window.document.canRun
        onTriggered: window.document.run(false)
    }
    readonly property Kirigami.Action debugAction: Kirigami.Action {
        text: i18nc("@action", "Run with Debugger")
        icon.name: "debug-run"
        shortcut: "Ctrl+Shift+R"
        enabled: window.document.canRun
        onTriggered: window.document.run(true)
    }

    menuBar: QQC2.MenuBar {
        QQC2.Menu {
            title: i18nc("@title:menu", "File")
            QQC2.Action {
                text: i18nc("@action", "New...")
                icon.name: "document-new"
                shortcut: StandardKey.New
                onTriggered: window.app.newDocument()
            }
            QQC2.Action {
                text: i18nc("@action", "Open...")
                icon.name: "document-open"
                shortcut: StandardKey.Open
                onTriggered: openDialog.open()
            }
            QQC2.MenuSeparator {}
            QQC2.Action {
                text: i18nc("@action", "Save")
                icon.name: "document-save"
                shortcut: StandardKey.Save
                enabled: !window.document.empty
                onTriggered: window.save()
            }
            QQC2.Action {
                text: i18nc("@action", "Save As...")
                icon.name: "document-save-as"
                shortcut: StandardKey.SaveAs
                enabled: !window.document.empty
                onTriggered: window.saveAs()
            }
            QQC2.MenuSeparator {}
            QQC2.Action {
                text: i18nc("@action", "Close")
                icon.name: "document-close"
                shortcut: StandardKey.Close
                onTriggered: window.close()
            }
            QQC2.Action {
                text: i18nc("@action", "Quit")
                icon.name: "application-exit"
                shortcut: StandardKey.Quit
                onTriggered: window.app.quit()
            }
        }
        QQC2.Menu {
            title: i18nc("@title:menu", "Machine")
            QQC2.MenuItem { action: window.runAction }
            QQC2.MenuItem { action: window.debugAction }
            QQC2.Action {
                text: i18nc("@action", "Stop")
                icon.name: "media-playback-stop"
                shortcut: "Ctrl+."
                enabled: window.running
                onTriggered: MachineRunner.stop()
            }
            QQC2.Action {
                text: i18nc("@action", "Reset")
                icon.name: "view-refresh"
                enabled: window.running
                onTriggered: MachineRunner.reset(false)
            }
            QQC2.Action {
                text: i18nc("@action", "Hard Reset")
                icon.name: "system-reboot"
                enabled: window.running
                onTriggered: MachineRunner.reset(true)
            }
            QQC2.Action {
                text: i18nc("@action", "Clear NVRAM…")
                icon.name: "edit-clear"
                // MAME writes NVRAM when a machine exits, so it is not cleared while one runs
                enabled: !window.running && !window.document.empty
                onTriggered: clearNvramDialog.open()
            }
            QQC2.MenuSeparator {}
            QQC2.Action {
                text: i18nc("@action", "Copy Command Line")
                icon.name: "edit-copy"
                enabled: !window.document.empty
                onTriggered: window.document.copyCommandLine()
            }
        }
        QQC2.Menu {
            title: i18nc("@title:menu", "Settings")
            QQC2.Action {
                text: i18nc("@action", "Configure Pi/Ne-Sy...")
                icon.name: "configure"
                shortcut: StandardKey.Preferences
                onTriggered: window.app.showSettings()
            }
        }
    }

    function save() {
        if (document.url.toString() === "")
            saveAs()
        else
            report(document.save(document.url))
    }

    function saveAs() {
        let folder = saveDialog.currentFolder.toString()
        if (folder === "")
            folder = StandardPaths.writableLocation(StandardPaths.DocumentsLocation).toString()
        saveDialog.selectedFile = folder + "/" + document.title + ".pinesy"
        saveDialog.open()
    }

    function report(error) {
        if (error !== "") {
            errorDialog.subtitle = error
            errorDialog.open()
        }
    }

    // Configurations with unsaved changes ask before they close.
    onClosing: close => {
        if (document.modified && !closeConfirmed) {
            close.accepted = false
            closeDialog.open()
        }
    }

    Component.onCompleted: {
        if (fileToOpen.toString() !== "") {
            report(document.load(fileToOpen))
            if (document.empty)
                Qt.callLater(close)
        } else {
            Qt.callLater(picker.open)
        }
    }

    MachinePicker {
        id: picker
        canChoose: !window.running
        onChosen: machine => window.document.choose(machine)
        // A new configuration with no machine closes with its picker
        onCancelled: {
            if (window.document.empty)
                window.close()
        }
    }

    FileDialog {
        id: openDialog
        title: i18nc("@title:window", "Open Configuration")
        fileMode: FileDialog.OpenFile
        nameFilters: [i18nc("@item:inlistbox file type", "Pinesy configurations (*.pinesy)")]
        onAccepted: {
            // An untouched new configuration gives way to the file
            if (window.document.empty && !window.running) {
                window.report(window.document.load(selectedFile))
                picker.close()
            } else {
                window.app.openDocument(selectedFile)
            }
        }
    }

    FileDialog {
        id: saveDialog
        title: i18nc("@title:window", "Save Configuration")
        fileMode: FileDialog.SaveFile
        defaultSuffix: "pinesy"
        nameFilters: [i18nc("@item:inlistbox file type", "Pinesy configurations (*.pinesy)")]
        onAccepted: window.report(window.document.save(selectedFile))
    }

    FileDialog {
        id: mediaDialog
        property string media
        title: i18nc("@title:window", "Choose a File")
        fileMode: FileDialog.OpenFile
        onAccepted: window.report(window.document.setMedia(media, Core.localPath(selectedFile)))
    }

    Kirigami.PromptDialog {
        id: runFailedDialog
        standardButtons: Kirigami.Dialog.Ok
    }

    Kirigami.PromptDialog {
        id: errorDialog
        title: i18nc("@title:dialog", "The configuration could not be read or saved")
        standardButtons: Kirigami.Dialog.Ok
    }

    Kirigami.PromptDialog {
        id: clearNvramDialog
        title: i18nc("@title:dialog", "Clear the NVRAM of \"%1\"?", window.document.title)
        subtitle: i18n("The NVRAM that MAME saved for this machine is deleted. You cannot undo this.")
        standardButtons: Kirigami.Dialog.Cancel
        customFooterActions: [
            Kirigami.Action {
                text: i18nc("@action:button", "Clear NVRAM")
                icon.name: "edit-clear"
                onTriggered: {
                    clearNvramDialog.close()
                    if (!window.running && !window.document.clearNvram())
                        nvramFailedDialog.open()
                }
            }
        ]
    }

    Kirigami.PromptDialog {
        id: nvramFailedDialog
        title: i18nc("@title:dialog", "The NVRAM could not be cleared")
        subtitle: i18n("A file in the NVRAM folder could not be deleted.")
        standardButtons: Kirigami.Dialog.Ok
    }

    Kirigami.PromptDialog {
        id: closeDialog
        title: i18nc("@title:dialog", "Save changes to \"%1\"?", window.document.title)
        subtitle: i18n("The changes are lost if the configuration is closed without saving.")
        standardButtons: Kirigami.Dialog.Save | Kirigami.Dialog.Discard | Kirigami.Dialog.Cancel
        onAccepted: {
            window.save()
            if (!window.document.modified) {
                window.closeConfirmed = true
                window.close()
            }
        }
        onDiscarded: {
            window.closeConfirmed = true
            window.close()
        }
    }
}
