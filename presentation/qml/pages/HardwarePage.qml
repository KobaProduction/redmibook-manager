pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Redmibook.Manager
import ".."

Pane {
    id: root
    padding: 0
    required property DeviceControlsModel controls
    required property ManagerModel manager
    readonly property var panel: controls.builtinDisplay
    ColumnLayout {
        anchors.fill: parent
        spacing: 18
        RowLayout {
            Layout.fillWidth: true
            Label { text: qsTr("Device controls"); font.pixelSize: 26; font.bold: true; Layout.fillWidth: true }
            BusyIndicator { Layout.preferredWidth: 30; Layout.preferredHeight: 30; running: root.controls.busy; opacity: running ? 1 : 0 }
            Button { text: qsTr("Refresh"); enabled: !root.controls.busy; onClicked: root.controls.refresh() }
        }
        Surface {
            Layout.fillWidth: true
            cornerRadius: root.manager.cornerRadius
            RowLayout {
                width: parent.width
                Label { text: qsTr("Microphone"); font.pixelSize: 18; font.bold: true; Layout.fillWidth: true }
                Switch {
                    text: qsTr("Mute")
                    enabled: !root.controls.busy && root.controls.microphoneAvailable
                    checked: root.controls.microphoneMuted
                    onClicked: root.controls.setMicrophoneMuted(checked)
                }
            }
        }
        Surface {
            Layout.fillWidth: true
            cornerRadius: root.manager.cornerRadius
            ColumnLayout {
                width: parent.width
                spacing: 12
                RowLayout {
                    Layout.fillWidth: true
                    Label { text: qsTr("Built-in display"); font.pixelSize: 18; font.bold: true; Layout.fillWidth: true }
                    Label { text: root.panel.currentHz > 0 ? qsTr("%1 Hz").arg(root.panel.currentHz) : qsTr("Unavailable"); color: palette.placeholderText }
                }
                Label { text: root.panel.name || ""; visible: text.length > 0; Layout.fillWidth: true; elide: Text.ElideRight }
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
        Label { text: root.controls.microphoneStatus; visible: !root.controls.microphoneAvailable; Layout.fillWidth: true; wrapMode: Text.WordWrap; color: palette.placeholderText }
        Label { text: root.controls.operationStatus; visible: text.length > 0; Layout.fillWidth: true; wrapMode: Text.WordWrap }
        Item { Layout.fillHeight: true }
    }
}
