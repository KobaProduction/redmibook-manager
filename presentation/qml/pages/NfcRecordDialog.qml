pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Redmibook.Manager

ModalDialog {
    id: root
    required property NfcModel nfc
    property int recordIndex: -1
    readonly property var currentRecord: recordIndex >= 0 && recordIndex < nfc.draftRecords.length
        ? nfc.draftRecords[recordIndex] : ({kind: "", title: "", value: ""})
    title: recordIndex < 0 ? qsTr("Add record") : currentRecord.kind === "binary" ? qsTr("Replace record") : qsTr("Edit record")
    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(520, parent.width - 40)
    modal: true
    function edit(index: int): void {
        recordIndex = index
        format.currentIndex = currentRecord.kind === "uri" ? 1 : 0
        value.text = index < 0 || currentRecord.kind === "binary" ? "" : currentRecord.value
        if (currentRecord.kind === "uri" && value.text.startsWith("tel:")) {
            format.currentIndex = 2
            value.text = value.text.slice(4)
        } else if (currentRecord.kind === "uri" && value.text.startsWith("mailto:")) {
            format.currentIndex = 3
            value.text = value.text.slice(7)
        }
        root.open()
    }
    function apply(): bool {
        const kinds = ["text", "uri", "phone", "email"]
        if (!nfc.putDraftRecord(recordIndex, kinds[format.currentIndex], value.text)) return false
        root.close()
        return true
    }
    ColumnLayout {
        anchors.fill: parent
        spacing: 12
        Label { text: qsTr("Record type") }
        ChoiceBox { id: format; objectName: "nfcRecordType"; Layout.fillWidth: true; model: [qsTr("Text"), qsTr("Link / URI"), qsTr("Phone number"), qsTr("Email address")] }
        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredHeight: format.currentIndex === 0 ? 130 : 70
            TextArea {
                id: value
                objectName: "nfcRecordValue"
                textFormat: TextEdit.PlainText
                wrapMode: TextEdit.WrapAnywhere
                selectByMouse: true
                placeholderText: format.currentIndex === 0 ? qsTr("Text to store") : format.currentIndex === 1 ? "https://example.org/" : format.currentIndex === 2 ? "+7…" : "name@example.org"
                Accessible.name: qsTr("Record value")
            }
        }
    }
    footer: DialogButtonBox {
        Button { text: qsTr("Cancel"); DialogButtonBox.buttonRole: DialogButtonBox.RejectRole }
        Button { text: root.recordIndex < 0 ? qsTr("Add") : qsTr("Apply"); enabled: value.text.length > 0; DialogButtonBox.buttonRole: DialogButtonBox.ActionRole; onClicked: root.apply() }
        onRejected: root.close()
    }
}
