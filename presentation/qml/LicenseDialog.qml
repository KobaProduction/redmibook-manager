import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Redmibook.Manager
Dialog {
    id: root
    required property AboutModel about
    required property ManagerModel manager
    objectName: "licenseDialog"
    title: qsTr("Licenses")
    modal: true
    anchors.centerIn: parent
    width: Math.min(parent.width - 40, 760)
    height: Math.min(parent.height - 40, 620)
    standardButtons: Dialog.Close
    background: Rectangle {
        color: root.palette.window
        radius: root.manager.cornerRadius
        border.color: root.palette.mid
    }
    ColumnLayout {
        anchors.fill: parent
        TabBar {
            id: tabs
            Layout.fillWidth: true
            TabButton { text: qsTr("Application · MIT") }
            TabButton { text: "Qt · LGPLv3" }
            TabButton { text: qsTr("Third-party notices") }
        }
        CheckBox { id: original; text: qsTr("Show English original"); visible: tabs.currentIndex === 0 && root.about.hasTranslation }
        Label {
            text: tabs.currentIndex === 0 && root.about.hasTranslation && !original.checked ? qsTr("Unofficial translation. The English original is authoritative.") : qsTr("Dependency licenses are provided in their original language.")
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
        }
        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: availableWidth
            TextArea {
                objectName: "licenseText"
                text: tabs.currentIndex === 0 && !original.checked ? root.about.mitTranslation : root.about.licenseText(tabs.currentIndex, true)
                readOnly: true
                selectByMouse: true
                wrapMode: TextEdit.Wrap
            }
        }
    }
}
