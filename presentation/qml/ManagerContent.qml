pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Redmibook.Manager

Item {
    id: root
    required property Main hostWindow
    required property var preferenceConnection
    required property ManagerModel manager
    required property UpdateModel updates
    required property AboutModel about
    required property DeviceControlsModel controls
    required property PresetsModel presets
    required property PowerControlsModel powerControls
    required property NfcModel nfc
    Loader { id: errorLoader }
    function showError(message: string) {
        errorLoader.setSource("")
        errorLoader.setSource("ErrorDialog.qml", { "message": message })
    }
    Connections {
        target: root.manager
        function onChanged() {
            if (root.manager.takePreferenceError()) {
                root.showError(qsTr("Could not save preferences."))
            }
        }
        function onErrorOccurred(message: string) {
            root.showError(message)
        }
    }
    Connections {
        target: root.controls
        function onErrorOccurred(message: string) {
            root.showError(message)
        }
    }
    Connections {
        target: root.presets
        function onErrorOccurred(message: string) { root.showError(message) }
    }
    Connections {
        target: root.powerControls
        function onErrorOccurred(message: string) { root.showError(message) }
    }
    Connections {
        target: root.nfc
        function onErrorOccurred(message: string) { root.showError(message) }
    }
    Pane {
        id: connectionBanner
        visible: !root.preferenceConnection.available
        anchors.top: parent.top
        width: parent.width
        height: visible ? implicitHeight : 0
        RowLayout {
            width: parent.width
            Label { text: root.preferenceConnection.connectionText; wrapMode: Text.WordWrap; Layout.fillWidth: true }
            Button { text: qsTr("Retry connection"); onClicked: root.preferenceConnection.reconnect() }
        }
    }
    RowLayout {
        anchors.fill: parent
        anchors.topMargin: connectionBanner.height
        spacing: 0
        Pane {
            padding: 16
            Layout.preferredWidth: 180
            Layout.fillHeight: true
            ColumnLayout {
                anchors.fill: parent
                spacing: 6
                Repeater {
                    model: [qsTr("Overview"), qsTr("Device"), qsTr("Hotkeys"), qsTr("Settings"), qsTr("About")]
                    Button {
                        id: navigationButton
                        required property string modelData
                        required property int index
                        readonly property int pageIndex: [0, 3, 2, 4, 5][index]
                        objectName: "navigation" + pageIndex
                        text: modelData
                        flat: true
                        highlighted: (root.hostWindow.page === 6 ? 4 : root.hostWindow.page) === pageIndex
                        contentItem: Label {
                            text: navigationButton.text
                            font: navigationButton.font
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                            elide: Text.ElideRight
                            color: navigationButton.highlighted ? navigationButton.palette.highlightedText : navigationButton.palette.buttonText
                        }
                        Layout.fillWidth: true
                        onClicked: root.hostWindow.page = pageIndex
                    }
                }
                Item { Layout.fillHeight: true }
            }
        }
        StackLayout {
            enabled: root.preferenceConnection.available || root.hostWindow.page === 5
            currentIndex: root.hostWindow.page === 0 ? 0 : (root.hostWindow.page >= 2 && root.hostWindow.page <= 6 ? root.hostWindow.page - 1 : -1)
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: 24
            LazyPage {
                selected: root.hostWindow.page === 0
                pageSource: Qt.resolvedUrl("pages/OverviewPage.qml")
                pageProperties: ({ "manager": root.manager, "controls": root.controls, "about": root.about })
            }
            LazyPage {
                selected: root.hostWindow.page === 2
                pageSource: Qt.resolvedUrl("pages/HotkeysPage.qml")
                pageProperties: ({ "manager": root.manager })
            }
            LazyPage {
                selected: root.hostWindow.page === 3
                pageSource: Qt.resolvedUrl("pages/DevicePage.qml")
                pageProperties: ({ "manager": root.manager, "controls": root.controls, "powerControls": root.powerControls, "nfc": root.nfc })
            }
            LazyPage {
                selected: root.hostWindow.page === 4
                pageSource: Qt.resolvedUrl("pages/SettingsPage.qml")
                pageProperties: ({
                    "preferencesAvailable": Qt.binding(function() { return root.preferenceConnection.available }),
                    "manager": root.manager, "controls": root.controls, "presets": root.presets
                })
            }
            LazyPage {
                selected: root.hostWindow.page === 5
                pageSource: Qt.resolvedUrl("pages/AboutPage.qml")
                pageProperties: ({
                    "preferencesAvailable": Qt.binding(function() { return root.preferenceConnection.available }),
                    "manager": root.manager, "updates": root.updates, "about": root.about
                })
            }
            LazyPage {
                selected: root.hostWindow.page === 6
                id: trayPage
                pageSource: Qt.resolvedUrl("pages/TrayPage.qml")
                pageProperties: ({ "manager": root.manager, "presets": root.presets })
            }
        }
    }
    Connections {
        target: trayPage.item
        function onBackRequested() { root.hostWindow.page = 4 }
    }
}
