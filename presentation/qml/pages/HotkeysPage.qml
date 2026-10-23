pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Redmibook.Manager
import ".."

Pane {
    id: root
    required property ManagerModel manager
    padding: 0
    ColumnLayout {
        anchors.fill: parent
        spacing: 18
        Label { text: qsTr("Hotkeys"); font.pixelSize: 26; font.bold: true; Layout.fillWidth: true }
        Surface {
            Layout.fillWidth: true
            padding: 18
            cornerRadius: root.manager.cornerRadius
            ColumnLayout {
                width: parent.width
                spacing: 14
                Label { text: qsTr("Laptop key"); font.pixelSize: 18; font.bold: true }
                RowLayout {
                    Layout.fillWidth: true
                    ToolButton {
                        focusPolicy: Qt.NoFocus
                        hoverEnabled: false
                        background: Item {}
                        display: AbstractButton.IconOnly
                        leftPadding: 0
                        rightPadding: 0
                        topPadding: 0
                        bottomPadding: 0
                        Layout.alignment: Qt.AlignVCenter
                        Layout.preferredHeight: vendorKeySelector.implicitHeight
                        icon.source: "qrc:/icons/vendor-key.svg"
                        icon.color: palette.text
                        icon.width: 26
                        icon.height: 26
                        Layout.preferredWidth: 88
                        Accessible.name: qsTr("Laptop key")
                    }
                    ChoiceBox {
                        id: vendorKeySelector
                        objectName: "vendorKeyActionSelector"
                        Layout.fillWidth: true
                        model: [qsTr("Open Manager · Overview"), qsTr("Open tray dashboard"), qsTr("Do nothing")]
                        readonly property var actions: [1, 3, 2]
                        currentIndex: Math.max(0, actions.indexOf(root.manager.vendorKeyAction))
                        Accessible.name: qsTr("Laptop key action")
                        onActivated: (index) => root.manager.setVendorKeyAction(actions[index])
                    }
                }
            }
        }
        Surface {
            Layout.fillWidth: true
            padding: 18
            cornerRadius: root.manager.cornerRadius
            ColumnLayout {
                width: parent.width
                spacing: 14
                Label { text: qsTr("Function shortcuts"); font.pixelSize: 18; font.bold: true }
                RowLayout {
                    Layout.fillWidth: true
                    Label {
                        text: "Fn + S"
                        font.bold: true
                        Layout.preferredWidth: 88
                        Layout.alignment: Qt.AlignVCenter
                        horizontalAlignment: Text.AlignHCenter
                    }
                    ChoiceBox {
                        objectName: "fnSActionSelector"
                        Layout.fillWidth: true
                        model: [qsTr("Switch display refresh rate"), qsTr("Open Manager · Overview"), qsTr("Do nothing"), qsTr("Open tray dashboard")]
                        currentIndex: root.manager.fnSAction
                        Accessible.name: qsTr("Fn+S action")
                        onActivated: (index) => root.manager.setFnSAction(index)
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    Label {
                        text: "Fn + K"
                        font.bold: true
                        Layout.preferredWidth: 88
                        Layout.alignment: Qt.AlignVCenter
                        horizontalAlignment: Text.AlignHCenter
                    }
                    Item {
                        objectName: "fnKFixedBinding"
                        Layout.fillWidth: true
                        implicitHeight: fixedAction.implicitHeight
                        ChoiceBox {
                            id: fixedAction
                            objectName: "fnKFixedAction"
                            anchors.fill: parent
                            enabled: false
                            model: [qsTr("Switch performance mode")]
                            currentIndex: 0
                            Accessible.name: qsTr("Fn+K action")
                            Accessible.description: qsTr("Controlled by BIOS/firmware. Reassignment in Manager is unavailable.")
                        }
                        HoverHandler { id: fixedHover; objectName: "fixedShortcutHover" }
                        ToolTip.visible: fixedHover.hovered
                        ToolTip.text: qsTr("Controlled by BIOS/firmware. Reassignment in Manager is unavailable.")
                    }
                }
            }
        }
        Item { Layout.fillHeight: true }
    }
}
