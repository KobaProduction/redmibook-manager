pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Redmibook.Manager

Surface {
    id: root
    required property NfcModel nfc
    implicitHeight: content.implicitHeight + topPadding + bottomPadding
    ColumnLayout {
        id: content
        width: parent.width
        spacing: 12
        RowLayout {
            Layout.fillWidth: true
            spacing: 12
            Label { text: qsTr("NFC"); font.bold: true; font.pixelSize: 18 }
            Label { visible: text.length > 0; text: root.nfc.name; opacity: 0.65; Layout.fillWidth: true; horizontalAlignment: Text.AlignRight; elide: Text.ElideRight }
        }
        GridLayout {
            Layout.fillWidth: true
            columns: width >= 380 ? 4 : 2
            columnSpacing: 12
            rowSpacing: 8
            Label { text: qsTr("State"); opacity: 0.65; Layout.alignment: Qt.AlignTop }
            Label { text: root.nfc.statusText; Layout.fillWidth: true; wrapMode: Text.WordWrap }
            Label { visible: root.nfc.ready && root.nfc.contentLimit > 0; text: qsTr("Memory"); opacity: 0.65; Layout.alignment: Qt.AlignTop }
            Label { visible: root.nfc.ready && root.nfc.contentLimit > 0; text: qsTr("%1 / %2 bytes").arg(root.nfc.used).arg(root.nfc.contentLimit); Layout.fillWidth: true; wrapMode: Text.WordWrap }
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: 12
            Button { text: qsTr("Records"); enabled: root.nfc.ready && !root.nfc.busy; onClicked: editor.edit() }
            Button { visible: !root.nfc.ready; text: qsTr("Refresh"); enabled: !root.nfc.busy; onClicked: root.nfc.refresh() }
            Label { visible: root.nfc.ready; Layout.fillWidth: true; text: root.nfc.fieldText; wrapMode: Text.WordWrap; opacity: 0.65 }
            Item { visible: !root.nfc.ready; Layout.fillWidth: true }
        }
    }
    NfcRecordsDialog { id: editor; objectName: "nfcEditor"; nfc: root.nfc; cornerRadius: root.cornerRadius }
}
