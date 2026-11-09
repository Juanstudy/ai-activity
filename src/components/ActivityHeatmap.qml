import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

Rectangle {
    id: root

    color: "#161b22"
    radius: Math.min(8, theme.radius)
    border.color: "#30363d"
    border.width: 1
    implicitHeight: 180
    Layout.fillWidth: true

    property var dayLookup: ({})
    property int maxTurns: 1
    property var hoveredDay: null

    function rebuildLookup() {
        var map = {}
        var maxVal = 1
        var list = backend.heatmapData
        for (var i = 0; i < list.length; ++i) {
            var item = list[i]
            map[item.date] = item
            if (item.turns > maxVal)
                maxVal = item.turns
        }
        dayLookup = map
        maxTurns = maxVal
    }

    Connections {
        target: backend
        function onDataChanged() {
            root.rebuildLookup()
        }
    }

    Component.onCompleted: rebuildLookup()

    // 53 weeks x 7 days
    readonly property int weeksCount: 53
    readonly property int cellSize: 11
    readonly property int cellSpacing: 3

    readonly property date today: new Date()
    // Calculate start date: 52 weeks ago, aligned to Monday
    readonly property date startDate: {
        var d = new Date()
        var dayOfWeek = (d.getDay() + 6) % 7 // Monday = 0, Sunday = 6
        d.setDate(d.getDate() - (52 * 7 + dayOfWeek))
        d.setHours(0, 0, 0, 0)
        return d
    }

    function getDateForCell(weekIndex, dayIndex) {
        var d = new Date(startDate.getTime())
        d.setDate(d.getDate() + (weekIndex * 7 + dayIndex))
        return d
    }

    function formatDate(d) {
        var y = d.getFullYear()
        var m = (d.getMonth() + 1).toString().padStart(2, '0')
        var day = d.getDate().toString().padStart(2, '0')
        return y + "-" + m + "-" + day
    }

    function getLevel(turns) {
        if (!turns || turns <= 0) return 0
        var ratio = turns / maxTurns
        if (ratio < 0.2) return 1
        if (ratio < 0.45) return 2
        if (ratio < 0.75) return 3
        return 4
    }

    function getCellColor(level) {
        if (level === 0) return "#1c2128"
        var accent = theme.accent
        if (level === 1) return Qt.rgba(accent.r, accent.g, accent.b, 0.3)
        if (level === 2) return Qt.rgba(accent.r, accent.g, accent.b, 0.55)
        if (level === 3) return Qt.rgba(accent.r, accent.g, accent.b, 0.8)
        return accent
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 8

        // Header: Title and hovered summary
        RowLayout {
            Layout.fillWidth: true

            Text {
                text: qsTr("AI ACTIVITY HEATMAP")
                font.pixelSize: 11
                font.bold: true
                color: "#8b949e"
            }

            Item { Layout.fillWidth: true }

            Text {
                text: root.hoveredDay ? (root.hoveredDay.date + " · " + root.hoveredDay.turns + " turns · $" + root.hoveredDay.costUsd.toFixed(4) + " USD (~$" + Math.round(root.hoveredDay.costUsd * backend.trmRate).toLocaleString() + " COP)") : qsTr("Hover over a day for details")
                font.pixelSize: 11
                color: root.hoveredDay ? theme.accent : "#8b949e"
            }
        }

        // Grid container
        RowLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignHCenter
            spacing: 6

            // Day labels (Mon, Wed, Fri)
            Column {
                spacing: root.cellSpacing
                Layout.alignment: Qt.AlignTop
                Item { width: 24; height: 16 } // space for month headers

                Repeater {
                    model: [
                        { label: qsTr("Mon"), visible: true },
                        { label: "", visible: false },
                        { label: qsTr("Wed"), visible: true },
                        { label: "", visible: false },
                        { label: qsTr("Fri"), visible: true },
                        { label: "", visible: false },
                        { label: "", visible: false }
                    ]

                    Item {
                        width: 24
                        height: root.cellSize

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: modelData.label
                            font.pixelSize: 9
                            color: "#8b949e"
                            visible: modelData.visible
                        }
                    }
                }
            }

            // Weeks columns
            Flickable {
                id: gridFlickable
                Layout.fillWidth: true
                Layout.preferredHeight: 16 + (root.cellSize + root.cellSpacing) * 7
                contentWidth: root.weeksCount * (root.cellSize + root.cellSpacing)
                contentHeight: height
                clip: true
                boundsBehavior: Flickable.StopAtBounds

                Row {
                    spacing: root.cellSpacing

                    Repeater {
                        model: root.weeksCount

                        Column {
                            id: weekCol
                            readonly property int weekIdx: index
                            spacing: root.cellSpacing

                            // Month header label (only show when 1st of month falls in this week)
                            Item {
                                width: root.cellSize
                                height: 16

                                Text {
                                    id: monthText
                                    font.pixelSize: 9
                                    color: "#8b949e"
                                    visible: {
                                        var d = root.getDateForCell(weekIdx, 0)
                                        return d.getDate() <= 7
                                    }
                                    text: {
                                        var d = root.getDateForCell(weekIdx, 0)
                                        return d.toLocaleString(Qt.locale(), "MMM")
                                    }
                                }
                            }

                            // 7 Days
                            Repeater {
                                model: 7

                                Rectangle {
                                    id: dayCell
                                    readonly property int dayIdx: index
                                    readonly property date cellDate: root.getDateForCell(weekCol.weekIdx, dayIdx)
                                    readonly property string dateStr: root.formatDate(cellDate)
                                    readonly property bool isFuture: cellDate > root.today
                                    readonly property var dayData: root.dayLookup[dateStr] || { turns: 0, costUsd: 0, tokensTotal: 0, tokensReasoning: 0 }
                                    readonly property int level: isFuture ? 0 : root.getLevel(dayData.turns)

                                    width: root.cellSize
                                    height: root.cellSize
                                    radius: 2
                                    opacity: isFuture ? 0.2 : 1.0

                                    color: root.getCellColor(level)
                                    border.color: isFuture ? "transparent" : (level === 0 ? "#272e39" : Qt.darker(color, 1.2))
                                    border.width: 1

                                    MouseArea {
                                        anchors.fill: parent
                                        hoverEnabled: !parent.isFuture
                                        onEntered: {
                                            root.hoveredDay = {
                                                date: parent.dateStr,
                                                turns: parent.dayData.turns,
                                                costUsd: parent.dayData.costUsd,
                                                tokensTotal: parent.dayData.tokensTotal,
                                                tokensReasoning: parent.dayData.tokensReasoning
                                            }
                                        }
                                        onExited: {
                                            if (root.hoveredDay && root.hoveredDay.date === parent.dateStr)
                                                root.hoveredDay = null
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        // Footer: Legend "Less ... More"
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Text {
                text: qsTr("Pi Agent turns tracked locally")
                font.pixelSize: 10
                color: "#6e7681"
            }

            Item { Layout.fillWidth: true }

            Text {
                text: qsTr("Less")
                font.pixelSize: 10
                color: "#8b949e"
            }

            Row {
                spacing: 3
                Repeater {
                    model: 5
                    Rectangle {
                        width: 10
                        height: 10
                        radius: 2
                        color: root.getCellColor(index)
                        border.color: index === 0 ? "#272e39" : Qt.darker(color, 1.2)
                        border.width: 1
                    }
                }
            }

            Text {
                text: qsTr("More")
                font.pixelSize: 10
                color: "#8b949e"
            }
        }
    }
}
