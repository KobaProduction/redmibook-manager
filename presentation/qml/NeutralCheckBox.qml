import QtQuick
import QtQuick.Controls

CheckBox {
    id: root
    indicator: Rectangle {
        implicitWidth: 20
        implicitHeight: 20
        x: root.leftPadding
        y: root.topPadding + (root.availableHeight - height) / 2
        radius: 4
        color: root.checked ? root.palette.highlight : root.palette.base
        border.width: root.checked ? 0 : 2
        border.color: root.enabled ? root.palette.windowText : root.palette.placeholderText
        Text {
            anchors.centerIn: parent
            text: "✓"
            visible: root.checked
            color: root.palette.highlightedText
            font.pixelSize: 16
            font.weight: Font.DemiBold
        }
    }
}
