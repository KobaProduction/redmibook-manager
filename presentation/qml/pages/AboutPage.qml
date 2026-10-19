import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Redmibook.Manager
import ".."
Pane {
    id: root
    required property ManagerModel manager
    required property UpdateModel updates
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
            Label { text: qsTr("About"); font.pixelSize: 26; font.bold: true }
            RowLayout {
                Label { text: "RedmiBook Manager"; font.pixelSize: 22; font.bold: true }
                Label { text: root.updates.version; color: palette.placeholderText }
                Label {
                    text: "· " + root.updates.build
                    color: palette.placeholderText
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                    HoverHandler { id: buildHover }
                    ToolTip.visible: buildHover.hovered
                    ToolTip.text: root.updates.build
                }
            }
            Label { text: qsTr("A laptop manager for Windows and Linux."); Layout.fillWidth: true; wrapMode: Text.WordWrap }
            RowLayout {
                Layout.fillWidth: true
                Label { text: qsTr("Developer: %1").arg("Koba Production"); font.pixelSize: 16; Layout.fillWidth: true }
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
                    Label { text: qsTr("Device profile · Demo"); font.pixelSize: 18; font.bold: true }
                    Label { text: qsTr("Sample data, not detected hardware."); color: palette.placeholderText }
                    GridLayout {
                        columns: 2
                        Layout.fillWidth: true
                        columnSpacing: 24
                        Label { text: qsTr("Laptop") }
                        Label { text: root.about.model; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                        Label { text: qsTr("BIOS vendor") }
                        Label { text: root.about.biosVendor }
                        Label { text: qsTr("BIOS version") }
                        Label { text: root.about.biosVersion }
                        Label { text: qsTr("Embedded controller") }
                        Label { text: root.about.controller }
                        Label { text: qsTr("BIOS flash chip") }
                        Label { text: root.about.firmwareChip }
                    }
                }
            }
            Surface {
                cornerRadius: root.manager.cornerRadius
                Layout.fillWidth: true
                ColumnLayout {
                    width: parent.width
                    spacing: 12
                    Label { text: qsTr("Updates"); font.pixelSize: 18; font.bold: true }
                    Label { objectName: "updateStatus"; text: root.updates.status; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                    RowLayout {
                        Button { objectName: "checkUpdatesButton"; text: qsTr("Check for updates"); enabled: !root.updates.busy; onClicked: root.updates.check() }
                        Button { text: root.updates.directDownload ? qsTr("Download for %1").arg(root.updates.target) : qsTr("View release"); visible: root.updates.available; onClicked: Qt.openUrlExternally(root.updates.downloadUrl) }
                    }
                    CheckBox { text: qsTr("Check for updates daily"); checked: root.manager.autoUpdates; onClicked: root.manager.setAutoUpdates(checked) }
                }
            }
        }
    }
    LicenseDialog { id: licenses; parent: Overlay.overlay; about: root.about; manager: root.manager; palette: root.palette }
}
