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
    readonly property var telemetry: controls.batteryTelemetry
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
            BusyIndicator { Layout.preferredWidth: 30; Layout.preferredHeight: 30; running: root.controls.busy || root.controls.refreshing; opacity: running ? 1 : 0 }
            Button { text: qsTr("Refresh"); enabled: !root.controls.busy && !root.controls.refreshing; onClicked: root.controls.requestRefresh() }
        }
        Surface {
            visible: root.controls.profileSupported || root.controls.displaySupported || root.controls.batterySupported || root.controls.microphoneControllable || root.controls.microphoneAvailable
            Layout.fillWidth: true
            Layout.fillHeight: false
            cornerRadius: root.manager.cornerRadius
            GridLayout {
                id: controlGrid
                width: parent.width
                columns: root.width >= 720 ? 2 : 1
                columnSpacing: 24
                rowSpacing: 18
                Label {
                    visible: root.controls.profileSupported
                    text: qsTr("Performance mode"); font.pixelSize: 16; font.bold: true
                    wrapMode: Text.WordWrap
                    Layout.preferredWidth: controlGrid.columns === 2 ? 180 : -1
                    Layout.fillWidth: controlGrid.columns === 1
                }
                ColumnLayout {
                    visible: root.controls.profileSupported
                    Layout.fillWidth: true
                    spacing: 6
                    RowLayout {
                        Layout.fillWidth: true
                        uniformCellSizes: true
                        spacing: 6
                        Repeater {
                            model: [qsTr("Quiet"), qsTr("Balanced"), qsTr("Performance")]
                            Button {
                                required property string modelData
                                required property int index
                                text: modelData
                                Layout.fillWidth: true
                                Layout.preferredWidth: 0
                                highlighted: root.controls.profileAvailable && root.controls.profileIndex === [1, 0, 2][index]
                                enabled: root.controls.profileAvailable && !root.controls.busy
                                onClicked: root.controls.setPerformanceProfile([1, 0, 2][index])
                            }
                        }
                    }
                    Label { text: root.controls.profileStatus; visible: !root.controls.profileAvailable; color: palette.placeholderText; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                }
                Label {
                    visible: root.controls.displaySupported
                    text: qsTr("Built-in display"); font.pixelSize: 16; font.bold: true
                    wrapMode: Text.WordWrap
                    Layout.preferredWidth: controlGrid.columns === 2 ? 180 : -1
                    Layout.fillWidth: controlGrid.columns === 1
                }
                ColumnLayout {
                    visible: root.controls.displaySupported
                    Layout.fillWidth: true
                    spacing: 6
                    RefreshRateButtons { controls: root.controls; Layout.fillWidth: true }
                    Label {
                        text: root.panel.currentHz > 0 ? qsTr("%1 Hz").arg(root.panel.currentHz) : qsTr("Unavailable")
                        visible: (root.panel.selectedRates || []).indexOf(root.panel.currentHz) < 0
                        color: palette.placeholderText
                    }
                }
                Label {
                    visible: root.controls.batterySupported
                    text: qsTr("Battery"); font.pixelSize: 16; font.bold: true
                    Layout.preferredWidth: controlGrid.columns === 2 ? 180 : -1
                    Layout.fillWidth: controlGrid.columns === 1
                }
                ColumnLayout {
                    visible: root.controls.batterySupported
                    Layout.fillWidth: true
                    spacing: 12
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8
                        Label { text: qsTr("Charge limit"); Layout.rightMargin: 8 }
                        Button {
                            text: "80%"
                            Layout.fillWidth: true
                            Layout.preferredWidth: 0
                            highlighted: root.controls.batteryAvailable && root.controls.batteryProtectionEnabled
                            enabled: root.controls.batteryAvailable && !root.controls.busy
                            onClicked: root.controls.setBatteryProtection(true)
                        }
                        Button {
                            text: "100%"
                            Layout.fillWidth: true
                            Layout.preferredWidth: 0
                            highlighted: root.controls.batteryAvailable && !root.controls.batteryProtectionEnabled
                            enabled: root.controls.batteryAvailable && !root.controls.busy
                            onClicked: root.controls.setBatteryProtection(false)
                        }
                    }
                    GridLayout {
                        Layout.fillWidth: true
                        columns: 3
                        columnSpacing: 18
                        Label {
                            text: qsTr("Charge: %1%").arg(root.controls.batteryPercent)
                            visible: root.controls.batteryPercent >= 0
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                        }
                        Label {
                            visible: root.telemetry.wear !== undefined
                            text: qsTr("Wear: %1%").arg(Number(root.telemetry.wear).toLocaleString(Qt.locale(), 'f', 1))
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                        }
                        Label {
                            visible: root.telemetry.rateW !== undefined
                            text: qsTr("%1: %2 W").arg(root.telemetry.rateLabel || qsTr("Charge")).arg(Math.abs(Number(root.telemetry.rateW)).toLocaleString(Qt.locale(), 'f', 1))
                            Layout.alignment: Qt.AlignRight
                            wrapMode: Text.WordWrap
                        }
                    }
                    Label {
                        visible: !root.controls.batteryAvailable
                        text: root.controls.batteryStatus
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                    }
                }
                Label {
                    visible: root.controls.microphoneControllable || root.controls.microphoneAvailable
                    text: qsTr("Microphone"); font.pixelSize: 16; font.bold: true
                    Layout.preferredWidth: controlGrid.columns === 2 ? 180 : -1
                    Layout.fillWidth: controlGrid.columns === 1
                }
                RowLayout {
                    visible: root.controls.microphoneControllable || root.controls.microphoneAvailable
                    Layout.fillWidth: true
                    Label { text: root.controls.microphoneStatus; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                    Switch {
                        visible: root.controls.microphoneControllable
                        checked: !root.controls.microphoneFirmwareMuted
                        enabled: !root.controls.busy
                        Accessible.name: qsTr("Microphone")
                        onClicked: root.controls.setMicrophoneMuted(!checked)
                    }
                }
            }
        }
        Item { Layout.fillHeight: true }
    }
}
