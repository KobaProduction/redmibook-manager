pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Redmibook.Manager

ListView {
    id: root
    required property NfcModel nfc
    property bool draftActive: true
    signal editRecord(int index)
    signal removeRecord(int index)
    clip: true
    spacing: 6
    model: draftActive ? nfc.draftRecords : nfc.records
    ScrollBar.vertical: ScrollBar {}
    delegate: Surface {
        id: entry
        required property int index
        required property var modelData
        width: ListView.view.width
        padding: 12
        implicitHeight: row.implicitHeight + topPadding + bottomPadding
        RowLayout {
            id: row
            width: parent.width
            spacing: 12
            Label { text: entry.index + 1; opacity: 0.55; Layout.alignment: Qt.AlignTop }
            ColumnLayout {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                Layout.preferredWidth: 1
                spacing: 4
                Label { text: entry.modelData.title; font.bold: true; textFormat: Text.PlainText; Layout.fillWidth: true; wrapMode: Text.WordWrap; maximumLineCount: 3; elide: Text.ElideRight }
                Label {
                    text: entry.modelData.value
                    textFormat: Text.PlainText
                    Layout.fillWidth: true
                    wrapMode: Text.WrapAnywhere
                    maximumLineCount: 2
                    elide: Text.ElideRight
                    opacity: 0.7
                }
            }
            ToolButton {
                text: entry.modelData.kind === "binary" ? qsTr("Replace") : qsTr("Edit")
                enabled: root.nfc.writable
                onClicked: root.editRecord(entry.index)
            }
            ToolButton {
                text: qsTr("Remove")
                enabled: root.nfc.writable
                onClicked: root.removeRecord(entry.index)
            }
        }
    }
    Label { anchors.centerIn: parent; visible: root.count === 0; text: qsTr("The list is empty. Add a record to begin."); wrapMode: Text.WordWrap; width: parent.width; horizontalAlignment: Text.AlignHCenter; opacity: 0.65 }
}
