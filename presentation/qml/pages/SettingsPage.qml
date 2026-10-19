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
    ScrollView {
        id: scroll
        objectName: "settingsScroll"
        anchors.fill: parent
        rightPadding: 20
        clip: true
        contentWidth: availableWidth
        ScrollBar.vertical: ScrollBar {
            parent: scroll
            x: scroll.width - width
            y: 0
            height: scroll.height
            policy: ScrollBar.AsNeeded
        }
        ColumnLayout {
            width: scroll.availableWidth
            spacing: 16
            Label { text: qsTr("Settings"); font.pixelSize: 26; font.bold: true }
            Label {
                visible: root.manager.preferenceError
                text: root.manager.runtimeStatus
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
            }
            Surface {
                cornerRadius: root.manager.cornerRadius
                padding: 18
                Layout.fillWidth: true
                ColumnLayout {
                    width: parent.width
                    spacing: 10
                    Label { text: qsTr("Appearance"); font.pixelSize: 18; font.bold: true }
                    Label { text: qsTr("Language / Язык") }
                    ComboBox {
                        objectName: "languageSelector"
                        model: [qsTr("System"), "English", "Русский"]
                        Accessible.name: qsTr("Interface language")
                        currentIndex: root.manager.languageIndex
                        onActivated: root.manager.setLanguage(currentIndex)
                        Layout.fillWidth: true
                    }
                    Label { text: qsTr("Color scheme") }
                    RowLayout {
                        Repeater {
                            model: [qsTr("Follow system"), qsTr("Light"), qsTr("Dark")]
                            RadioButton {
                                required property int index
                                required property string modelData
                                text: modelData
                                checked: root.manager.themeIndex === index
                                onClicked: root.manager.setTheme(index)
                            }
                        }
                    }
                    Label { text: qsTr("Interface design") }
                    RowLayout {
                        Layout.fillWidth: true
                        RadioButton { text: "Fluent"; checked: root.manager.designIndex === 0; onClicked: root.manager.setDesign(0) }
                        RadioButton { text: qsTr("Classic"); checked: root.manager.designIndex === 1; onClicked: root.manager.setDesign(1) }
                        Item { Layout.fillWidth: true }
                        Item {
                            implicitWidth: applyDesign.implicitWidth
                            implicitHeight: applyDesign.implicitHeight
                            Button {
                                id: applyDesign
                                objectName: "applyDesignButton"
                                anchors.fill: parent
                                text: qsTr("Apply and restart")
                                visible: root.manager.designPending
                                onClicked: root.manager.applyDesign()
                            }
                        }
                    }
                    CheckBox {
                        objectName: "roundedCornersSwitch"
                        text: qsTr("Rounded corners")
                        checked: root.manager.roundedCorners
                        onClicked: root.manager.setRoundedCorners(checked)
                    }
                }
            }
            Surface {
                cornerRadius: root.manager.cornerRadius
                padding: 18
                Layout.fillWidth: true
                ColumnLayout {
                    width: parent.width
                    Label { text: qsTr("Startup"); font.pixelSize: 18; font.bold: true }
                    CheckBox {
                        objectName: "trayAutostartSwitch"
                        text: qsTr("Start tray when I sign in")
                        checked: root.manager.trayAutostart
                        onClicked: root.manager.setTrayAutostart(checked)
                    }
                }
            }
            Surface {
                cornerRadius: root.manager.cornerRadius
                padding: 18
                Layout.fillWidth: true
                ColumnLayout {
                    width: parent.width
                    spacing: 12
                    RowLayout {
                        Layout.fillWidth: true
                        Label { text: qsTr("On-screen indicator"); font.pixelSize: 18; font.bold: true; Layout.fillWidth: true }
                        Button {
                            objectName: "testIndicatorButton"
                            text: qsTr("Test")
                            enabled: !root.manager.busy
                            onClicked: root.manager.diagnostic()
                        }
                    }
                    MonitorPicker { manager: root.manager; Layout.fillWidth: true }
                }
            }
        }
    }
}
