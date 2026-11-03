pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs as NativeDialogs
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
            Label { text: qsTr("NFC"); font.bold: true; font.pixelSize: 20; Layout.fillWidth: true }
            BusyIndicator { running: root.nfc.busy; Layout.preferredWidth: 24; Layout.preferredHeight: 24 }
            Button { text: qsTr("Refresh"); enabled: !root.nfc.busy; onClicked: root.nfc.refresh() }
        }
        Label { text: root.nfc.name; font.bold: true }
        Label { text: root.nfc.statusText; wrapMode: Text.WordWrap; Layout.fillWidth: true }
        Label { text: root.nfc.fieldText }
        Label { text: root.nfc.eventDriven ? qsTr("Live approach detection") : qsTr("Periodic NFC field observation") }
        Label { text: qsTr("Observed approaches: %1").arg(root.nfc.encounters) }
        Label { text: qsTr("NDEF: %1 bytes · available area: %2 bytes").arg(root.nfc.used).arg(root.nfc.capacity) }
        Label {
            text: qsTr("This built-in tag responds to an NFC reader, such as a phone. It does not read cards.")
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }
        Label {
            text: root.nfc.records.length > 0
                ? qsTr("Records: %1").arg(root.nfc.records.map(record => record.type).join(", "))
                : qsTr("No decoded NDEF records")
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }
        ScrollView {
            Layout.fillWidth: true
            Layout.preferredHeight: 90
            TextArea {
                text: root.nfc.contentHex
                readOnly: true
                wrapMode: TextEdit.WrapAnywhere
                selectByMouse: true
                font.family: "monospace"
                Accessible.name: qsTr("Current NDEF contents")
            }
        }
        RowLayout {
            Button { text: qsTr("Save backup"); enabled: !root.nfc.busy; onClicked: exportDialog.open() }
            Button { text: qsTr("Load backup"); enabled: root.nfc.writable; onClicked: importDialog.open() }
        }
        Label { text: qsTr("Replace tag contents"); font.bold: true }
        ChoiceBox {
            id: format
            Layout.fillWidth: true
            model: [qsTr("Text"), qsTr("Web link"), qsTr("NDEF bytes (hex)")]
            enabled: root.nfc.writable
        }
        ScrollView {
            Layout.fillWidth: true
            Layout.preferredHeight: 100
            TextArea {
                id: draft
                objectName: "nfcDraft"
                wrapMode: TextEdit.WrapAnywhere
                selectByMouse: true
                enabled: root.nfc.writable
                placeholderText: qsTr("New tag contents")
                Accessible.name: qsTr("New tag contents")
            }
        }
        Button {
            text: qsTr("Write to tag")
            enabled: root.nfc.writable
            onClicked: confirm.open()
        }
    }
    Dialog {
        id: confirm
        title: qsTr("Replace NFC tag contents?")
        standardButtons: Dialog.Ok | Dialog.Cancel
        modal: true
        anchors.centerIn: Overlay.overlay
        width: Math.min(460, root.width)
        Label {
            width: parent.width
            text: qsTr("The current NDEF records will be replaced. A backup is saved before writing. Factory and protection settings are preserved.")
            wrapMode: Text.WordWrap
        }
        onAccepted: {
            if (format.currentIndex === 0) root.nfc.writeText(draft.text)
            else if (format.currentIndex === 1) root.nfc.writeUri(draft.text)
            else root.nfc.writeHex(draft.text)
        }
    }
    NativeDialogs.FileDialog {
        id: exportDialog
        title: qsTr("Save NFC backup")
        fileMode: NativeDialogs.FileDialog.SaveFile
        defaultSuffix: "ndef"
        nameFilters: [qsTr("NDEF data (*.ndef)")]
        onAccepted: root.nfc.exportTag(selectedFile)
    }
    NativeDialogs.FileDialog {
        id: importDialog
        title: qsTr("Load NFC backup")
        fileMode: NativeDialogs.FileDialog.OpenFile
        nameFilters: [qsTr("NDEF data (*.ndef)")]
        onAccepted: {
            draft.text = root.nfc.readBackup(selectedFile)
            format.currentIndex = 2
        }
    }
}
