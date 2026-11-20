import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root

    property string title: ""
    property var items: []

    color: "#141315"
    radius: Math.max(theme.dp(6), Math.min(theme.dp(10), theme.radius))
    border.color: Qt.rgba(1, 1, 1, 0.05)
    border.width: 1
    Layout.fillWidth: true
    Layout.fillHeight: true
    implicitHeight: theme.dp(390)

    readonly property double totalCost: {
        var sum = 0.0
        for (var i = 0; i < items.length; ++i) {
            if (items[i].costUsd > 0) sum += items[i].costUsd
        }
        return sum > 0 ? sum : 0.0001
    }

    readonly property double maxCost: {
        var m = 0.0001
        for (var i = 0; i < items.length; ++i) {
            if (items[i].costUsd > m) m = items[i].costUsd
        }
        return m
    }

    function getSectionIcon() {
        if (root.title === "projects") return "󰉋"
        if (root.title === "models") return "󰚩"
        if (root.title === "agents") return "󰒋"
        return "󰄧"
    }

    function splitPath(name) {
        if (!name) return { prefix: "", basename: "" }
        var lastSlash = name.lastIndexOf('/')
        if (lastSlash !== -1) {
            return {
                prefix: name.substring(0, lastSlash + 1),
                basename: name.substring(lastSlash + 1)
            }
        }
        return {
            prefix: "",
            basename: name
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: theme.dp(14)
        spacing: theme.dp(10)

        // Section Header
        RowLayout {
            Layout.fillWidth: true
            spacing: theme.dp(8)

            Text {
                text: getSectionIcon()
                font.pixelSize: theme.dp(13)
                color: theme.accent
            }

            Text {
                text: root.title.toUpperCase()
                font.pixelSize: theme.dp(11)
                font.bold: true
                font.family: "monospace"
                color: "#7c777d"
            }

            Item { Layout.fillWidth: true }

            Rectangle {
                implicitHeight: theme.dp(18)
                implicitWidth: countText.implicitWidth + theme.dp(10)
                radius: theme.dp(9)
                color: Qt.rgba(1, 1, 1, 0.04)
                border.color: Qt.rgba(1, 1, 1, 0.06)
                border.width: 1

                Text {
                    id: countText
                    anchors.centerIn: parent
                    text: root.items.length.toString()
                    font.pixelSize: theme.dp(10)
                    font.bold: true
                    font.family: "monospace"
                    color: "#7c777d"
                }
            }
        }

        // 1px Subtle Header Divider
        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: Qt.rgba(1, 1, 1, 0.04)
        }

        ListView {
            id: listView
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: root.items
            spacing: theme.dp(4)
            boundsBehavior: Flickable.StopAtBounds

            delegate: Rectangle {
                id: rowDelegate
                width: listView.width
                height: theme.dp(54)
                radius: theme.dp(6)
                color: itemMouse.containsMouse ? "#1c1b1f" : "transparent"
                opacity: modelData.costUsd > 0 ? 1.0 : 0.40

                Behavior on color {
                    ColorAnimation { duration: 100 }
                }

                readonly property var pathObj: root.splitPath(modelData.name)
                readonly property double sharePercent: root.totalCost > 0 ? (modelData.costUsd / root.totalCost * 100) : 0

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: theme.dp(10)
                    anchors.rightMargin: theme.dp(10)
                    spacing: theme.dp(10)

                    // Left Side: Name and Badges
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: theme.dp(3)

                        // Name with dimmed prefix
                        RowLayout {
                            spacing: 0
                            Layout.fillWidth: true

                            Text {
                                text: rowDelegate.pathObj.prefix
                                font.pixelSize: theme.dp(12)
                                font.family: "monospace"
                                color: "#584e51"
                                visible: rowDelegate.pathObj.prefix.length > 0
                            }

                            Text {
                                text: rowDelegate.pathObj.basename
                                font.pixelSize: theme.dp(12)
                                font.bold: true
                                font.family: "monospace"
                                color: "#fafcfb"
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }
                        }

                        // Structured Metric Chips
                        RowLayout {
                            spacing: theme.dp(5)

                            // Tokens chip
                            Rectangle {
                                implicitHeight: theme.dp(18)
                                implicitWidth: tokTxt.implicitWidth + theme.dp(10)
                                radius: theme.dp(3)
                                color: Qt.rgba(1, 1, 1, 0.04)

                                Text {
                                    id: tokTxt
                                    anchors.centerIn: parent
                                    text: backend.formatCompact(modelData.tokensTotal) + " tok"
                                    font.pixelSize: theme.dp(9)
                                    font.family: "monospace"
                                    color: "#8e888f"
                                }
                            }

                            // Turns chip
                            Rectangle {
                                implicitHeight: theme.dp(18)
                                implicitWidth: trnTxt.implicitWidth + theme.dp(10)
                                radius: theme.dp(3)
                                color: Qt.rgba(1, 1, 1, 0.04)

                                Text {
                                    id: trnTxt
                                    anchors.centerIn: parent
                                    text: backend.formatNumber(modelData.turns) + " turns"
                                    font.pixelSize: theme.dp(9)
                                    font.family: "monospace"
                                    color: "#8e888f"
                                }
                            }

                            // Reasoning chip (if present)
                            Rectangle {
                                visible: modelData.tokensReasoning > 0
                                implicitHeight: theme.dp(18)
                                implicitWidth: rsnTxt.implicitWidth + theme.dp(10)
                                radius: theme.dp(3)
                                color: Qt.rgba(Qt.color(theme.accent).r, Qt.color(theme.accent).g, Qt.color(theme.accent).b, 0.12)

                                Text {
                                    id: rsnTxt
                                    anchors.centerIn: parent
                                    text: backend.formatCompact(modelData.tokensReasoning) + " rsn"
                                    font.pixelSize: theme.dp(9)
                                    font.family: "monospace"
                                    color: theme.accent
                                }
                            }

                            Item { Layout.fillWidth: true }
                        }
                    }

                    // Right Side: Cost, Share %, and Proportion Bar
                    ColumnLayout {
                        spacing: theme.dp(2)
                        Layout.alignment: Qt.AlignRight

                        // Cost in USD
                        Text {
                            text: "$" + modelData.costUsd.toFixed(modelData.costUsd < 0.01 && modelData.costUsd > 0 ? 4 : 2)
                            font.pixelSize: theme.dp(12)
                            font.bold: true
                            font.family: "monospace"
                            color: "#fafcfb"
                            Layout.alignment: Qt.AlignRight
                        }

                        // Share % and COP equivalent
                        RowLayout {
                            Layout.alignment: Qt.AlignRight
                            spacing: theme.dp(6)

                            Text {
                                text: rowDelegate.sharePercent.toFixed(1) + "%"
                                font.pixelSize: theme.dp(10)
                                font.bold: true
                                font.family: "monospace"
                                color: theme.accent
                            }

                            Text {
                                text: "~$" + Math.round(modelData.costUsd * backend.trmRate).toLocaleString() + " COP"
                                font.pixelSize: theme.dp(10)
                                font.family: "monospace"
                                color: "#584e51"
                            }
                        }

                        // Proportion Bar
                        Rectangle {
                            Layout.alignment: Qt.AlignRight
                            implicitWidth: theme.dp(72)
                            implicitHeight: theme.dp(3)
                            radius: theme.dp(1.5)
                            color: Qt.rgba(1, 1, 1, 0.06)
                            clip: true

                            Rectangle {
                                height: parent.height
                                width: parent.width * Math.min(1.0, Math.max(0.0, modelData.costUsd / root.maxCost))
                                radius: theme.dp(1.5)
                                color: theme.accent
                            }
                        }
                    }
                }

                MouseArea {
                    id: itemMouse
                    anchors.fill: parent
                    hoverEnabled: true
                }
            }

            Text {
                anchors.centerIn: parent
                text: qsTr("none")
                font.pixelSize: theme.dp(11)
                font.family: "monospace"
                color: "#584e51"
                visible: root.items.length === 0
            }
        }
    }
}
