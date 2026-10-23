pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Redmibook.Manager
import ".."
import "../presets"

Pane {
    id: root
    required property ManagerModel manager
    required property PresetsModel presets
    signal backRequested()
    padding: 0
    ScrollView {
        id: scroll
        anchors.fill: parent
        rightPadding: 12
        contentWidth: availableWidth
        clip: true
        TraySettings { width: scroll.availableWidth; manager: root.manager; presets: root.presets }
    }
}
