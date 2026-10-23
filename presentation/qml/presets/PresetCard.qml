pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Redmibook.Manager
import ".."

Surface {
    id: root
    required property var preset
    required property PresetsModel presets
    required property bool first
    required property bool last
    signal editRequested(var button)
    padding: 16
    ColumnLayout {
        width: parent.width
        spacing: 16
        RowLayout {
            Layout.fillWidth: true
            spacing: 12
            Label {
                text: root.preset.title
                font.pixelSize: 17
                font.weight: Font.DemiBold
                elide: Text.ElideRight
                Layout.fillWidth: true
                Layout.minimumWidth: 0
            }
            NeutralCheckBox {
                text: qsTr("Show in tray")
                checked: root.preset.visible
                onClicked: root.presets.showPreset(root.preset.id, checked)
                Accessible.name: qsTr("Show in tray")
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Show in tray")
            }
            ToolButton {
                text: "⋯"
                font.pixelSize: 22
                Layout.preferredWidth: 32
                Layout.preferredHeight: 32
                Accessible.name: qsTr("Preset options")
                onClicked: presetMenu.popup()
                Menu {
                    id: presetMenu
                    MenuItem { text: qsTr("Rename"); onTriggered: { presetName.text = root.preset.title; renameDialog.open() } }
                    MenuSeparator {}
                    MenuItem { text: qsTr("Move preset up"); enabled: !root.first; onTriggered: root.presets.movePreset(root.preset.id, -1) }
                    MenuItem { text: qsTr("Move preset down"); enabled: !root.last; onTriggered: root.presets.movePreset(root.preset.id, 1) }
                    MenuSeparator {}
                    MenuItem { text: qsTr("Delete preset"); onTriggered: root.presets.removePreset(root.preset.id) }
                }
            }
        }
        Surface {
            Layout.fillWidth: true
            padding: 12
            cornerRadius: root.cornerRadius
            surfaceColor: palette.window
            GridLayout {
                width: parent.width
                columns: Math.min(4, root.preset.buttons.length)
                rowSpacing: 12
                columnSpacing: 8
                Repeater {
                    model: root.preset.buttons
                    ColumnLayout {
                        id: tile
                        required property var modelData
                        required property int index
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        spacing: 4
                        Button {
                            Layout.fillWidth: true
                            Layout.minimumWidth: 0
                            Layout.preferredWidth: 120
                            Layout.preferredHeight: 40
                            text: tile.modelData.title
                            icon.source: "qrc:/icons/" + tile.modelData.icon + ".svg"
                            onClicked: root.editRequested(tile.modelData)
                            ToolTip.visible: hovered
                            ToolTip.text: tile.modelData.title
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            Label {
                                text: qsTr("Actions: %1").arg(tile.modelData.actions.length)
                                font.pixelSize: 12
                                color: palette.placeholderText
                                Layout.fillWidth: true
                            }
                            ToolButton {
                                text: "⋯"
                                font.pixelSize: 18
                                Layout.preferredWidth: 28
                                Layout.preferredHeight: 24
                                Accessible.name: qsTr("Button options")
                                onClicked: buttonMenu.popup()
                                Menu {
                                    id: buttonMenu
                                    MenuItem { text: qsTr("Edit actions"); onTriggered: root.editRequested(tile.modelData) }
                                    MenuSeparator {}
                                    MenuItem { text: qsTr("Move button up"); enabled: tile.index > 0; onTriggered: root.presets.moveButton(root.preset.id, tile.modelData.id, -1) }
                                    MenuItem { text: qsTr("Move button down"); enabled: tile.index < root.preset.buttons.length - 1; onTriggered: root.presets.moveButton(root.preset.id, tile.modelData.id, 1) }
                                    MenuSeparator {}
                                    MenuItem { text: qsTr("Remove button"); enabled: root.preset.buttons.length > 2; onTriggered: root.presets.removeButton(root.preset.id, tile.modelData.id) }
                                }
                            }
                        }
                    }
                }
            }
        }
        Button {
            text: qsTr("Add button")
            flat: true
            enabled: root.preset.buttons.length < 8
            onClicked: root.presets.addButton(root.preset.id)
        }
    }
    Dialog {
        id: renameDialog
        parent: Overlay.overlay
        anchors.centerIn: parent
        title: qsTr("Rename preset")
        modal: true
        width: Math.min(420, parent.width - 32)
        standardButtons: Dialog.Ok | Dialog.Cancel
        TextField { id: presetName; width: parent.width; maximumLength: 128 }
        onAccepted: if (presetName.text.trim().length > 0) root.presets.renamePreset(root.preset.id, presetName.text)
    }
}
