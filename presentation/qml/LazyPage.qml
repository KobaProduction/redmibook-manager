pragma ComponentBehavior: Bound
import QtQuick

Loader {
    id: root
    required property bool selected
    required property url pageSource
    required property var pageProperties
    property bool completed: false
    property bool requested: false
    function loadSelected() {
        if (!completed || !selected || requested)
            return
        requested = true
        setSource(pageSource, pageProperties)
    }
    onSelectedChanged: loadSelected()
    Component.onCompleted: {
        completed = true
        loadSelected()
    }
}
