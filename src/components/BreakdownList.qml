import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

Rectangle {
    id: root

    property string title: ""
    property var items: []
    property color accentColor: theme.accent

    color: "#161b22"
    radius: Math.min(8, theme.radius)
    border.color: "#30363d"
    border.width: 1
    Layout.fillWidth: true
    Layout.fillHeight: true

    readonly property double maxCost: {
        var m = 0.00001
        for (var i = 0; i < items.length; ++i) {
            if (items[i].costUsd > m) m = items[i].costUsd
        }
        return m
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 10

        RowLayout {
            Layout.fillWidth: true

            Text {
                text: root.title.toUpperCase()
                font.pixelSize: 11
                font.bold: true
                color: "#8b949e"
            }

            Item { Layout.fillWidth: true }

            Text {
                text: root.items.length + " " + qsTr("items")
                font.pixelSize: 10
                color: "#6e7681"
            }
        }

        ListView {
            id: listView
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: root.items
            spacing: 8
            boundsBehavior: Flickable.StopAtBounds

            delegate: Rectangle {
                width: listView.width
                height: 52
                color: "#1c2128"
                radius: Math.min(4, theme.radius)
                border.color: "#282e38"
                border.width: 1

                // Subtle progress bar at bottom of item
                Rectangle {
                    anchors.bottom: parent.bottom
                    anchors.left: parent.left
                    height: 2
                    width: parent.width * Math.min(1.0, (modelData.costUsd / root.maxCost))
                    color: root.accentColor
                    opacity: 0.8
                }

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 2

                    RowLayout {
                        Layout.fillWidth: true

                        Text {
                            text: modelData.name
                            font.pixelSize: 12
                            font.bold: true
                            color: "#f0f6fc"
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                        }

                        Text {
                            text: "$" + modelData.costUsd.toFixed(4) + " USD"
                            font.pixelSize: 11
                            font.bold: true
                            color: "#f0f6fc"
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true

                        Text {
                            text: modelData.turns + " " + qsTr("turns") + " · " + modelData.tokensTotal.toLocaleString() + " tokens" + (modelData.tokensReasoning > 0 ? (" (" + modelData.tokensReasoning.toLocaleString() + " thinking)") : "")
                            font.pixelSize: 10
                            color: "#8b949e"
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                        }

                        Text {
                            text: "~$" + Math.round(modelData.costUsd * backend.trmRate).toLocaleString() + " COP"
                            font.pixelSize: 10
                            color: theme.accent
                        }
                    }
                }
            }

            Text {
                anchors.centerIn: parent
                text: qsTr("No activity in this time range")
                font.pixelSize: 12
                color: "#6e7681"
                visible: root.items.length === 0
            }
        }
    }
}
