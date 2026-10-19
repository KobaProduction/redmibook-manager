pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Redmibook.Manager
import "pages"

ApplicationWindow {
    id: root
    required property ManagerModel manager
    required property UpdateModel updates
    required property AboutModel about
    property int page: 0
    width: 980
    height: 720
    minimumWidth: 720
    minimumHeight: 520
    visible: true
    objectName: "managerWindow"
    title: qsTr("RedmiBook Manager")
    palette: AppPalette {}
    color: palette.window
    RowLayout {
        anchors.fill: parent
        spacing: 0
        Pane {
            padding: 16
            Layout.preferredWidth: 180
            Layout.fillHeight: true
            ColumnLayout {
                anchors.fill: parent
                spacing: 6
                Repeater {
                    model: [qsTr("Overview"), qsTr("Battery"), qsTr("Mi key"), qsTr("Settings"), qsTr("Diagnostics"), qsTr("About")]
                    Button {
                        required property string modelData
                        required property int index
                        objectName: "navigation" + index
                        text: modelData
                        flat: true
                        highlighted: root.page === index
                        Layout.fillWidth: true
                        onClicked: root.page = index
                    }
                }
                Item { Layout.fillHeight: true }
            }
        }
        StackLayout {
            currentIndex: root.page
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: 24
            OverviewPage { manager: root.manager }
            UnsupportedPage {
                heading: qsTr("Battery protection")
                description: qsTr("Charge limits will appear here after hardware support is verified. No firmware settings are changed by this build.")
            }
            UnsupportedPage {
                heading: qsTr("Mi key and shortcuts")
                description: qsTr("Custom bindings are not implemented yet.")
            }
            SettingsPage { manager: root.manager }
            OverviewPage { manager: root.manager; diagnostics: true }
            AboutPage { manager: root.manager; updates: root.updates; about: root.about }
        }
    }
}
