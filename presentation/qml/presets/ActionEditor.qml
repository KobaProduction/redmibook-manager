pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ".."

Surface {
    id: root
    required property var action
    required property var choices
    required property bool first
    required property bool last
    required property int orderIndex
    signal edited(var value)
    signal removeRequested()
    signal moveRequested(int direction)
    readonly property var kinds: {
        const labels = [qsTr("Performance mode"), qsTr("Screen refresh rate"), qsTr("Windows power scheme"), qsTr("Energy saver"), qsTr("Screen brightness")]
        const result = []
        for (let kind = 0; kind < labels.length; ++kind)
            if (choices.some(item => item.kind === kind) || action.kind === kind)
                result.push({ kind: kind, label: labels[kind] })
        return result
    }
    readonly property var values: choices.filter(item => item.kind === action.kind)
    padding: 0
    background: Item {}
    RowLayout {
        width: parent.width
        spacing: 12
        Label { text: String(root.orderIndex + 1); font.pixelSize: 12; color: palette.placeholderText; Layout.preferredWidth: 16 }
        ChoiceBox {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            Layout.preferredWidth: 270
            Layout.preferredHeight: 36
            model: root.kinds
            textRole: "label"
            currentIndex: root.kinds.findIndex(item => item.kind === root.action.kind)
            onActivated: {
                const kind = root.kinds[currentIndex].kind
                const choice = root.choices.find(item => item.kind === kind)
                if (choice) root.edited({ kind: kind, value: choice.value })
            }
        }
        ChoiceBox {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            Layout.preferredWidth: 230
            Layout.preferredHeight: 36
            model: root.values
            textRole: "label"
            currentIndex: root.values.findIndex(item => item.value === root.action.value)
            displayText: currentIndex < 0 ? qsTr("Unavailable: %1").arg(root.action.value) : currentText
            onActivated: root.edited({ kind: root.action.kind, value: root.values[currentIndex].value })
        }
        ToolButton {
            text: "⋯"
            font.pixelSize: 22
            Layout.preferredWidth: 32
            Layout.preferredHeight: 36
            Accessible.name: qsTr("Action options")
            onClicked: actionMenu.popup()
            Menu {
                id: actionMenu
                MenuItem { text: qsTr("Move action up"); enabled: !root.first; onTriggered: root.moveRequested(-1) }
                MenuItem { text: qsTr("Move action down"); enabled: !root.last; onTriggered: root.moveRequested(1) }
                MenuSeparator {}
                MenuItem { text: qsTr("Remove action"); onTriggered: root.removeRequested() }
            }
        }
    }
}
