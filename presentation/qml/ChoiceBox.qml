import QtQuick
import QtQuick.Controls

ComboBox {
    id: root
    background: Rectangle {
        implicitWidth: 140
        implicitHeight: 36
        radius: 6
        color: root.hovered ? root.palette.alternateBase : root.palette.button
        border.width: root.activeFocus ? 2 : 1
        border.color: root.activeFocus ? root.palette.highlight : root.palette.mid
    }
    indicator: Text {
        width: 16
        x: root.mirrored ? 12 : root.width - width - 12
        y: (root.height - height) / 2
        text: "⌄"
        color: root.palette.buttonText
        horizontalAlignment: Text.AlignHCenter
        font.pixelSize: 18
        font.bold: true
    }
}
