import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root

    color: "#141315"
    radius: Math.max(6, Math.min(10, theme.radius))
    border.color: Qt.rgba(1, 1, 1, 0.05)
    border.width: 1
    implicitHeight: 188
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
    readonly property int cellSize: 13
    readonly property int cellSpacing: 4

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
        if (level === 0) return "#1b1a1d"
        var accent = theme.accent
        if (level === 1) return Qt.rgba(accent.r, accent.g, accent.b, 0.25)
        if (level === 2) return Qt.rgba(accent.r, accent.g, accent.b, 0.50)
        if (level === 3) return Qt.rgba(accent.r, accent.g, accent.b, 0.75)
        return accent
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 8

        // Header: Title and Hover Info
        RowLayout {
            Layout.fillWidth: true

            Text {
                text: qsTr("ACTIVITY HEATMAP")
                font.pixelSize: 10
                font.bold: true
                font.family: "monospace"
                color: "#7c777d"
            }

            Item { Layout.fillWidth: true }

            Text {
                text: root.hoveredDay
                    ? (root.hoveredDay.date + " · " + root.hoveredDay.turns + " turns · $" + root.hoveredDay.costUsd.toFixed(4) + " USD (~$" + Math.round(root.hoveredDay.costUsd * backend.trmRate).toLocaleString() + " COP) · " + backend.formatCompact(root.hoveredDay.tokensTotal) + " tok")
                    : qsTr("hover a day for stats")
                font.pixelSize: 11
                font.family: "monospace"
                color: root.hoveredDay ? theme.accent : "#584e51"
            }
        }

        // Grid Container
        RowLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignHCenter
            spacing: 6

            // Day labels
            Column {
                spacing: root.cellSpacing
                Layout.alignment: Qt.AlignTop
                Item { width: 24; height: 16 }

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
                        width: 24
                        height: root.cellSize

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: modelData.label
                            font.pixelSize: 9
                            font.family: "monospace"
                            color: "#584e51"
                            visible: modelData.show
                        }
                    }
                }
            }

            // Weeks grid
            Flickable {
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

                            Item {
                                width: root.cellSize
                                height: 16

                                Text {
                                    font.pixelSize: 9
                                    font.family: "monospace"
                                    color: "#584e51"
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
                                    radius: 2
                                    opacity: isFuture ? 0.15 : 1.0

                                    color: root.getCellColor(level)
                                    border.color: isFuture ? "transparent" : (level === 0 ? Qt.rgba(1, 1, 1, 0.03) : Qt.rgba(1, 1, 1, 0.08))
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
            spacing: 6

            Text {
                text: qsTr("pi session history")
                font.pixelSize: 10
                font.family: "monospace"
                color: "#584e51"
            }

            Item { Layout.fillWidth: true }

            Text {
                text: qsTr("less")
                font.pixelSize: 10
                font.family: "monospace"
                color: "#584e51"
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
                        border.color: Qt.rgba(1, 1, 1, 0.05)
                        border.width: 1
                    }
                }
            }

            Text {
                text: qsTr("more")
                font.pixelSize: 10
                font.family: "monospace"
                color: "#584e51"
            }
        }
    }
}
