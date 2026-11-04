import QtQuick
import QtQuick.Controls

Dialog {
    id: root
    property real cornerRadius: 8
    height: parent ? Math.min(implicitHeight, parent.height - 40) : implicitHeight
    topInset: 0
    bottomInset: 0
    leftInset: 0
    rightInset: 0
    background: Rectangle {
        color: root.palette.window
        radius: root.cornerRadius
        border.width: 1
        border.color: root.palette.mid
    }
}
