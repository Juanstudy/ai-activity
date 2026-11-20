import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    implicitHeight: theme.dp(36)
    implicitWidth: rowLayout.implicitWidth + theme.dp(10)
    radius: theme.dp(18)
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
        spacing: theme.dp(2)

        Repeater {
            model: root.ranges

            Rectangle {
                id: itemRect
                implicitHeight: theme.dp(28)
                implicitWidth: textItem.implicitWidth + theme.dp(16)
                radius: theme.dp(14)
                color: backend.currentRange === modelData.id ? theme.accent : (mouseArea.containsMouse ? Qt.rgba(1, 1, 1, 0.05) : "transparent")

                Behavior on color {
                    ColorAnimation { duration: 120 }
                }

                Text {
                    id: textItem
                    anchors.centerIn: parent
                    text: modelData.label
                    font.pixelSize: theme.dp(12)
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
