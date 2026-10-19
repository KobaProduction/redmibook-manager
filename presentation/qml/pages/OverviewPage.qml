import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Redmibook.Manager
import ".."

Pane {
    id: root
    padding: 0
    required property ManagerModel manager
    property bool diagnostics: false
    ColumnLayout {
        anchors.fill: parent
        spacing: 18
        Label {
            text: root.diagnostics ? qsTr("Diagnostics") : qsTr("Background components")
            font.pixelSize: 22
            font.bold: true
        }
        Surface {
            cornerRadius: root.manager.cornerRadius
            Layout.fillWidth: true
            ColumnLayout {
                anchors.fill: parent
                spacing: 14
                Label {
                    text: root.manager.runtimeStatus
                    font.bold: true
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
                Label {
                    text: root.manager.daemonDetails
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
                RowLayout {
                    Button {
                        text: qsTr("Refresh status")
                        enabled: !root.manager.busy
                        onClicked: root.manager.refresh()
                    }
                    Button {
                        text: qsTr("Open tray")
                        enabled: !root.manager.busy
                        onClicked: root.manager.openTray()
                    }
                }
            }
        }
        Label {
            text: qsTr("Closing Manager or its tray does not stop the session runtime. Diagnostic signals do not change hardware settings.")
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }
        Label {
            text: qsTr("Battery control, Mi-key capture and performance switching are not implemented yet.")
            wrapMode: Text.WordWrap
            color: palette.placeholderText
            Layout.fillWidth: true
        }
        Item { Layout.fillHeight: true }
    }
}
