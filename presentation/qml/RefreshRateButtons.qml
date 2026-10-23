pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Redmibook.Manager

RowLayout {
    id: root
    required property DeviceControlsModel controls
    readonly property var panel: controls.builtinDisplay
    spacing: 6
    uniformCellSizes: true
    Repeater {
        model: root.panel.selectedRates || []
        Button {
            required property int modelData
            text: qsTr("%1 Hz").arg(modelData)
            Layout.fillWidth: true
            Layout.preferredWidth: 0
            highlighted: root.panel.currentHz === modelData
            icon.color: highlighted ? palette.highlightedText : palette.buttonText
            enabled: !root.controls.busy
            onClicked: root.controls.setRefresh(root.panel.displayId, modelData)
        }
    }
}
