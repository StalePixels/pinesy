// The Settings window: a page for each group of settings, chosen from
// the sidebar. The settings apply as they change.

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Kirigami.ApplicationWindow {
    id: settings

    title: i18nc("@title:window", "Configure Pi/Ne-Sy")
    width: Kirigami.Units.gridUnit * 40
    height: Kirigami.Units.gridUnit * 26
    minimumWidth: Kirigami.Units.gridUnit * 30
    minimumHeight: Kirigami.Units.gridUnit * 18
    pageStack.globalToolBar.style: Kirigami.ApplicationHeaderStyle.None

    readonly property list<QtObject> categories: [
        QtObject {
            readonly property string name: i18nc("@title:tab settings page", "General")
            readonly property string iconName: "configure"
            readonly property Component page: Component { GeneralSettingsPage {} }
        },
        QtObject {
            readonly property string name: i18nc("@title:tab settings page", "Debugger")
            readonly property string iconName: "debug-run"
            readonly property Component page: Component { DebuggerSettingsPage {} }
        }
    ]
    property int current: 0

    pageStack.initialPage: Kirigami.Page {
        padding: 0
        globalToolBarStyle: Kirigami.ApplicationHeaderStyle.None

        RowLayout {
            anchors.fill: parent
            spacing: 0

            QQC2.ScrollView {
                Layout.preferredWidth: Kirigami.Units.gridUnit * 10
                Layout.fillHeight: true
                ListView {
                    model: settings.categories
                    currentIndex: settings.current
                    delegate: QQC2.ItemDelegate {
                        required property var modelData
                        required property int index
                        width: ListView.view.width
                        text: modelData.name
                        icon.name: modelData.iconName
                        highlighted: ListView.isCurrentItem
                        onClicked: settings.current = index
                    }
                }
            }

            Kirigami.Separator { Layout.fillHeight: true }

            Loader {
                Layout.fillWidth: true
                Layout.fillHeight: true
                sourceComponent: settings.categories[settings.current].page
            }
        }
    }
}
