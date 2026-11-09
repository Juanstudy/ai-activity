import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root

    property string title: ""
    property var items: []

    color: "#141315"
    radius: Math.max(6, Math.min(10, theme.radius))
    border.color: Qt.rgba(1, 1, 1, 0.05)
    border.width: 1
    Layout.fillWidth: true
    Layout.fillHeight: true

    readonly property double maxCost: {
        var m = 0.0001
        for (var i = 0; i < items.length; ++i) {
            if (items[i].costUsd > m) m = items[i].costUsd
        }
        return m
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 8

        RowLayout {
            Layout.fillWidth: true

            Text {
                text: root.title.toUpperCase()
                font.pixelSize: 9
                font.bold: true
                font.family: "monospace"
                color: "#7c777d"
            }

            Item { Layout.fillWidth: true }

            Text {
                text: root.items.length.toString()
                font.pixelSize: 9
                font.family: "monospace"
                color: "#584e51"
            }
        }

        ListView {
            id: listView
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: root.items
            spacing: 2
            boundsBehavior: Flickable.StopAtBounds

            delegate: Rectangle {
                id: rowDelegate
                width: listView.width
                height: 42
                radius: 4
                color: itemMouse.containsMouse ? "#1c1b1f" : "transparent"

                Behavior on color {
                    ColorAnimation { duration: 100 }
                }

                // Subtle proportional progress indicator
                Rectangle {
                    anchors.left: parent.left
                    anchors.bottom: parent.bottom
                    height: 1
                    width: parent.width * Math.min(1.0, (modelData.costUsd / root.maxCost))
                    color: theme.accent
                    opacity: 0.35
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    anchors.rightMargin: 8
                    spacing: 8

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 1

                        Text {
                            text: modelData.name
                            font.pixelSize: 11
                            font.bold: true
                            font.family: "monospace"
                            color: "#fafcfb"
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                        }

                        Text {
                            text: modelData.turns + " turns · " + backend.formatCompact(modelData.tokensTotal) + " tok" + (modelData.tokensReasoning > 0 ? (" · " + backend.formatCompact(modelData.tokensReasoning) + " rsn") : "")
                            font.pixelSize: 9
                            font.family: "monospace"
                            color: "#7c777d"
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                        }
                    }

                    ColumnLayout {
                        spacing: 1
                        Layout.alignment: Qt.AlignRight

                        Text {
                            text: "$" + modelData.costUsd.toFixed(4)
                            font.pixelSize: 11
                            font.bold: true
                            font.family: "monospace"
                            color: "#fafcfb"
                            Layout.alignment: Qt.AlignRight
                        }

                        Text {
                            text: "~$" + Math.round(modelData.costUsd * backend.trmRate).toLocaleString() + " COP"
                            font.pixelSize: 9
                            font.family: "monospace"
                            color: theme.accent
                            Layout.alignment: Qt.AlignRight
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
                font.pixelSize: 11
                font.family: "monospace"
                color: "#584e51"
                visible: root.items.length === 0
            }
        }
    }
}
