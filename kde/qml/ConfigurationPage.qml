// The configuration page: the machine, its model and ROM, its hardware,
// its media and its command line. Everything shown is the core's summary
// of the document's command line.

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import Pinesy

Kirigami.Page {
    id: page

    required property ConfigurationDocument document
    readonly property var summary: document.summary
    readonly property bool hasSummary: summary.driver !== undefined
    readonly property bool running: MachineRunner.running !== ""

    signal chooseMachine()
    signal run(bool debug)
    signal chooseMedia(string name, var extensions)

    title: document.title

    actions: [
        Kirigami.Action {
            text: i18nc("@action:intoolbar", "Run with Debugger")
            icon.name: "debug-run"
            enabled: page.document.canRun
            tooltip: i18n("Run this configuration with the debugger")
            onTriggered: page.run(true)
        },
        Kirigami.Action {
            text: i18nc("@action:intoolbar", "Run")
            icon.name: "media-playback-start"
            enabled: page.document.canRun
            tooltip: i18n("Run this configuration")
            onTriggered: page.run(false)
        }
    ]

    Kirigami.PlaceholderMessage {
        anchors.centerIn: parent
        width: parent.width - Kirigami.Units.gridUnit * 4
        visible: !page.hasSummary
        icon.name: page.document.waiting ? "media-playback-start" : "computer"
        text: page.document.waiting ? i18n("A machine is running") : i18n("No machine")
        explanation: page.document.waiting ? i18n("This configuration opens when the running machine stops.")
                                           : i18n("Choose a machine for this configuration.")
        helpfulAction: page.document.waiting ? null : chooseAction
        Kirigami.Action {
            id: chooseAction
            text: i18nc("@action:button", "Choose a Machine...")
            icon.name: "list-add"
            onTriggered: page.chooseMachine()
        }
    }

    QQC2.ScrollView {
        id: scroll
        anchors.fill: parent
        visible: page.hasSummary
        contentWidth: availableWidth

        ColumnLayout {
            width: scroll.availableWidth
            enabled: !page.running
            spacing: Kirigami.Units.largeSpacing

            Kirigami.InlineMessage {
                Layout.fillWidth: true
                visible: page.hasSummary && page.summary.errors !== ""
                type: Kirigami.MessageType.Error
                text: "<b>" + i18n("This configuration cannot run") + "</b><br>" + escapeText(page.summary.errors ?? "")
            }

            Kirigami.InlineMessage {
                Layout.fillWidth: true
                visible: page.hasSummary && page.summary.warnings !== ""
                type: Kirigami.MessageType.Information
                text: "<b>" + i18n("The emulator warns") + "</b><br>" + escapeText(page.summary.warnings ?? "")
            }

            // One form per section: a Repeater adds new rows at the end of
            // its form, so each section's rows stay under its own heading.
            Kirigami.FormLayout {
                id: machineForm
                Layout.fillWidth: true
                twinFormLayouts: [hardwareForm, mediaForm, commandForm]

                // The machine
                ColumnLayout {
                    Kirigami.FormData.isSection: true
                    spacing: 0
                    Kirigami.Heading {
                        level: 2
                        text: page.summary.machine ? page.summary.machine.name : (page.summary.driver ?? "")
                    }
                    QQC2.Label {
                        color: Kirigami.Theme.disabledTextColor
                        visible: text !== ""
                        text: {
                            const machine = page.summary.machine
                            if (!machine)
                                return ""
                            const parts = []
                            if (!machine.manufacturer.startsWith("<"))
                                parts.push(machine.manufacturer)
                            const year = page.summary.year ?? ""
                            if (year !== "" && year.indexOf("?") < 0)
                                parts.push(year)
                            return parts.join(", ")
                        }
                    }
                    QQC2.Label {
                        visible: page.summary.preliminary === true
                        text: i18n("The emulation of this machine is not complete.")
                        color: Kirigami.Theme.neutralTextColor
                        font: Kirigami.Theme.smallFont
                    }
                }

                QQC2.ComboBox {
                    Kirigami.FormData.label: i18nc("@label:listbox", "Model:")
                    visible: (page.summary.models ?? []).length > 1
                    Layout.minimumWidth: Kirigami.Units.gridUnit * 14
                    model: (page.summary.models ?? []).map(m => ({
                        value: m.driver,
                        text: m.year.indexOf("?") >= 0 ? m.label : i18nc("model (year)", "%1 (%2)", m.label, m.year)
                    }))
                    textRole: "text"
                    valueRole: "value"
                    currentIndex: (page.summary.models ?? []).findIndex(m => m.driver === page.summary.driver)
                    onActivated: page.document.setDriver(currentValue)
                }

                QQC2.ComboBox {
                    id: romBox
                    readonly property var roms: (page.summary.roms ?? []).concat(page.summary.variantROMs ?? [])
                    Kirigami.FormData.label: i18nc("@label:listbox", "ROM:")
                    visible: roms.length > 1
                    Layout.minimumWidth: Kirigami.Units.gridUnit * 14
                    model: roms.map(r => r.label)
                    currentIndex: {
                        const current = page.summary.currentROM
                        if (!current)
                            return -1
                        return roms.findIndex(r => r.driver === current.driver && r.bios === current.bios)
                    }
                    onActivated: index => page.document.setRom(roms[index].driver, roms[index].bios)
                }

                // the core's choices, and a magnification field for Custom
                QQC2.ComboBox {
                    Kirigami.FormData.label: i18nc("@label:listbox", "Window size:")
                    Layout.minimumWidth: Kirigami.Units.gridUnit * 14
                    model: page.document.windowChoices.map(choice =>
                        choice === page.document.windowFit ? i18nc("@item:inlistbox window size", "Fit to screen")
                        : choice === page.document.windowFullScreen ? i18nc("@item:inlistbox window size", "Full screen")
                        : choice === page.document.windowCustom ? i18nc("@item:inlistbox window size", "Custom")
                        : i18nc("@item:inlistbox window magnification", "%1×", choice))
                    currentIndex: page.document.windowChoices.indexOf(page.document.windowChoice)
                    onActivated: index => page.document.chooseWindowSize(page.document.windowChoices[index])
                }

                QQC2.SpinBox {
                    Kirigami.FormData.label: i18nc("@label:spinbox", "Magnification:")
                    visible: page.document.windowChoice === page.document.windowCustom
                    editable: true
                    from: 1
                    to: 2147483647
                    value: page.document.windowSize
                    onValueModified: page.document.setCustomWindowSize(value)
                }

            }

            Kirigami.FormLayout {
                id: hardwareForm
                Layout.fillWidth: true
                visible: (page.summary.hardware ?? []).length > 0
                twinFormLayouts: [machineForm, mediaForm, commandForm]
                Item {
                    Kirigami.FormData.isSection: true
                    Kirigami.FormData.label: i18nc("@title:group", "Hardware")
                    visible: (page.summary.hardware ?? []).length > 0
                }

                Repeater {
                    model: page.summary.hardware ?? []
                    delegate: ColumnLayout {
                        id: hardwareRow
                        required property var modelData
                        readonly property var slot: modelData.slot
                        readonly property var choices: {
                            const list = modelData.choices.slice()
                            if (!list.some(c => c.value === slot.current))
                                list.push({ value: slot.current, label: slot.currentdescription || slot.current })
                            return list
                        }
                        Kirigami.FormData.label: modelData.label + ":"
                        Layout.leftMargin: modelData.depth * Kirigami.Units.gridUnit
                        spacing: Kirigami.Units.smallSpacing

                        QQC2.Label {
                            visible: hardwareRow.modelData.fixedText !== null
                            text: hardwareRow.modelData.fixedText ?? ""
                        }
                        QQC2.ComboBox {
                            visible: hardwareRow.modelData.fixedText === null
                            Layout.minimumWidth: Kirigami.Units.gridUnit * 14
                            model: hardwareRow.choices
                            textRole: "label"
                            valueRole: "value"
                            currentIndex: hardwareRow.choices.findIndex(c => c.value === hardwareRow.slot.current)
                            onActivated: page.document.setSlot(hardwareRow.slot.tag, currentValue)
                        }
                        QQC2.Label {
                            visible: hardwareRow.modelData.meantFor !== null
                            text: i18n("Made for: %1", hardwareRow.modelData.meantFor ?? "")
                            color: Kirigami.Theme.disabledTextColor
                            font: Kirigami.Theme.smallFont
                        }
                        RowLayout {
                            visible: hardwareRow.modelData.roms.length > 0
                            QQC2.Label { text: i18nc("@label:listbox", "ROM:") }
                            QQC2.ComboBox {
                                model: hardwareRow.modelData.roms.map(r => ({ value: r.value, label: r.label }))
                                textRole: "label"
                                valueRole: "value"
                                currentIndex: hardwareRow.modelData.roms.findIndex(r => r.value === hardwareRow.modelData.currentROM)
                                onActivated: page.document.setCardRom(hardwareRow.slot.tag, currentValue)
                            }
                        }
                    }
                }

            }

            Kirigami.FormLayout {
                id: mediaForm
                Layout.fillWidth: true
                visible: (page.summary.media ?? []).length > 0
                twinFormLayouts: [machineForm, hardwareForm, commandForm]
                Item {
                    Kirigami.FormData.isSection: true
                    Kirigami.FormData.label: i18nc("@title:group", "Media")
                    visible: (page.summary.media ?? []).length > 0
                }

                Repeater {
                    model: page.summary.media ?? []
                    delegate: ColumnLayout {
                        id: mediaRow
                        required property var modelData
                        readonly property var media: modelData.media
                        readonly property var extensions: media.extensions === "" ? [] : media.extensions.split(",")
                        readonly property bool needed: media.mandatory && media.current === ""
                        readonly property bool missing: (page.summary.missingFiles ?? []).indexOf(media.name) >= 0
                        readonly property string sdCardProblem: (page.summary.sdCardProblems ?? {})[media.name] ?? ""
                        readonly property string problem: needed ? i18n("This machine needs a file here before it can run.")
                                                        : missing ? i18n("File not found: %1", media.current)
                                                                  : sdCardProblem
                        Kirigami.FormData.label: modelData.label + ":"
                        spacing: 0

                        RowLayout {
                            Kirigami.Icon {
                                visible: mediaRow.problem !== ""
                                source: "dialog-error"
                                implicitWidth: Kirigami.Units.iconSizes.small
                                implicitHeight: Kirigami.Units.iconSizes.small
                            }
                            QQC2.Label {
                                Layout.maximumWidth: Kirigami.Units.gridUnit * 14
                                elide: Text.ElideMiddle
                                text: mediaRow.media.current === "" ? i18nc("@info no file in a drive", "Empty")
                                                                    : mediaRow.media.current.split("/").pop()
                                color: mediaRow.media.current === "" ? Kirigami.Theme.disabledTextColor : Kirigami.Theme.textColor
                                QQC2.ToolTip.visible: mediaRow.media.current !== "" && fileHover.hovered
                                QQC2.ToolTip.text: mediaRow.media.current
                                HoverHandler { id: fileHover }
                            }
                            QQC2.Button {
                                text: i18nc("@action:button", "Choose...")
                                onClicked: page.chooseMedia(mediaRow.media.name, mediaRow.extensions)
                            }
                            QQC2.ToolButton {
                                visible: mediaRow.media.current !== ""
                                icon.name: "media-eject"
                                display: QQC2.AbstractButton.IconOnly
                                text: i18nc("@action:button", "Remove the file")
                                QQC2.ToolTip.visible: hovered
                                QQC2.ToolTip.text: text
                                onClicked: page.document.setMedia(mediaRow.media.name, "")
                            }
                        }
                        QQC2.Label {
                            visible: mediaRow.problem !== ""
                            Layout.maximumWidth: Kirigami.Units.gridUnit * 20
                            wrapMode: Text.WrapAnywhere
                            text: mediaRow.problem
                            color: Kirigami.Theme.negativeTextColor
                            font: Kirigami.Theme.smallFont
                        }
                        QQC2.Label {
                            Layout.maximumWidth: Kirigami.Units.gridUnit * 20
                            elide: Text.ElideRight
                            text: mediaRow.extensions.map(e => "." + e).join(" ")
                            color: Kirigami.Theme.disabledTextColor
                            font: Kirigami.Theme.smallFont
                        }
                    }
                }

            }

            Kirigami.FormLayout {
                id: commandForm
                Layout.fillWidth: true
                twinFormLayouts: [machineForm, hardwareForm, mediaForm]
                Item {
                    Kirigami.FormData.isSection: true
                    Kirigami.FormData.label: i18nc("@title:group", "Command line")
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    Kirigami.SelectableLabel {
                        Layout.fillWidth: true
                        Layout.maximumWidth: scroll.availableWidth - Kirigami.Units.gridUnit * 8
                        text: page.document.commandLine
                        font.family: "monospace"
                        wrapMode: Text.WrapAnywhere
                    }
                    QQC2.Button {
                        text: i18nc("@action:button", "Copy Command Line")
                        icon.name: "edit-copy"
                        onClicked: page.document.copyCommandLine()
                    }
                }
            }
        }
    }

    function escapeText(text) {
        return text.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;").replace(/\n/g, "<br>")
    }
}
