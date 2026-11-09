import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

RowLayout {
    id: root
    spacing: 6

    readonly property var ranges: [
        { id: "5h", label: "5h", key: Qt.Key_1 },
        { id: "12h", label: "12h", key: Qt.Key_2 },
        { id: "24h", label: "24h", key: Qt.Key_3 },
        { id: "7d", label: "7d", key: Qt.Key_4 },
        { id: "30d", label: "30d", key: Qt.Key_5 },
        { id: "6m", label: "6m", key: Qt.Key_6 },
        { id: "1y", label: "1y", key: Qt.Key_7 },
        { id: "all", label: "All", key: Qt.Key_8 }
    ]

    Repeater {
        model: root.ranges

        Button {
            id: btn
            text: modelData.label
            flat: true
            font.bold: backend.currentRange === modelData.id
            font.pixelSize: 12
            implicitHeight: 28
            implicitWidth: 44

            background: Rectangle {
                radius: Math.min(6, theme.radius)
                color: backend.currentRange === modelData.id ? theme.accent : (btn.hovered ? "#222730" : "#14171d")
                border.color: backend.currentRange === modelData.id ? theme.accent : "#2d333f"
                border.width: 1
            }

            contentItem: Text {
                text: btn.text
                font: btn.font
                color: backend.currentRange === modelData.id ? theme.accentForeground : "#c9d1d9"
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }

            onClicked: backend.setRange(modelData.id)

            Shortcut {
                sequence: (index + 1).toString()
                context: Qt.ApplicationShortcut
                onActivated: backend.setRange(modelData.id)
            }
        }
    }
}
