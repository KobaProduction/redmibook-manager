pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Redmibook.Manager
import ".."

Pane {
    id: root
    property bool preferencesAvailable: true
    required property ManagerModel manager
    required property DeviceControlsModel controls
    required property PresetsModel presets
    padding: 0
    onPreferencesAvailableChanged: { if (!preferencesAvailable) resetDialog.close() }
    Dialog {
        id: resetDialog
        objectName: "resetSettingsDialog"
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(480, parent.width - 32)
        implicitHeight: 340
        height: Math.min(implicitHeight, parent.height - 32)
        enabled: root.preferencesAvailable
        modal: true
        title: qsTr("Reset settings?")
        background: Surface { cornerRadius: root.manager.cornerRadius; implicitWidth: 0; implicitHeight: 0 }
        contentItem: Item {
            implicitHeight: resetMessage.implicitHeight
            Text {
                id: resetMessage
                width: parent.width
                text: qsTr("Reset language, appearance, shortcuts and tray settings? Custom presets will be removed. Screen refresh rate and its configured choices will stay unchanged. On supported hardware, the charge limit will be restored to 100% first. If that cannot be confirmed, settings will not be reset.")
                color: root.palette.windowText
                font: resetDialog.font
                wrapMode: Text.WordWrap
            }
        }
        footer: DialogButtonBox {
            implicitHeight: Math.max(resetAccept.implicitHeight, resetCancel.implicitHeight) + topPadding + bottomPadding
            Button { id: resetAccept; text: qsTr("Reset"); DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole }
            Button { id: resetCancel; text: qsTr("Cancel"); DialogButtonBox.buttonRole: DialogButtonBox.RejectRole }
        }
        onAccepted: { if (root.preferencesAvailable) root.controls.prepareSettingsReset() }
    }
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
            Surface {
                cornerRadius: root.manager.cornerRadius
                padding: 18
                Layout.fillWidth: true
                ColumnLayout {
                    width: parent.width
                    spacing: 10
                    Label { text: qsTr("Appearance"); font.pixelSize: 18; font.bold: true }
                    Label { text: qsTr("Language / Язык") }
                    ChoiceBox {
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
                    CheckBox {
                        text: qsTr("Show on-screen indicator")
                        checked: root.manager.osdEnabled
                        onClicked: root.manager.setOsdEnabled(checked)
                    }
                    MonitorPicker { manager: root.manager; Layout.fillWidth: true; enabled: root.manager.osdEnabled }
                }
            }
            Surface {
                cornerRadius: root.manager.cornerRadius
                padding: 18
                Layout.fillWidth: true
                ColumnLayout {
                    width: parent.width
                    spacing: 10
                    Label { text: qsTr("Sound"); font.pixelSize: 18; font.bold: true }
                    CheckBox {
                        text: qsTr("Play a sound when switching device modes")
                        checked: root.manager.osdSound
                        onClicked: root.manager.setOsdSound(checked)
                    }
                }
            }
            Surface {
                cornerRadius: root.manager.cornerRadius
                Layout.fillWidth: true
                ColumnLayout {
                    width: parent.width
                    spacing: 12
                    Label { text: qsTr("Tray settings"); font.pixelSize: 18; font.bold: true }
                    TraySettings { manager: root.manager; presets: root.presets; Layout.fillWidth: true }
                }
            }
            Surface {
                cornerRadius: root.manager.cornerRadius
                padding: 18
                Layout.fillWidth: true
                RowLayout {
                    width: parent.width
                    spacing: 12
                    Label { text: qsTr("Default settings"); font.pixelSize: 18; font.bold: true; Layout.fillWidth: true }
                    Button {
                        objectName: "resetSettingsButton"
                        text: qsTr("Reset settings")
                        enabled: !root.controls.busy
                        onClicked: resetDialog.open()
                    }
                }
            }
        }
    }
}
