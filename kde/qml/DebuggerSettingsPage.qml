// Debugger settings: what Run with Debugger uses.

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import Pinesy

Kirigami.ScrollablePage {
    id: page

    title: i18nc("@title settings page", "Debugger")

    Kirigami.FormLayout {
        QQC2.ButtonGroup { id: debuggerGroup }

        QQC2.RadioButton {
            Kirigami.FormData.label: i18nc("@label", "Run with Debugger uses:")
            text: i18nc("@option:radio", "MAME's ImGui debugger, in the machine window")
            QQC2.ButtonGroup.group: debuggerGroup
            checked: Core.debugger === "windows"
            onToggled: if (checked) Core.debugger = "windows"
        }
        QQC2.RadioButton {
            text: i18nc("@option:radio", "GDB remote stub")
            QQC2.ButtonGroup.group: debuggerGroup
            checked: Core.debugger === "gdbStub"
            onToggled: if (checked) Core.debugger = "gdbStub"
        }

        QQC2.SpinBox {
            Kirigami.FormData.label: i18nc("@label:spinbox", "GDB port:")
            enabled: Core.debugger === "gdbStub"
            from: Core.minGdbPort
            to: Core.maxGdbPort
            editable: true
            value: Core.gdbPort
            textFromValue: (value, locale) => value.toString()
            valueFromText: (text, locale) => parseInt(text)
            onValueModified: Core.gdbPort = value
        }

        QQC2.Label {
            Layout.maximumWidth: Kirigami.Units.gridUnit * 24
            wrapMode: Text.Wrap
            color: Kirigami.Theme.disabledTextColor
            font: Kirigami.Theme.smallFont
            text: i18n("With the GDB remote stub, the machine waits for a GDB client to connect to this port.")
        }
    }
}
