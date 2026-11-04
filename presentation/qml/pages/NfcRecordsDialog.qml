pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Redmibook.Manager

ModalDialog {
    id: root
    required property NfcModel nfc
    title: qsTr("Records")
    modal: true
    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(720, parent.width - 40)
    bottomPadding: 12
    closePolicy: root.nfc.busy ? Popup.NoAutoClose : Popup.CloseOnEscape
    function edit(): void { if (manager.edit()) root.open() }
    onClosed: { if (root.nfc && manager.editing) manager.closeDraft() }
    header: Control {
        padding: root.padding
        bottomPadding: 0
        contentItem: RowLayout {
            spacing: 12
            Label { text: root.title; font.pixelSize: 20; font.bold: true; Layout.fillWidth: true }
            Label { text: qsTr("%1 / %2 bytes").arg(manager.editing ? root.nfc.draftSize : root.nfc.used).arg(root.nfc.contentLimit); opacity: 0.65 }
        }
    }
    contentItem: NfcRecordManager {
        id: manager
        nfc: root.nfc
        cornerRadius: root.cornerRadius
    }
    footer: Control {
        padding: root.padding
        topPadding: 8
        contentItem: GridLayout {
            id: footerLayout
            columns: width >= fileActions.implicitWidth + closingActions.implicitWidth + 2 * columnSpacing ? 3 : 1
            rowSpacing: 10
            columnSpacing: 12
            RowLayout {
                id: fileActions
                spacing: 8
                Button { text: qsTr("Import"); enabled: root.nfc.writable; onClicked: manager.importRecords() }
                Button { text: qsTr("Export"); enabled: root.nfc.ready && !root.nfc.busy; onClicked: manager.exportRecords() }
            }
            Item { visible: footerLayout.columns === 3; Layout.fillWidth: true }
            RowLayout {
                id: closingActions
                Layout.alignment: Qt.AlignRight
                spacing: 8
                BusyIndicator { running: root.nfc.busy; visible: running; implicitWidth: 24; implicitHeight: 24 }
                Button {
                    text: qsTr("Save to tag")
                    visible: manager.editing && root.nfc.draftDirty
                    enabled: root.nfc.writable && root.nfc.draftFits && !root.nfc.draftConflict
                    onClicked: manager.confirmWrite()
                }
                Button {
                    text: manager.editing && root.nfc.draftDirty ? qsTr("Cancel") : qsTr("Close")
                    enabled: !root.nfc.busy
                    onClicked: root.close()
                }
            }
        }
    }
}
