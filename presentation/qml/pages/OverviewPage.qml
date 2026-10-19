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
    required property AboutModel about
    readonly property var panel: controls.builtinDisplay
    ColumnLayout {
        anchors.fill: parent
        spacing: 18
        RowLayout {
            Layout.fillWidth: true
            ColumnLayout {
                Layout.fillWidth: true
                Label { text: root.about.model || qsTr("Laptop"); font.pixelSize: 26; font.bold: true; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                Label { text: root.about.manufacturer; visible: text.length > 0; color: palette.placeholderText }
            }
            BusyIndicator { Layout.preferredWidth: 30; Layout.preferredHeight: 30; running: root.controls.busy; opacity: running ? 1 : 0 }
            Button { text: qsTr("Refresh"); enabled: !root.controls.busy; onClicked: root.controls.refresh() }
        }
        Surface {
            Layout.fillWidth: true
            cornerRadius: root.manager.cornerRadius
            ColumnLayout {
                width: parent.width
                spacing: 12
                Label { text: qsTr("Performance mode"); font.pixelSize: 18; font.bold: true }
                RowLayout {
                    Layout.fillWidth: true
                    Repeater {
                        model: [qsTr("Balanced"), qsTr("Quiet"), qsTr("Turbo")]
                        Button {
                            required property string modelData
                            required property int index
                            text: modelData
                            Layout.fillWidth: true
                            highlighted: root.controls.profileAvailable && root.controls.profileIndex === index
                            enabled: root.controls.profileAvailable && !root.controls.busy
                            onClicked: root.controls.setPerformanceProfile(index)
                        }
                    }
                }
                Label { text: root.controls.profileStatus; visible: !root.controls.profileAvailable; color: palette.placeholderText; Layout.fillWidth: true; wrapMode: Text.WordWrap }
            }
        }
        GridLayout {
            Layout.fillWidth: true
            columns: 2
            columnSpacing: 16
            rowSpacing: 16
            Surface {
                Layout.fillWidth: true
                Layout.fillHeight: true
                cornerRadius: root.manager.cornerRadius
                ColumnLayout {
                    width: parent.width
                    Label { text: qsTr("Battery"); font.bold: true }
                    Label { text: qsTr("%1%").arg(root.controls.batteryPercent); visible: root.controls.batteryPercent >= 0; font.pixelSize: 26 }
                    Label {
                        text: !root.controls.batteryAvailable ? qsTr("Unavailable") : root.controls.batteryProtectionEnabled ? qsTr("Charge limit: 80%") : qsTr("Full charge: 100%")
                        color: palette.placeholderText
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                    }
                }
            }
            Surface {
                Layout.fillWidth: true
                Layout.fillHeight: true
                cornerRadius: root.manager.cornerRadius
                ColumnLayout {
                    width: parent.width
                    Label { text: qsTr("Built-in display"); font.bold: true }
                    Label { text: root.panel.currentHz > 0 ? qsTr("%1 Hz").arg(root.panel.currentHz) : qsTr("Unavailable"); font.pixelSize: 26 }
                    Label { text: root.panel.name || ""; color: palette.placeholderText; Layout.fillWidth: true; elide: Text.ElideRight }
                }
            }
            Surface {
                Layout.fillWidth: true
                Layout.columnSpan: 2
                cornerRadius: root.manager.cornerRadius
                RowLayout {
                    width: parent.width
                    Label { text: qsTr("Microphone"); font.bold: true; Layout.fillWidth: true }
                    Label { text: !root.controls.microphoneAvailable ? qsTr("Unavailable") : root.controls.microphoneMuted ? qsTr("Muted") : qsTr("On") }
                }
            }
        }
        Label { text: root.controls.operationStatus; visible: text.length > 0; Layout.fillWidth: true; wrapMode: Text.WordWrap; color: palette.placeholderText }
        Item { Layout.fillHeight: true }
    }
}
