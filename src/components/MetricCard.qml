import QtQuick
import QtQuick.Layouts

Rectangle {
    id: root

    property string title: ""
    property string primaryValue: ""
    property string secondaryValue: ""
    property string icon: ""
    property string badgeText: ""
    property bool isHero: false
    property bool accentBorder: false
    property bool showBar: false
    property real barRatio: 0.5

    color: accentBorder ? "#17151a" : "#141315"
    radius: Math.max(theme.dp(8), Math.min(theme.dp(12), theme.radius))
    border.color: {
        if (accentBorder) {
            var c = Qt.color(theme.accent)
            return Qt.rgba(c.r, c.g, c.b, 0.30)
        }
        return Qt.rgba(1, 1, 1, 0.05)
    }
    border.width: accentBorder ? 1.5 : 1

    implicitHeight: theme.dp(136)
    Layout.fillWidth: true

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: theme.dp(18)
        spacing: theme.dp(8)

        // Header Row: Icon + Title + Optional Badge
        RowLayout {
            Layout.fillWidth: true
            spacing: theme.dp(6)

            Text {
                text: root.icon
                font.pixelSize: theme.dp(13)
                color: root.accentBorder ? theme.accent : "#7c777d"
                visible: root.icon.length > 0
            }

            Text {
                text: root.title.toUpperCase()
                font.pixelSize: theme.dp(11)
                font.bold: true
                font.family: "monospace"
                color: root.accentBorder ? theme.accent : "#7c777d"
                elide: Text.ElideRight
            }

            Item { Layout.fillWidth: true }

            Rectangle {
                visible: root.badgeText.length > 0
                implicitHeight: theme.dp(20)
                implicitWidth: badgeLabel.implicitWidth + theme.dp(12)
                radius: theme.dp(10)
                color: {
                    if (root.accentBorder) {
                        var c = Qt.color(theme.accent)
                        return Qt.rgba(c.r, c.g, c.b, 0.15)
                    }
                    return Qt.rgba(1, 1, 1, 0.06)
                }
                border.color: {
                    if (root.accentBorder) {
                        var cb = Qt.color(theme.accent)
                        return Qt.rgba(cb.r, cb.g, cb.b, 0.35)
                    }
                    return Qt.rgba(1, 1, 1, 0.08)
                }
                border.width: 1

                Text {
                    id: badgeLabel
                    anchors.centerIn: parent
                    text: root.badgeText
                    font.pixelSize: theme.dp(10)
                    font.bold: true
                    font.family: "monospace"
                    color: root.accentBorder ? theme.accent : "#a09aa3"
                }
            }
        }

        // Primary Value
        Text {
            text: root.primaryValue
            font.pixelSize: root.isHero ? theme.dp(34) : theme.dp(28)
            font.bold: true
            font.family: "monospace"
            color: "#fafcfb"
            elide: Text.ElideRight
            Layout.fillWidth: true
        }

        // Optional Ratio Bar
        Rectangle {
            visible: root.showBar
            Layout.fillWidth: true
            implicitHeight: theme.dp(4)
            radius: theme.dp(2)
            color: Qt.rgba(1, 1, 1, 0.08)
            clip: true

            Rectangle {
                height: parent.height
                width: parent.width * Math.min(1.0, Math.max(0.0, root.barRatio))
                radius: theme.dp(2)
                color: theme.accent
            }
        }

        // Secondary Metric / Subtitle
        Text {
            text: root.secondaryValue
            font.pixelSize: theme.dp(12)
            font.family: "monospace"
            color: "#8e888f"
            visible: text.length > 0
            elide: Text.ElideRight
            Layout.fillWidth: true
        }
    }
}
