import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Pane {
    id: root
    padding: 0
    required property string heading
    required property string description
    ColumnLayout {
        anchors.fill: parent
        spacing: 20
        Label {
            text: root.heading
            font.pixelSize: 22
            font.bold: true
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
        }
        Label {
            text: root.description
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
        }
        Item { Layout.fillHeight: true }
    }
}
