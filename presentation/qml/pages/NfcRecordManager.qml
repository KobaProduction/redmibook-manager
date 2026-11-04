pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs as NativeDialogs
import Redmibook.Manager

Item {
    id: root
    required property NfcModel nfc
    property real cornerRadius: 8
    property bool editing: false
    implicitHeight: content.implicitHeight
    function edit(): bool {
        if (editing) return true
        if (!nfc.beginDraft()) return false
        editing = true
        return true
    }
    function closeDraft(): void { editing = false; nfc.endDraft() }
    function confirmWrite(): void { saveConfirm.open() }
    function importRecords(): void { importFile.open() }
    function exportRecords(): void { exportFile.open() }
    Component.onDestruction: { if (editing && nfc) nfc.endDraft() }
    ColumnLayout {
        id: content
        anchors.fill: parent
        spacing: 10
        NfcDraftList {
            id: records
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredHeight: Math.max(64, Math.min(300, records.contentHeight))
            nfc: root.nfc
            draftActive: root.editing
            onEditRecord: index => { if (root.edit()) recordEditor.edit(index) }
            onRemoveRecord: index => { if (root.edit()) root.nfc.removeDraftRecord(index) }
        }
        RowLayout {
            Layout.fillWidth: true
            Button { text: qsTr("Add record"); enabled: root.nfc.writable; onClicked: { if (root.edit()) recordEditor.edit(-1) } }
            Item { Layout.fillWidth: true }
        }
        Label {
            visible: root.editing && (root.nfc.draftConflict || !root.nfc.draftFits)
            Layout.fillWidth: true
            text: root.nfc.draftConflict ? qsTr("The tag changed. Cancel your changes and try again.") : qsTr("Not enough space on the tag.")
            wrapMode: Text.WordWrap
        }
    }
    NfcRecordDialog {
        id: recordEditor
        objectName: "nfcRecordEditor"
        nfc: root.nfc
        cornerRadius: root.cornerRadius
        onClosed: { if (!root.nfc.draftDirty) root.closeDraft() }
    }
    ModalDialog {
        id: saveConfirm
        title: qsTr("Write records to the tag?")
        modal: true
        cornerRadius: root.cornerRadius
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(420, parent.width - 40)
        closePolicy: root.nfc.busy ? Popup.NoAutoClose : Popup.CloseOnEscape
        Label { width: parent.width; text: qsTr("The list shown here will replace the current contents of the tag."); wrapMode: Text.WordWrap }
        footer: DialogButtonBox {
            Button { text: qsTr("Cancel"); enabled: !root.nfc.busy; DialogButtonBox.buttonRole: DialogButtonBox.RejectRole }
            Button { text: qsTr("Write"); enabled: root.nfc.writable && !root.nfc.draftConflict; DialogButtonBox.buttonRole: DialogButtonBox.ActionRole; onClicked: root.nfc.saveDraft() }
            onRejected: saveConfirm.close()
        }
    }
    NativeDialogs.FileDialog { id: importFile; title: qsTr("Import records"); nameFilters: [qsTr("NDEF data (*.ndef)")]; onAccepted: { if (root.edit()) root.nfc.importDraft(selectedFile) } }
    NativeDialogs.FileDialog { id: exportFile; title: qsTr("Export records"); fileMode: NativeDialogs.FileDialog.SaveFile; defaultSuffix: "ndef"; nameFilters: [qsTr("NDEF data (*.ndef)")]; onAccepted: { if (root.editing) root.nfc.exportDraft(selectedFile); else root.nfc.exportTag(selectedFile) } }
    Connections { target: root.nfc; function onWriteFinished(): void { saveConfirm.close(); root.closeDraft() } }
}
