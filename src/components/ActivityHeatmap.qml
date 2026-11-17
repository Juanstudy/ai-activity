import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root

    color: "#141315"
    radius: Math.max(theme.dp(6), Math.min(theme.dp(10), theme.radius))
    border.color: Qt.rgba(1, 1, 1, 0.05)
    border.width: 1
    implicitHeight: contentCol.implicitHeight + theme.dp(28)
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

    readonly property int weeksCount: 53
    readonly property int cellSpacing: theme.dp(5)
    readonly property int minCellSize: theme.dp(18)
    readonly property int cellSize: {
        if (!gridFlickable || gridFlickable.width <= 0)
            return minCellSize
        var available = gridFlickable.width - (weeksCount - 1) * cellSpacing
        var computed = Math.floor(available / weeksCount)
        return Math.max(minCellSize, computed)
    }

    readonly property date today: new Date()
    readonly property date startDate: {
        var d = new Date()
        var dayOfWeek = (d.getDay() + 6) % 7 // Monday = 0
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
        if (level === 0) return "#18171a"
        var c = Qt.color(theme.accent)
        if (level === 1) return Qt.rgba(c.r * 0.32, c.g * 0.32, c.b * 0.32, 1.0)
        if (level === 2) return Qt.rgba(c.r * 0.52, c.g * 0.52, c.b * 0.52, 1.0)
        if (level === 3) return Qt.rgba(c.r * 0.75, c.g * 0.75, c.b * 0.75, 1.0)
        return c
    }

    function getCellBorderColor(level) {
        if (level === 0) return Qt.rgba(1, 1, 1, 0.04)
        var c = Qt.color(theme.accent)
        return Qt.rgba(c.r, c.g, c.b, 0.20 + level * 0.15)
    }

    ColumnLayout {
        id: contentCol
        anchors.fill: parent
        anchors.margins: theme.dp(14)
        spacing: theme.dp(8)

        // Header: Title and Hover Info
        RowLayout {
            Layout.fillWidth: true

            RowLayout {
                spacing: theme.dp(8)
                Rectangle {
                    width: theme.dp(8)
                    height: theme.dp(8)
                    radius: theme.dp(2)
                    color: theme.accent
                }

                Text {
                    text: qsTr("ACTIVITY HEATMAP")
                    font.pixelSize: theme.dp(11)
                    font.bold: true
                    font.family: "monospace"
                    color: "#fafcfb"
                }
            }

            Item { Layout.fillWidth: true }

            Text {
                text: root.hoveredDay
                    ? (root.hoveredDay.date + " · " + root.hoveredDay.turns + " turns · $" + root.hoveredDay.costUsd.toFixed(4) + " USD (~$" + Math.round(root.hoveredDay.costUsd * backend.trmRate).toLocaleString() + " COP) · " + backend.formatCompact(root.hoveredDay.tokensTotal) + " tok")
                    : qsTr("hover a day for stats")
                font.pixelSize: theme.dp(12)
                font.family: "monospace"
                font.bold: root.hoveredDay ? true : false
                color: root.hoveredDay ? theme.accent : "#584e51"
            }
        }

        // Grid Container
        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: theme.dp(20) + (root.cellSize + root.cellSpacing) * 7
            spacing: theme.dp(8)

            // Day labels
            Column {
                spacing: root.cellSpacing
                Layout.alignment: Qt.AlignTop
                Item { width: theme.dp(30); height: theme.dp(20) }

                Repeater {
                    model: [
                        { label: "Mon", show: true },
                        { label: "", show: false },
                        { label: "Wed", show: true },
                        { label: "", show: false },
                        { label: "Fri", show: true },
                        { label: "", show: false },
                        { label: "", show: false }
                    ]

                    Item {
                        width: theme.dp(30)
                        height: root.cellSize

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: modelData.label
                            font.pixelSize: theme.dp(10)
                            font.family: "monospace"
                            color: "#584e51"
                            visible: modelData.show
                        }
                    }
                }
            }

            // Weeks grid
            Flickable {
                id: gridFlickable
                Layout.fillWidth: true
                Layout.preferredHeight: theme.dp(20) + (root.cellSize + root.cellSpacing) * 7
                contentWidth: root.weeksCount * (root.cellSize + root.cellSpacing)
                contentHeight: height
                clip: true
                boundsBehavior: Flickable.StopAtBounds

                Row {
                    // Center the grid only while it fits the viewport; otherwise keep it
                    // flush left so flicking can reveal the remaining weeks.
                    x: {
                        var gridWidth = root.weeksCount * root.cellSize + (root.weeksCount - 1) * root.cellSpacing
                        return gridFlickable.width > gridWidth ? (gridFlickable.width - gridWidth) / 2 : 0
                    }
                    spacing: root.cellSpacing

                    Repeater {
                        model: root.weeksCount

                        Column {
                            id: weekCol
                            readonly property int weekIdx: index
                            spacing: root.cellSpacing

                            Item {
                                width: root.cellSize
                                height: theme.dp(20)

                                Text {
                                    anchors.centerIn: parent
                                    font.pixelSize: theme.dp(10)
                                    font.family: "monospace"
                                    color: "#7c777d"
                                    visible: {
                                        var d = root.getDateForCell(weekIdx, 0)
                                        return d.getDate() <= 7
                                    }
                                    text: {
                                        var d = root.getDateForCell(weekIdx, 0)
                                        return d.toLocaleString(Qt.locale(), "MMM").toLowerCase()
                                    }
                                }
                            }

                            Repeater {
                                model: 7

                                Rectangle {
                                    readonly property int dayIdx: index
                                    readonly property date cellDate: root.getDateForCell(weekCol.weekIdx, dayIdx)
                                    readonly property string dateStr: root.formatDate(cellDate)
                                    readonly property bool isFuture: cellDate > root.today
                                    readonly property var dayData: root.dayLookup[dateStr] || { turns: 0, costUsd: 0, tokensTotal: 0, tokensReasoning: 0 }
                                    readonly property int level: isFuture ? 0 : root.getLevel(dayData.turns)

                                    width: root.cellSize
                                    height: root.cellSize
                                    radius: Math.max(2, Math.round(root.cellSize * 0.16))
                                    opacity: isFuture ? 0.15 : 1.0

                                    color: root.getCellColor(level)
                                    border.color: isFuture ? "transparent" : root.getCellBorderColor(level)
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

        // Footer: Legend
        RowLayout {
            Layout.fillWidth: true
            spacing: theme.dp(8)

            Text {
                text: qsTr("pi session history")
                font.pixelSize: theme.dp(11)
                font.family: "monospace"
                color: "#584e51"
            }

            Item { Layout.fillWidth: true }

            Text {
                text: qsTr("less")
                font.pixelSize: theme.dp(11)
                font.family: "monospace"
                color: "#584e51"
            }

            Row {
                spacing: theme.dp(4)
                Repeater {
                    model: 5
                    Rectangle {
                        width: theme.dp(12)
                        height: theme.dp(12)
                        radius: theme.dp(2)
                        color: root.getCellColor(index)
                        border.color: root.getCellBorderColor(index)
                        border.width: 1
                    }
                }
            }

            Text {
                text: qsTr("more")
                font.pixelSize: theme.dp(11)
                font.family: "monospace"
                color: "#584e51"
            }
        }
    }
}
