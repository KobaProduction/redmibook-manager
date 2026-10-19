pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Redmibook.Manager
import ".."

Pane {
    id: root
    padding: 0
    required property ManagerModel manager
    required property DeviceControlsModel controls
    ColumnLayout {
        anchors.fill: parent
        spacing: 18
        RowLayout {
            Layout.fillWidth: true
            Label { text: qsTr("Battery"); font.pixelSize: 26; font.bold: true; Layout.fillWidth: true }
            BusyIndicator { Layout.preferredWidth: 30; Layout.preferredHeight: 30; running: root.controls.busy; opacity: running ? 1 : 0 }
            Button { text: qsTr("Refresh"); enabled: !root.controls.busy; onClicked: root.controls.refresh() }
        }
        Surface {
            Layout.fillWidth: true
            cornerRadius: root.manager.cornerRadius
            ColumnLayout {
                width: parent.width
                spacing: 14
                RowLayout {
                    Layout.fillWidth: true
                    Label { text: qsTr("Charge limit"); font.pixelSize: 18; font.bold: true; Layout.fillWidth: true }
                    Label { text: qsTr("Battery: %1%").arg(root.controls.batteryPercent); visible: root.controls.batteryPercent >= 0; color: palette.placeholderText }
                }
                RowLayout {
                    Layout.fillWidth: true
                    Button {
                        text: "80%"
                        Layout.fillWidth: true
                        highlighted: root.controls.batteryAvailable && root.controls.batteryProtectionEnabled
                        enabled: root.controls.batteryAvailable && !root.controls.busy
                        onClicked: root.controls.setBatteryProtection(true)
                    }
                    Button {
                        text: "100%"
                        Layout.fillWidth: true
                        highlighted: root.controls.batteryAvailable && !root.controls.batteryProtectionEnabled
                        enabled: root.controls.batteryAvailable && !root.controls.busy
                        onClicked: root.controls.setBatteryProtection(false)
                    }
                }
                Label { text: root.controls.batteryStatus; Layout.fillWidth: true; wrapMode: Text.WordWrap; color: palette.placeholderText }
            }
        }
        Label { text: root.controls.operationStatus; visible: text.length > 0; Layout.fillWidth: true; wrapMode: Text.WordWrap }
        Item { Layout.fillHeight: true }
    }
}
