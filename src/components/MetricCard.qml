import QtQuick
import QtQuick.Layouts

Rectangle {
    id: root

    property string title: ""
    property string primaryValue: ""
    property string secondaryValue: ""

    color: "#141315"
    radius: Math.max(6, Math.min(10, theme.radius))
    border.color: Qt.rgba(1, 1, 1, 0.05)
    border.width: 1

    implicitHeight: 88
    Layout.fillWidth: true

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 4

        Text {
            text: root.title.toUpperCase()
            font.pixelSize: 10
            font.bold: true
            font.family: "monospace"
            color: "#7c777d"
            Layout.fillWidth: true
            elide: Text.ElideRight
        }

        Text {
            text: root.primaryValue
            font.pixelSize: 21
            font.bold: true
            font.family: "monospace"
            color: "#fafcfb"
            elide: Text.ElideRight
            Layout.fillWidth: true
        }

        Text {
            text: root.secondaryValue
            font.pixelSize: 11
            font.family: "monospace"
            color: "#8e888f"
            visible: text.length > 0
            elide: Text.ElideRight
            Layout.fillWidth: true
        }
    }
}
