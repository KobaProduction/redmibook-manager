import QtQuick
import QtQuick.Controls
Frame {
    id: root
    property real cornerRadius: 8
    padding: 18
    background: Rectangle {
        radius: root.cornerRadius
        color: root.palette.alternateBase
        border.color: root.palette.mid
        border.width: 0
    }
}
