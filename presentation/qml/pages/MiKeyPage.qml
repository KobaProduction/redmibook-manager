pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Redmibook.Manager
import ".."

Pane {
    id: root
    padding: 0
    required property ManagerModel manager
    ColumnLayout {
        anchors.fill: parent
        spacing: 18
        Label { text: qsTr("Mi key and shortcuts"); font.pixelSize: 26; font.bold: true }
        Surface {
            Layout.fillWidth: true
            cornerRadius: root.manager.cornerRadius
            RowLayout {
                width: parent.width
                Label { text: "Mi"; font.pixelSize: 18; font.bold: true; Layout.preferredWidth: 90 }
                Label { text: qsTr("Open Manager · Overview"); Layout.fillWidth: true; wrapMode: Text.WordWrap }
            }
        }
        Surface {
            Layout.fillWidth: true
            cornerRadius: root.manager.cornerRadius
            ColumnLayout {
                width: parent.width
                spacing: 14
                Label { text: qsTr("Function shortcuts"); font.pixelSize: 18; font.bold: true }
                RowLayout {
                    Layout.fillWidth: true
                    Label { text: "Fn + S"; font.bold: true; Layout.preferredWidth: 90 }
                    ComboBox {
                        Layout.fillWidth: true
                        model: [qsTr("Switch display refresh rate"), qsTr("Open Manager"), qsTr("Do nothing")]
                        currentIndex: root.manager.fnSAction
                        onActivated: root.manager.setFnSAction(currentIndex)
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    Label { text: "Fn + K"; font.bold: true; Layout.preferredWidth: 90 }
                    ComboBox {
                        Layout.fillWidth: true
                        model: [qsTr("Switch performance mode"), qsTr("Open Manager"), qsTr("Do nothing")]
                        currentIndex: root.manager.fnKAction
                        onActivated: root.manager.setFnKAction(currentIndex)
                    }
                }
            }
        }
        Item { Layout.fillHeight: true }
    }
}
