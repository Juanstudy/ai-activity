import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    implicitHeight: 32
    implicitWidth: rowLayout.implicitWidth + 8
    radius: 16
    color: "#161517"
    border.color: Qt.rgba(1, 1, 1, 0.07)
    border.width: 1

    readonly property var ranges: [
        { id: "5h", label: "5h" },
        { id: "12h", label: "12h" },
        { id: "24h", label: "24h" },
        { id: "7d", label: "7d" },
        { id: "30d", label: "30d" },
        { id: "6m", label: "6m" },
        { id: "1y", label: "1y" },
        { id: "all", label: "all" }
    ]

    RowLayout {
        id: rowLayout
        anchors.centerIn: parent
        spacing: 2

        Repeater {
            model: root.ranges

            Rectangle {
                id: itemRect
                implicitHeight: 24
                implicitWidth: textItem.implicitWidth + 14
                radius: 12
                color: backend.currentRange === modelData.id ? theme.accent : (mouseArea.containsMouse ? Qt.rgba(1, 1, 1, 0.05) : "transparent")

                Behavior on color {
                    ColorAnimation { duration: 120 }
                }

                Text {
                    id: textItem
                    anchors.centerIn: parent
                    text: modelData.label
                    font.pixelSize: 11
                    font.family: "monospace"
                    font.bold: backend.currentRange === modelData.id
                    color: backend.currentRange === modelData.id ? theme.accentForeground : (mouseArea.containsMouse ? "#fafcfb" : "#7c777d")
                }

                MouseArea {
                    id: mouseArea
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: backend.setRange(modelData.id)
                }

                Shortcut {
                    sequence: (index + 1).toString()
                    context: Qt.ApplicationShortcut
                    onActivated: backend.setRange(modelData.id)
                }
            }
        }
    }
}
