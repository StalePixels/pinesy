// General settings: extra ROM folders, and the folder where MAME keeps
// its files.

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Dialogs
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import Pinesy

Kirigami.ScrollablePage {
    id: page

    title: i18nc("@title settings page", "General")

    Kirigami.FormLayout {
        Item {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: i18nc("@title:group", "ROM folders")
        }

        ColumnLayout {
            Kirigami.FormData.label: i18nc("@label", "Extra folders:")
            Layout.fillWidth: true

            QQC2.Frame {
                Layout.fillWidth: true
                Layout.preferredWidth: Kirigami.Units.gridUnit * 20
                Layout.preferredHeight: Kirigami.Units.gridUnit * 5
                padding: 1
                ListView {
                    id: folderList
                    anchors.fill: parent
                    clip: true
                    model: Core.romFolders
                    currentIndex: -1
                    delegate: QQC2.ItemDelegate {
                        required property string modelData
                        required property int index
                        width: ListView.view.width
                        text: modelData
                        highlighted: ListView.isCurrentItem
                        onClicked: folderList.currentIndex = index
                    }
                }
            }
            RowLayout {
                QQC2.Button {
                    text: i18nc("@action:button", "Add Folder...")
                    icon.name: "list-add"
                    onClicked: romFolderDialog.open()
                }
                QQC2.Button {
                    text: i18nc("@action:button", "Remove")
                    icon.name: "list-remove"
                    enabled: folderList.currentIndex >= 0
                    onClicked: {
                        Core.removeRomFolder(Core.romFolders[folderList.currentIndex])
                        folderList.currentIndex = -1
                    }
                }
            }
            QQC2.Label {
                Layout.fillWidth: true
                Layout.maximumWidth: Kirigami.Units.gridUnit * 24
                wrapMode: Text.Wrap
                color: Kirigami.Theme.disabledTextColor
                font: Kirigami.Theme.smallFont
                text: i18n("Pi/Ne-Sy looks for machine ROMs in the ROM sets it comes with, then in %1, then in these folders.",
                           Core.userRomFolder)
            }
        }

        Item {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: i18nc("@title:group", "Machine settings and snapshots")
        }

        ColumnLayout {
            Kirigami.FormData.label: i18nc("@label", "Folder:")
            Layout.fillWidth: true
            QQC2.Label {
                Layout.maximumWidth: Kirigami.Units.gridUnit * 24
                elide: Text.ElideMiddle
                text: Core.dataFolder
            }
            RowLayout {
                QQC2.Button {
                    text: i18nc("@action:button", "Choose...")
                    icon.name: "document-open-folder"
                    onClicked: dataFolderDialog.open()
                }
                QQC2.Button {
                    visible: Core.dataFolderSetting !== ""
                    text: i18nc("@action:button", "Use Default")
                    onClicked: Core.setDataFolder("")
                }
            }
            QQC2.Label {
                Layout.fillWidth: true
                Layout.maximumWidth: Kirigami.Units.gridUnit * 24
                wrapMode: Text.Wrap
                color: Kirigami.Theme.disabledTextColor
                font: Kirigami.Theme.smallFont
                text: i18n("MAME keeps each machine's settings, memory contents and snapshots in this folder.")
            }
        }
    }

    FolderDialog {
        id: romFolderDialog
        title: i18nc("@title:window", "Add a ROM Folder")
        onAccepted: {
            const path = Core.localPath(selectedFolder)
            if (!Core.addRomFolder(path)) {
                rejectedDialog.subtitle = i18n("The emulator cannot use a folder with \";\" in its path: %1", path)
                rejectedDialog.open()
            }
        }
    }

    FolderDialog {
        id: dataFolderDialog
        title: i18nc("@title:window", "Choose the Data Folder")
        onAccepted: Core.setDataFolder(Core.localPath(selectedFolder))
    }

    Kirigami.PromptDialog {
        id: rejectedDialog
        title: i18nc("@title:dialog", "This folder cannot be a ROM folder")
        standardButtons: Kirigami.Dialog.Ok
    }
}
