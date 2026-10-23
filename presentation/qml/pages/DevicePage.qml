pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Redmibook.Manager

Pane {
    id: root
    required property ManagerModel manager
    required property DeviceControlsModel controls
    required property PowerControlsModel powerControls
    padding: 0
    ScrollView {
        id: scroll
        objectName: "deviceScroll"
        anchors.fill: parent
        rightPadding: 16
        contentWidth: availableWidth
        clip: true
        ColumnLayout {
            width: scroll.availableWidth
            spacing: 18
            Label { text: qsTr("Device"); font.pixelSize: 26; font.bold: true }
            BatteryPage { manager: root.manager; controls: root.controls; powerControls: root.powerControls; Layout.fillWidth: true }
            HardwarePage { manager: root.manager; controls: root.controls; powerControls: root.powerControls; Layout.fillWidth: true }
        }
    }
}
