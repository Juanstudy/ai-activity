import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

Rectangle {
    id: root

    property string title: ""
    property string primaryValue: ""
    property string secondaryValue: ""
    property string iconGlyph: ""
    property color accentColor: theme.accent

    color: "#161b22"
    radius: Math.min(8, theme.radius)
    border.color: "#30363d"
    border.width: 1

    implicitHeight: 88
    Layout.fillWidth: true

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 4

        RowLayout {
            Layout.fillWidth: true
            spacing: 6

            Text {
                text: root.title.toUpperCase()
                font.pixelSize: 10
                font.bold: true
                color: "#8b949e"
                Layout.fillWidth: true
                elide: Text.ElideRight
            }

            Rectangle {
                width: 6
                height: 6
                radius: 3
                color: root.accentColor
            }
        }

        Text {
            text: root.primaryValue
            font.pixelSize: 18
            font.bold: true
            color: "#f0f6fc"
            elide: Text.ElideRight
            Layout.fillWidth: true
        }

        Text {
            text: root.secondaryValue
            font.pixelSize: 11
            color: "#8b949e"
            visible: text.length > 0
            elide: Text.ElideRight
            Layout.fillWidth: true
        }
    }
}
