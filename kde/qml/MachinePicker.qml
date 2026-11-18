// The machine picker: makers on the left, their machines on the right,
// and a search field. The list is the core's catalogue.

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import Pinesy

Kirigami.Dialog {
    id: picker

    // False while a machine runs: a new configuration waits for it.
    property bool canChoose: true
    signal chosen(string machine)
    signal cancelled()

    readonly property var sections: Core.catalogue
    property int sectionIndex: 0
    readonly property string searchText: searchField.text.trim()
    readonly property bool searching: searchText !== ""
    readonly property var shownMachines: searching ? Core.search(searchText)
                                                   : (sections.length > 0 ? sections[sectionIndex].machines : [])
    readonly property bool showsMaker: searching || (sections.length > 0 && sections[sectionIndex].showsMaker)
    readonly property var selectedMachine: machineList.currentIndex >= 0 && machineList.currentIndex < shownMachines.length
                                           ? shownMachines[machineList.currentIndex] : null

    title: i18nc("@title:dialog", "Choose a Machine")
    preferredWidth: Kirigami.Units.gridUnit * 40
    padding: 0
    closePolicy: QQC2.Popup.CloseOnEscape
    showCloseButton: false

    property bool choosing: false

    function choose() {
        if (selectedMachine && canChoose) {
            const id = selectedMachine.id
            choosing = true
            picker.close()
            picker.chosen(id)
        }
    }

    onOpened: {
        choosing = false
        searchField.forceActiveFocus()
    }
    // Closed without a choice: Cancel, or Escape
    onClosed: {
        if (!choosing)
            cancelled()
    }

    customFooterActions: [
        Kirigami.Action {
            text: i18nc("@action:button", "Cancel")
            icon.name: "dialog-cancel"
            onTriggered: picker.close()
        },
        Kirigami.Action {
            text: i18nc("@action:button", "Choose")
            icon.name: "dialog-ok"
            enabled: picker.selectedMachine !== null && picker.canChoose
            tooltip: picker.canChoose ? "" : i18n("A machine is running. Choose when it stops.")
            onTriggered: picker.choose()
        }
    ]

    ColumnLayout {
        spacing: 0

        Kirigami.SearchField {
            id: searchField
            Layout.fillWidth: true
            Layout.margins: Kirigami.Units.smallSpacing
            placeholderText: i18nc("@info:placeholder", "Search machines")
            onTextChanged: machineList.currentIndex = -1
            // Down moves into the machine list
            Keys.onDownPressed: {
                if (machineList.count > 0) {
                    machineList.currentIndex = Math.max(0, machineList.currentIndex)
                    machineList.forceActiveFocus()
                }
            }
        }

        Kirigami.Separator { Layout.fillWidth: true }

        // The lists get a fixed height and scroll inside it. Under some
        // styles (not Plasma's) a ScrollView's own height is the whole
        // length of its list, so without this the lists would not scroll.
        RowLayout {
            spacing: 0
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredHeight: Kirigami.Units.gridUnit * 22

            QQC2.ScrollView {
                Layout.preferredWidth: Kirigami.Units.gridUnit * 12
                Layout.fillHeight: true
                enabled: !picker.searching

                ListView {
                    id: sectionList
                    clip: true
                    model: picker.sections
                    currentIndex: picker.searching ? -1 : picker.sectionIndex
                    delegate: QQC2.ItemDelegate {
                        required property var modelData
                        required property int index
                        width: ListView.view.width
                        text: modelData.name
                        highlighted: ListView.isCurrentItem
                        onClicked: {
                            picker.sectionIndex = index
                            machineList.currentIndex = -1
                        }
                    }
                }
            }

            Kirigami.Separator { Layout.fillHeight: true }

            QQC2.ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true

                ListView {
                    id: machineList
                    clip: true
                    model: picker.shownMachines
                    currentIndex: -1
                    keyNavigationEnabled: true
                    Keys.onReturnPressed: picker.choose()
                    Keys.onEnterPressed: picker.choose()

                    delegate: QQC2.ItemDelegate {
                        id: row
                        required property var modelData
                        required property int index
                        width: ListView.view.width
                        highlighted: ListView.isCurrentItem
                        onClicked: machineList.currentIndex = index
                        onDoubleClicked: {
                            machineList.currentIndex = index
                            picker.choose()
                        }

                        contentItem: RowLayout {
                            spacing: Kirigami.Units.largeSpacing
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 0
                                QQC2.Label {
                                    Layout.fillWidth: true
                                    text: row.modelData.name
                                    color: row.highlighted ? Kirigami.Theme.highlightedTextColor : Kirigami.Theme.textColor
                                    elide: Text.ElideRight
                                }
                                QQC2.Label {
                                    Layout.fillWidth: true
                                    visible: text !== ""
                                    text: picker.showsMaker && !row.modelData.manufacturer.startsWith("<")
                                          ? row.modelData.manufacturer : ""
                                    color: row.highlighted ? Kirigami.Theme.highlightedTextColor : Kirigami.Theme.disabledTextColor
                                    font: Kirigami.Theme.smallFont
                                    elide: Text.ElideRight
                                }
                            }
                            QQC2.Label {
                                visible: row.modelData.preliminary
                                text: i18nc("@info:status", "Partly working")
                                color: Kirigami.Theme.neutralTextColor
                                font: Kirigami.Theme.smallFont
                                QQC2.ToolTip.visible: hoverHandler.hovered
                                QQC2.ToolTip.text: i18n("The emulation of this machine is not complete. Some software will not run correctly.")
                                HoverHandler { id: hoverHandler }
                            }
                            QQC2.Label {
                                text: row.modelData.year.indexOf("?") >= 0 ? "" : row.modelData.year
                                color: Kirigami.Theme.disabledTextColor
                                font.features: { "tnum": 1 }
                            }
                        }
                    }

                    Kirigami.PlaceholderMessage {
                        anchors.centerIn: parent
                        width: parent.width - Kirigami.Units.gridUnit * 4
                        visible: machineList.count === 0
                        icon.name: picker.sections.length === 0 ? "dialog-warning" : "edit-find"
                        text: picker.sections.length === 0 ? i18n("No machines")
                                                           : i18n("No machines match \"%1\"", picker.searchText)
                        explanation: picker.sections.length === 0 ? i18n("The emulator reported no machines.") : ""
                    }
                }
            }
        }
    }
}
