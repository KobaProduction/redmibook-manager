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
    indicator: Canvas {
        id: chevron
        readonly property color stroke: root.palette.buttonText
        width: 12
        height: 8
        x: root.mirrored ? 12 : root.width - width - 12
        y: (root.height - height) / 2
        onStrokeChanged: requestPaint()
        onPaint: {
            const context = getContext("2d")
            context.reset()
            context.strokeStyle = stroke
            context.lineWidth = 1.5
            context.lineCap = "round"
            context.lineJoin = "round"
            context.beginPath()
            context.moveTo(1, 1)
            context.lineTo(6, 6)
            context.lineTo(11, 1)
            context.stroke()
        }
    }
}
