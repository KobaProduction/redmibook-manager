pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import Redmibook.Manager
import ".."

ColumnLayout {
    id: root
    required property ManagerModel manager
    required property DeviceControlsModel controls
    required property PowerControlsModel powerControls
    readonly property var telemetry: controls.batteryTelemetry
    readonly property int samplingInterval: manager.batteryIntervalMs
    onSamplingIntervalChanged: { if (batteryLease.running) controls.watchBatteryTelemetry(samplingInterval) }
    Component.onDestruction: { if (controls) controls.watchBatteryTelemetry(0) }
    function metric(value: var, unit: string): string {
        return value === undefined ? qsTr("Unavailable") : Number(value).toLocaleString(Qt.locale(), 'f', 1) + " " + unit
    }
    Timer {
        id: batteryLease
        interval: 1000
        running: root.visible
                 && root.Window.window !== null && root.Window.window.active
                 && root.Window.window.visibility !== Window.Minimized
        repeat: true
        triggeredOnStart: true
        onTriggered: root.controls.watchBatteryTelemetry(root.samplingInterval)
        onRunningChanged: { if (!running && root.controls) root.controls.watchBatteryTelemetry(0) }
    }
    spacing: 18
    RowLayout {
        Layout.fillWidth: true
        Label { text: qsTr("Battery"); font.pixelSize: 18; font.bold: true; Layout.fillWidth: true }
        Label { text: qsTr("Auto-refresh"); color: palette.placeholderText }
        ChoiceBox {
            id: refreshInterval
            model: ["100 ms", "1 sec", "5 sec"]
            currentIndex: Math.max(0, [100, 1000, 5000].indexOf(root.samplingInterval))
            onActivated: root.manager.setBatteryIntervalMs([100, 1000, 5000][currentIndex])
            Layout.preferredWidth: 140
            Accessible.name: qsTr("Auto-refresh interval")
        }
        BusyIndicator { Layout.preferredWidth: 30; Layout.preferredHeight: 30; running: root.controls.busy || root.controls.refreshing; opacity: running ? 1 : 0 }
        Button { text: qsTr("Refresh"); enabled: !root.controls.busy && !root.controls.refreshing; onClicked: root.controls.requestRefresh() }
    }
    Surface {
        Layout.fillWidth: true
        cornerRadius: root.manager.cornerRadius
        ColumnLayout {
            width: parent.width
            spacing: 12
            Label { text: qsTr("Battery state: %1").arg(root.telemetry.state || qsTr("Unavailable")); font.pixelSize: 18; font.bold: true; Layout.fillWidth: true; wrapMode: Text.WordWrap }
            GridLayout {
                Layout.fillWidth: true
                columns: 2
                columnSpacing: 32
                rowSpacing: 10
                Label { text: qsTr("Remaining capacity"); Layout.fillWidth: true }
                Label { text: root.metric(root.telemetry.remainingWh, qsTr("Wh")) }
                Label { text: qsTr("Full charge capacity") }
                Label { text: root.metric(root.telemetry.fullWh, qsTr("Wh")) }
                Label { text: qsTr("Design capacity") }
                Label { text: root.metric(root.telemetry.designWh, qsTr("Wh")) }
                Label { text: qsTr("Estimated wear") }
                Label { text: root.metric(root.telemetry.wear, "%") }
                Label { text: qsTr("Voltage") }
                Label { text: root.metric(root.telemetry.voltage, qsTr("V")) }
                Label { text: root.telemetry.rateLabel || qsTr("Charge") }
                Label { text: root.metric(root.telemetry.rateW === undefined ? undefined : Math.abs(root.telemetry.rateW), qsTr("W")) }
            }
        }
    }
    Surface {
        visible: root.controls.batterySupported || root.powerControls.modules.indexOf("energy-saver") >= 0
        Layout.fillWidth: true
        cornerRadius: root.manager.cornerRadius
        ColumnLayout {
            width: parent.width
            spacing: 14
            RowLayout {
                visible: root.controls.batterySupported
                Layout.fillWidth: true
                Label { text: qsTr("Charge limit"); font.pixelSize: 18; font.bold: true; Layout.fillWidth: true }
                Label { text: qsTr("Battery: %1%").arg(root.controls.batteryPercent); visible: root.controls.batteryPercent >= 0; color: palette.placeholderText }
            }
            RowLayout {
                visible: root.controls.batterySupported
                Layout.fillWidth: true
                Button {
                    text: "80%"
                    Layout.fillWidth: true
                    highlighted: root.controls.batteryAvailable && root.controls.batteryProtectionEnabled
                    icon.color: highlighted ? palette.highlightedText : palette.buttonText
                    enabled: root.controls.batteryAvailable && !root.controls.busy
                    onClicked: root.controls.setBatteryProtection(true)
                }
                Button {
                    text: "100%"
                    Layout.fillWidth: true
                    highlighted: root.controls.batteryAvailable && !root.controls.batteryProtectionEnabled
                    icon.color: highlighted ? palette.highlightedText : palette.buttonText
                    enabled: root.controls.batteryAvailable && !root.controls.busy
                    onClicked: root.controls.setBatteryProtection(false)
                }
            }
            RowLayout {
                visible: root.powerControls.modules.indexOf("energy-saver") >= 0
                Layout.fillWidth: true
                Label { text: qsTr("Energy saver"); font.pixelSize: 18; font.bold: true; Layout.fillWidth: true }
                Switch {
                    checked: root.powerControls.saverEnabled
                    enabled: root.powerControls.saverControllable && !root.powerControls.busy
                    Accessible.name: qsTr("Energy saver")
                    onClicked: root.powerControls.setSaver(checked)
                }
            }
        }
    }
}
