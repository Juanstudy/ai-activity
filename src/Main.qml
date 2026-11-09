import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

import "components"

ApplicationWindow {
    id: window
    visible: true
    width: 1140
    height: 780
    minimumWidth: 880
    minimumHeight: 620
    title: qsTr("AI Activity — Omarchy")
    color: "#0d1117"

    Material.theme: Material.Dark
    Material.accent: theme.accent
    Material.roundedScale: theme.radius

    Shortcut {
        sequence: "q"
        context: Qt.ApplicationShortcut
        onActivated: Qt.quit()
    }

    Shortcut {
        sequence: "Ctrl+Q"
        context: Qt.ApplicationShortcut
        onActivated: Qt.quit()
    }

    Shortcut {
        sequence: "r"
        context: Qt.ApplicationShortcut
        onActivated: backend.refresh()
    }

    Shortcut {
        sequence: "?"
        context: Qt.ApplicationShortcut
        onActivated: helpDialog.open()
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12

        // Top Header Row
        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            RowLayout {
                spacing: 8
                Rectangle {
                    width: 28
                    height: 28
                    radius: Math.min(6, theme.radius)
                    color: theme.accent

                    Text {
                        anchors.centerIn: parent
                        text: "󰄧"
                        font.pixelSize: 16
                        color: theme.accentForeground
                    }
                }

                ColumnLayout {
                    spacing: 0
                    Text {
                        text: "AI Activity"
                        font.pixelSize: 15
                        font.bold: true
                        color: "#f0f6fc"
                    }
                    Text {
                        text: backend.lastSyncStatus
                        font.pixelSize: 10
                        color: "#8b949e"
                    }
                }
            }

            Item { Layout.fillWidth: true }

            // Range buttons (1-8)
            TimeRangeBar {
                id: rangeBar
            }

            // TRM Badge (clickable to sync)
            Rectangle {
                implicitHeight: 28
                implicitWidth: trmText.implicitWidth + 16
                radius: Math.min(6, theme.radius)
                color: "#161b22"
                border.color: "#30363d"
                border.width: 1

                RowLayout {
                    anchors.centerIn: parent
                    spacing: 4

                    Text {
                        id: trmText
                        text: "1 USD = " + Math.round(backend.trmRate).toLocaleString() + " COP"
                        font.pixelSize: 11
                        color: theme.accent
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    ToolTip.visible: containsMouse
                    ToolTip.text: qsTr("Click to refresh TRM rate (Auto-sync)")
                    onClicked: backend.syncTrm()
                }
            }

            // Refresh button
            Button {
                text: "↻"
                implicitHeight: 28
                implicitWidth: 32
                font.bold: true
                flat: true
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Refresh session data (R)")
                onClicked: backend.refresh()

                background: Rectangle {
                    radius: Math.min(6, theme.radius)
                    color: parent.hovered ? "#222730" : "#161b22"
                    border.color: "#30363d"
                    border.width: 1
                }
                contentItem: Text {
                    text: parent.text
                    color: "#f0f6fc"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }

            // Help button
            Button {
                text: "?"
                implicitHeight: 28
                implicitWidth: 28
                flat: true
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Shortcuts (?)")
                onClicked: helpDialog.open()

                background: Rectangle {
                    radius: Math.min(6, theme.radius)
                    color: parent.hovered ? "#222730" : "#161b22"
                    border.color: "#30363d"
                    border.width: 1
                }
                contentItem: Text {
                    text: parent.text
                    color: "#8b949e"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }

        // Metrics Row
        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            MetricCard {
                title: qsTr("Total Cost")
                primaryValue: "$" + backend.totalCostUsd.toFixed(4) + " USD"
                secondaryValue: "~$" + Math.round(backend.totalCostCop).toLocaleString() + " COP"
                accentColor: theme.accent
            }

            MetricCard {
                title: qsTr("Average / Turn")
                primaryValue: "$" + backend.avgCostPerTurnUsd.toFixed(4) + " USD"
                secondaryValue: "~$" + Math.round(backend.avgCostPerTurnCop).toLocaleString() + " COP"
                accentColor: "#58a6ff"
            }

            MetricCard {
                title: qsTr("Total Tokens")
                primaryValue: backend.totalTokens.toLocaleString()
                secondaryValue: (backend.inputTokens / 1000).toFixed(1) + "k in · " + (backend.outputTokens / 1000).toFixed(1) + "k out"
                accentColor: "#3fb950"
            }

            MetricCard {
                title: qsTr("Thinking Tokens")
                primaryValue: backend.thinkingTokens.toLocaleString()
                secondaryValue: backend.totalTokens > 0 ? ((backend.thinkingTokens * 100.0 / backend.totalTokens).toFixed(1) + "% of total") : "0%"
                accentColor: "#bc8cff"
            }

            MetricCard {
                title: qsTr("Activity Volume")
                primaryValue: backend.totalTurns.toLocaleString() + " " + qsTr("turns")
                secondaryValue: backend.totalSessions + " " + qsTr("sessions")
                accentColor: "#d29922"
            }
        }

        // Activity Heatmap (GitHub Style)
        ActivityHeatmap {
            id: heatmap
        }

        // Bottom Breakdown Section: 3 Columns
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 10

            BreakdownList {
                title: qsTr("Projects / Workspaces")
                items: backend.projectBreakdown
                accentColor: theme.accent
            }

            BreakdownList {
                title: qsTr("Models & Reasoning")
                items: backend.modelBreakdown
                accentColor: "#58a6ff"
            }

            BreakdownList {
                title: qsTr("Agents & Subagents")
                items: backend.agentBreakdown
                accentColor: "#bc8cff"
            }
        }
    }

    // Keyboard Shortcuts Dialog
    Dialog {
        id: helpDialog
        anchors.centerIn: parent
        modal: true
        title: qsTr("Keyboard Shortcuts")
        standardButtons: Dialog.Close

        background: Rectangle {
            color: "#161b22"
            radius: Math.min(8, theme.radius)
            border.color: "#30363d"
            border.width: 1
        }

        ColumnLayout {
            spacing: 8
            Repeater {
                model: [
                    { key: "1 - 8", desc: qsTr("Switch time range (5h, 12h, 24h, 7d, 30d, 6m, 1y, All)") },
                    { key: "R", desc: qsTr("Refresh session data and reload logs") },
                    { key: "?", desc: qsTr("Toggle this shortcuts dialog") },
                    { key: "Q", desc: qsTr("Quit application") }
                ]

                RowLayout {
                    spacing: 14
                    Rectangle {
                        implicitWidth: 60
                        implicitHeight: 24
                        color: "#21262d"
                        radius: 4
                        Text {
                            anchors.centerIn: parent
                            text: modelData.key
                            font.bold: true
                            font.pixelSize: 11
                            color: theme.accent
                        }
                    }
                    Text {
                        text: modelData.desc
                        font.pixelSize: 12
                        color: "#c9d1d9"
                    }
                }
            }
        }
    }
}
