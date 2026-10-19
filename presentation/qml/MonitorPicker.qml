pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import Redmibook.Manager
Control {
    id: root
    required property ManagerModel manager
    readonly property var positions: [qsTr("Top left"), qsTr("Top center"), qsTr("Top right"),
        qsTr("Center left"), qsTr("Center"), qsTr("Center right"),
        qsTr("Bottom left"), qsTr("Bottom center"), qsTr("Bottom right")]
    implicitHeight: 160
    background: Rectangle {
        id: preview
        objectName: "indicatorPreview"
        width: Math.min(root.width, 300)
        height: root.height
        anchors.horizontalCenter: parent.horizontalCenter
        radius: root.manager.cornerRadius
        color: root.palette.base
        border.color: root.palette.mid
        border.width: 2
        Repeater {
            model: 9
            ToolButton {
                id: dot
                required property int index
                objectName: "indicatorPosition" + index
                width: 36
                height: 36
                x: 8 + (preview.width - 52) * (index % 3) / 2
                y: 8 + (preview.height - 52) * Math.floor(index / 3) / 2
                checked: root.manager.osdPositionIndex === index
                Accessible.name: root.positions[index]
                ToolTip.visible: hovered
                ToolTip.text: root.positions[index]
                background: Item {
                    Rectangle {
                        anchors.centerIn: parent
                        width: dot.checked ? 14 : 10
                        height: width
                        radius: width / 2
                        color: dot.checked ? root.palette.highlight : root.palette.mid
                        opacity: dot.hovered || dot.checked ? 1 : 0.65
                    }
                }
                onClicked: {
                    root.manager.setOsdScreen("auto")
                    root.manager.setOsdPosition(index)
                }
            }
        }
    }
}
