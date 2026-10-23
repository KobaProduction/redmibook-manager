pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Redmibook.Manager
import ".."

Dialog {
    id: root
    objectName: "presetButtonEditor"
    required property PresetsModel presets
    property string presetId
    property string buttonId
    property var draftActions: []
    property string draftIcon: "balanced"
    property real cornerRadius: 8
    readonly property var icons: presets.iconChoices
    title: qsTr("Button actions")
    modal: true
    anchors.centerIn: parent
    width: Math.min(680, parent.width - 32)
    height: Math.min(560, parent.height - 32)
    padding: 16
    spacing: 12
    background: Surface { cornerRadius: root.cornerRadius; implicitWidth: 0; implicitHeight: 0 }
    header: Label {
        text: root.title
        font.pixelSize: 18
        font.bold: true
        padding: 16
        bottomPadding: 0
    }
    Overlay.modal: Rectangle {
        color: root.palette.window
        opacity: 0.65
    }
    closePolicy: Popup.CloseOnEscape
    function edit(preset, button) {
        presetId = preset.id
        buttonId = button.id
        buttonName.text = button.title
        draftIcon = button.icon
        draftActions = JSON.parse(JSON.stringify(button.actions))
        presets.refreshChoices()
        open()
    }
    function replaceAction(index, action) {
        const next = draftActions.slice()
        next[index] = action
        draftActions = next
    }
    function removeAction(index) {
        const next = draftActions.slice()
        next.splice(index, 1)
        draftActions = next
    }
    function moveAction(index, direction) {
        const next = draftActions.slice()
        const target = index + direction
        if (target < 0 || target >= next.length) return
        const temp = next[index]; next[index] = next[target]; next[target] = temp
        draftActions = next
    }
    contentItem: ScrollView {
        id: actionScroll
        clip: true
        contentWidth: availableWidth
        ColumnLayout {
            width: actionScroll.availableWidth
            spacing: 12
            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                TextField {
                    id: buttonName
                    Layout.fillWidth: true
                    Layout.minimumWidth: 100
                    Layout.preferredHeight: 36
                    placeholderText: qsTr("Button text")
                    Accessible.name: qsTr("Button text")
                    maximumLength: 128
                }
                ToolButton {
                    id: iconButton
                    objectName: "presetIconPickerButton"
                    Layout.preferredWidth: 44
                    Layout.preferredHeight: 36
                    display: AbstractButton.IconOnly
                    icon.source: "qrc:/icons/" + root.draftIcon + ".svg"
                    icon.width: 22
                    icon.height: 22
                    Accessible.name: qsTr("Choose icon")
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Choose icon")
                    onClicked: iconPicker.open()
                    Popup {
                        id: iconPicker
                        objectName: "presetIconPicker"
                        x: iconButton.width - implicitWidth
                        y: iconButton.height + 6
                        padding: 8
                        contentItem: GridLayout {
                            columns: 5
                            rowSpacing: 4
                            columnSpacing: 4
                            Repeater {
                                model: root.icons
                                ToolButton {
                                    required property var modelData
                                    Layout.preferredWidth: 40
                                    Layout.preferredHeight: 40
                                    display: AbstractButton.IconOnly
                                    icon.source: "qrc:/icons/" + modelData.id + ".svg"
                                    icon.width: 22
                                    icon.height: 22
                                    checkable: true
                                    checked: root.draftIcon === modelData.id
                                    highlighted: checked
                                    icon.color: checked ? palette.highlightedText : palette.buttonText
                                    onClicked: { root.draftIcon = modelData.id; iconPicker.close() }
                                    Accessible.name: modelData.label
                                    ToolTip.visible: hovered
                                    ToolTip.text: modelData.label
                                }
                            }
                        }
                    }
                }
            }
            Label { text: qsTr("Actions run from top to bottom"); font.bold: true }
            Label { text: qsTr("No actions. This button will do nothing."); visible: root.draftActions.length === 0; wrapMode: Text.WordWrap; Layout.fillWidth: true }
            Repeater {
                model: root.draftActions
                ActionEditor {
                    required property int index
                    required property var modelData
                    Layout.fillWidth: true
                    cornerRadius: root.cornerRadius
                    orderIndex: index
                    action: modelData
                    choices: root.presets.choices
                    first: index === 0
                    last: index === root.draftActions.length - 1
                    onEdited: value => root.replaceAction(index, value)
                    onRemoveRequested: root.removeAction(index)
                    onMoveRequested: direction => root.moveAction(index, direction)
                }
            }
            Button {
                Layout.preferredHeight: 36
                text: root.presets.loading ? qsTr("Loading available actions…") : qsTr("Add action")
                enabled: !root.presets.loading && root.presets.choices.length > 0 && root.draftActions.length < 8
                onClicked: {
                    const choice = root.presets.choices[0]
                    root.draftActions = root.draftActions.concat([{ kind: choice.kind, value: choice.value }])
                }
            }
            Label {
                visible: !root.presets.loading && root.presets.choices.length === 0
                text: qsTr("No supported actions are currently available. Existing actions are kept.")
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
        }
    }
    footer: DialogButtonBox {
        background: Item {}
        alignment: Qt.AlignRight
        padding: 12
        spacing: 8
        Button { text: qsTr("Cancel"); implicitHeight: 36; implicitWidth: 112; DialogButtonBox.buttonRole: DialogButtonBox.RejectRole }
        Button {
            text: qsTr("Save")
            implicitHeight: 36
            implicitWidth: 112
            enabled: buttonName.text.trim().length > 0
            DialogButtonBox.buttonRole: DialogButtonBox.ActionRole
            onClicked: if (root.presets.saveButton(root.presetId, { id: root.buttonId, title: buttonName.text, icon: root.draftIcon, actions: root.draftActions })) root.close()
        }
        onRejected: root.close()
    }
}
