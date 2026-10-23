import QtQuick
import QtQuick.Controls
Frame {
    id: root
    property real cornerRadius: 8
    property color surfaceColor: palette.alternateBase
    padding: 18
    leftPadding: padding
    rightPadding: padding
    topPadding: padding
    bottomPadding: padding
    background: Rectangle {
        radius: root.cornerRadius
        color: root.surfaceColor
        border.color: Qt.alpha(root.palette.windowText, 0.14)
        border.width: 1
    }
}
