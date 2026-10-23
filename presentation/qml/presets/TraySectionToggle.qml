pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import ".."

Surface {
    id: root
    required property string label
    required property bool value
    signal toggled(bool enabled)
    padding: 12
    NeutralCheckBox {
        width: parent.width
        text: root.label
        font.pixelSize: 14
        checked: root.value
        Accessible.name: root.label
        onClicked: root.toggled(checked)
    }
}
