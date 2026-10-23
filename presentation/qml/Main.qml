pragma ComponentBehavior: Bound
import QtQuick
import Redmibook.Manager

Window {
    id: root
    required property var preferenceConnection
    required property ManagerModel manager
    required property UpdateModel updates
    required property AboutModel about
    required property DeviceControlsModel controls
    required property PresetsModel presets
    required property PowerControlsModel powerControls
    property int page: 0
    readonly property bool contentReady: content.status === Loader.Ready
    property bool startupPresented: false
    signal contentLoaded()
    width: 980
    height: 720
    minimumWidth: 720
    minimumHeight: 520
    visible: startupPresented
    objectName: "managerWindow"
    title: qsTr("RedmiBook Manager")
    palette: AppPalette {}
    color: palette.window
    function loadContent() {
        content.setSource("ManagerContent.qml", {
            "hostWindow": root, "preferenceConnection": root.preferenceConnection,
            "manager": root.manager, "updates": root.updates, "about": root.about,
            "controls": root.controls, "presets": root.presets, "powerControls": root.powerControls
        })
    }
    Loader {
        id: content
        anchors.fill: parent
        asynchronous: true
        visible: status === Loader.Ready
        onLoaded: root.contentLoaded()
    }
    LoadingSkeleton {
        anchors.fill: parent
        visible: !root.contentReady
        preferenceConnection: root.preferenceConnection
        failed: content.status === Loader.Error
        onRetryRequested: {
            if (content.status === Loader.Error) root.loadContent()
            else root.preferenceConnection.reconnect()
        }
    }
}
