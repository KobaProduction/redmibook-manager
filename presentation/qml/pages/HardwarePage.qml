pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import Redmibook.Manager
import ".."

ColumnLayout {
    id: root
    required property DeviceControlsModel controls
    required property ManagerModel manager
    required property PowerControlsModel powerControls
    readonly property var panel: controls.builtinDisplay
    spacing: 18
    Surface {
        visible: root.controls.microphoneAvailable
        Layout.fillWidth: true
        cornerRadius: root.manager.cornerRadius
        RowLayout {
            width: parent.width
            Label { text: qsTr("Microphone"); font.pixelSize: 18; font.bold: true; Layout.fillWidth: true }
            Label {
                visible: !root.controls.microphoneControllable
                text: root.controls.microphoneStatus
            }
            Switch {
                visible: root.controls.microphoneControllable
                enabled: !root.controls.busy
                checked: !root.controls.microphoneFirmwareMuted
                Accessible.name: qsTr("Microphone")
                onClicked: root.controls.setMicrophoneMuted(!checked)
            }
        }
    }
    Surface {
        visible: root.panel.displayId !== undefined
        Layout.fillWidth: true
        cornerRadius: root.manager.cornerRadius
        ColumnLayout {
            width: parent.width
            spacing: 12
            RowLayout {
                Layout.fillWidth: true
                Label { text: qsTr("Built-in display"); font.pixelSize: 18; font.bold: true; Layout.fillWidth: true }
                Label {
                    text: root.panel.currentHz > 0 ? qsTr("%1 Hz").arg(root.panel.currentHz) : qsTr("Unavailable")
                    visible: (root.panel.selectedRates || []).indexOf(root.panel.currentHz) < 0
                    color: palette.placeholderText
                }
            }
            Label { text: root.panel.name || ""; visible: text.length > 0; Layout.fillWidth: true; elide: Text.ElideRight }
            RefreshRateButtons { controls: root.controls; Layout.fillWidth: true }
            Label { text: qsTr("Refresh-rate cycle"); font.bold: true }
            RowLayout {
                Repeater {
                    model: [60, 120, 165]
                    CheckBox {
                        required property int modelData
                        text: qsTr("%1 Hz").arg(modelData)
                        visible: (root.panel.rates || []).indexOf(modelData) >= 0
                        checked: (root.panel.selectedRates || []).indexOf(modelData) >= 0
                        enabled: !root.controls.busy
                        onClicked: root.controls.setCycleRate(modelData, checked)
                    }
                }
            }
            Label {
                text: qsTr("Choose the rates used by the refresh shortcut and tray.")
                visible: root.panel.displayId !== undefined
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: palette.placeholderText
            }
        }
    }
}
