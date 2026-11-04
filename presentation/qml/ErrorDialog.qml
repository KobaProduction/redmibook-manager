pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Dialogs

Item {
    id: root
    required property string message
    MessageDialog {
        id: dialog
        title: qsTr("Error")
        text: root.message
        buttons: MessageDialog.Ok
    }
    Component.onCompleted: dialog.open()
}
