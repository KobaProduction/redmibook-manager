pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts

Item {
    id: root
    required property var preferenceConnection
    property bool failed: false
    signal retryRequested()
    objectName: "managerLoadingSkeleton"
    readonly property bool connectionFailed: !preferenceConnection.connecting && !preferenceConnection.available

    // Keep startup independent of Qt Quick Controls until preferences are loaded.
    Canvas {
        id: spinner
        anchors.centerIn: parent
        width: 48
        height: 48
        visible: root.visible && !root.failed && !root.connectionFailed
        property color strokeColor: root.Window.window.palette.windowText
        onStrokeColorChanged: requestPaint()
        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()
            ctx.lineWidth = 3
            ctx.lineCap = "round"
            ctx.strokeStyle = strokeColor
            ctx.beginPath()
            ctx.arc(width / 2, height / 2, width / 2 - 4, 0, Math.PI * 1.5)
            ctx.stroke()
        }
        RotationAnimator on rotation {
            from: 0
            to: 360
            duration: 900
            loops: Animation.Infinite
            running: spinner.visible
        }
    }

    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(parent.width - 48, 420)
        spacing: 16
        visible: root.failed || root.connectionFailed

        Text {
            Layout.fillWidth: true
            text: root.failed ? qsTr("Could not load the interface") : root.preferenceConnection.connectionText
            color: root.Window.window.palette.windowText
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
        }
        Text {
            Layout.alignment: Qt.AlignHCenter
            text: "<a href='retry'>" + qsTr("Retry connection") + "</a>"
            linkColor: root.Window.window.palette.windowText
            activeFocusOnTab: true
            onLinkActivated: root.retryRequested()
            Keys.onReturnPressed: root.retryRequested()
        }
    }
}
