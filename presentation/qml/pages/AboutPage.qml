import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Redmibook.Manager
import ".."
Pane {
    id: root
    required property ManagerModel manager
    required property UpdateModel updates
    property bool preferencesAvailable: true
    required property AboutModel about
    padding: 0
    ScrollView {
        id: scroll
        anchors.fill: parent
        rightPadding: 20
        clip: true
        contentWidth: availableWidth
        ScrollBar.vertical: ScrollBar { parent: scroll; x: scroll.width - width; y: 0; height: scroll.height; policy: ScrollBar.AsNeeded }
        ColumnLayout {
            width: scroll.availableWidth
            spacing: 16
            RowLayout {
                Label { text: qsTr("About"); font.pixelSize: 26; font.bold: true }
                Label { text: "RedmiBook Manager"; font.pixelSize: 22; font.bold: true }
                Label { text: root.updates.version; color: palette.placeholderText }
            }
            RowLayout {
                Layout.fillWidth: true
                Label { text: qsTr("Developer: %1").arg("Koba Production"); Layout.fillWidth: true }
                Button { text: qsTr("GitHub repository"); onClicked: Qt.openUrlExternally("https://github.com/KobaProduction/redmibook-manager") }
            }
            Surface {
                cornerRadius: root.manager.cornerRadius
                Layout.fillWidth: true
                RowLayout {
                    width: parent.width
                    Label { text: qsTr("License"); font.pixelSize: 18; font.bold: true }
                    Label { text: "MIT"; color: palette.placeholderText; Layout.fillWidth: true }
                    Button { objectName: "licensesButton"; text: qsTr("Read more"); onClicked: licenses.open() }
                }
            }
            Surface {
                cornerRadius: root.manager.cornerRadius
                Layout.fillWidth: true
                ColumnLayout {
                    width: parent.width
                    spacing: 10
                    Label { text: qsTr("Device profile"); font.pixelSize: 18; font.bold: true }
                    GridLayout {
                        columns: 2
                        Layout.fillWidth: true
                        columnSpacing: 24
                        Label { text: qsTr("Laptop") }
                        Label { text: root.about.model || qsTr("Device information unavailable"); Layout.fillWidth: true; wrapMode: Text.WordWrap }
                        Label { text: qsTr("Manufacturer"); visible: root.about.manufacturer.length > 0 }
                        Label { text: root.about.manufacturer; visible: root.about.manufacturer.length > 0 }
                        Label { text: qsTr("Baseboard model"); visible: root.about.boardModel.length > 0 }
                        Label { text: root.about.boardModel; visible: root.about.boardModel.length > 0 }
                        Label { text: qsTr("BIOS vendor"); visible: root.about.biosVendor.length > 0 }
                        Label { text: root.about.biosVendor; visible: root.about.biosVendor.length > 0 }
                        Label { text: qsTr("BIOS version"); visible: root.about.biosVersion.length > 0 }
                        Label { text: root.about.biosVersion; visible: root.about.biosVersion.length > 0 }
                        Label { text: qsTr("BIOS release date"); visible: root.about.biosDate.length > 0 }
                        Label { text: root.about.biosDate; visible: root.about.biosDate.length > 0 }
                        Label { text: qsTr("EC firmware revision"); visible: root.about.controller.length > 0 }
                        Label { text: root.about.controller; visible: root.about.controller.length > 0 }
                    }
                }
            }
            Surface {
                cornerRadius: root.manager.cornerRadius
                Layout.fillWidth: true
                ColumnLayout {
                    width: parent.width
                    spacing: 12
                    RowLayout {
                        Layout.fillWidth: true
                        Label { text: qsTr("Background components"); font.pixelSize: 18; font.bold: true; Layout.fillWidth: true }
                        Button { text: qsTr("Refresh status"); enabled: !root.manager.busy; onClicked: root.manager.refresh() }
                    }
                    Label { text: root.manager.runtimeStatus; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                    Label { text: root.manager.daemonDetails; color: palette.placeholderText; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                }
            }
            Surface {
                cornerRadius: root.manager.cornerRadius
                Layout.fillWidth: true
                ColumnLayout {
                    width: parent.width
                    spacing: 12
                    RowLayout {
                        Layout.fillWidth: true
                        Label { objectName: "updateStatus"; text: qsTr("Updates: %1").arg(root.updates.status); font.pixelSize: 18; font.bold: true; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                        Button { objectName: "checkUpdatesButton"; text: qsTr("Check for updates"); enabled: !root.updates.busy; onClicked: root.updates.check() }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        CheckBox { enabled: root.preferencesAvailable; text: qsTr("Check daily"); font.pixelSize: 13; checked: root.manager.autoUpdates; onClicked: root.manager.setAutoUpdates(checked) }
                        Label { text: qsTr("Last checked: %1").arg(Qt.formatDateTime(root.updates.checkedAt, Qt.locale(), Locale.ShortFormat)); visible: !isNaN(root.updates.checkedAt.getTime()); font.pixelSize: 13; color: palette.placeholderText; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                        Button { text: root.updates.directDownload ? qsTr("Download for %1").arg(root.updates.target) : qsTr("View release"); visible: root.updates.available; onClicked: Qt.openUrlExternally(root.updates.downloadUrl) }
                    }
                }
            }
        }
    }
    LicenseDialog { id: licenses; parent: Overlay.overlay; about: root.about; manager: root.manager; palette: root.palette }
}
