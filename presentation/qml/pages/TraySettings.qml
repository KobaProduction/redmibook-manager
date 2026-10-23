pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Redmibook.Manager
import ".."
import "../presets"

ColumnLayout {
    id: root
    required property ManagerModel manager
    required property PresetsModel presets
    spacing: 16
    PresetButtonEditor { id: editor; presets: root.presets; cornerRadius: root.manager.cornerRadius; parent: Overlay.overlay }
    GridLayout {
        Layout.fillWidth: true
        columns: width >= 620 ? 2 : 1
        columnSpacing: 8
        rowSpacing: 8
        Repeater {
            model: root.manager.traySections
            TraySectionToggle {
                required property var modelData
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                cornerRadius: root.manager.cornerRadius
                label: modelData.label
                value: modelData.enabled
                onToggled: enabled => root.manager.setTraySection(modelData.id, enabled)
            }
        }
    }
    RowLayout {
        Layout.fillWidth: true
        Layout.topMargin: 8
        Label { text: qsTr("Custom presets"); font.pixelSize: 18; font.weight: Font.DemiBold; Layout.fillWidth: true }
        Button { text: qsTr("Create preset"); enabled: root.presets.items.length < 16; onClicked: root.presets.addPreset() }
    }
    Label { text: qsTr("No presets yet. Create one to combine several actions."); visible: root.presets.items.length === 0; wrapMode: Text.WordWrap; Layout.fillWidth: true; color: palette.placeholderText }
    Repeater {
        model: root.presets.items
        PresetCard {
            required property var modelData
            required property int index
            Layout.fillWidth: true
            cornerRadius: root.manager.cornerRadius
            preset: modelData
            presets: root.presets
            first: index === 0
            last: index === root.presets.items.length - 1
            onEditRequested: button => editor.edit(modelData, button)
        }
    }
}
