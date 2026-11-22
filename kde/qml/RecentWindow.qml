// The window a launch with no file to open shows when there are recent
// configurations: the recent .pinesy files, Open, and Create a New
// Machine…. With no recent configuration, the launch opens a new
// configuration instead (Main.qml).

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import Pinesy

Kirigami.ApplicationWindow {
    id: window

    required property var app
    // The entries as the launch found them.
    required property var entries
    readonly property var selectedEntry: list.currentIndex >= 0 && list.currentIndex < entries.length
                                         ? entries[list.currentIndex] : null

    title: i18nc("@title:window", "Recent Configurations")
    width: Kirigami.Units.gridUnit * 30
    height: Kirigami.Units.gridUnit * 22
    minimumWidth: Kirigami.Units.gridUnit * 20
    minimumHeight: Kirigami.Units.gridUnit * 14
    pageStack.globalToolBar.style: Kirigami.ApplicationHeaderStyle.None

    function openSelected() {
        if (selectedEntry)
            app.openRecent(selectedEntry.url)
    }

    Component.onCompleted: list.forceActiveFocus()

    pageStack.initialPage: Kirigami.ScrollablePage {
        padding: 0
        globalToolBarStyle: Kirigami.ApplicationHeaderStyle.None

        ListView {
            id: list
            model: window.entries
            currentIndex: 0
            keyNavigationEnabled: true
            focus: true
            Keys.onReturnPressed: window.openSelected()
            Keys.onEnterPressed: window.openSelected()

            delegate: QQC2.ItemDelegate {
                id: row
                required property var modelData
                required property int index
                width: ListView.view.width
                highlighted: ListView.isCurrentItem
                onClicked: list.currentIndex = index
                onDoubleClicked: {
                    list.currentIndex = index
                    window.openSelected()
                }
                QQC2.ToolTip.visible: hovered
                QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
                QQC2.ToolTip.text: modelData.path

                contentItem: RowLayout {
                    spacing: Kirigami.Units.largeSpacing
                    Kirigami.Icon {
                        source: "com.stalepixels.pinesy.kde"
                        Layout.preferredWidth: Kirigami.Units.iconSizes.medium
                        Layout.preferredHeight: Kirigami.Units.iconSizes.medium
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 0
                        QQC2.Label {
                            Layout.fillWidth: true
                            text: row.modelData.name
                            textFormat: Text.PlainText
                            color: row.highlighted ? Kirigami.Theme.highlightedTextColor : Kirigami.Theme.textColor
                            elide: Text.ElideRight
                        }
                        QQC2.Label {
                            Layout.fillWidth: true
                            text: row.modelData.folder
                            textFormat: Text.PlainText
                            color: row.highlighted ? Kirigami.Theme.highlightedTextColor : Kirigami.Theme.disabledTextColor
                            font: Kirigami.Theme.smallFont
                            elide: Text.ElideMiddle
                        }
                    }
                }
            }
        }

        footer: QQC2.ToolBar {
            position: QQC2.ToolBar.Footer
            contentItem: RowLayout {
                QQC2.Button {
                    text: i18nc("@action:button", "Create a New Machine…")
                    icon.name: "document-new"
                    onClicked: window.app.newDocument()
                }
                Item { Layout.fillWidth: true }
                QQC2.Button {
                    text: i18nc("@action:button", "Open")
                    icon.name: "document-open"
                    enabled: window.selectedEntry !== null
                    highlighted: true
                    onClicked: window.openSelected()
                }
            }
        }
    }
}
