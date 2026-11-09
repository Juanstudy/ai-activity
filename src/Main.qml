import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

import "components"

ApplicationWindow {
    id: window
    visible: true
    width: 1240
    height: 820
    minimumWidth: 960
    minimumHeight: 680
    title: "ai-activity"
    color: "#0c0b0c"

    Material.theme: Material.Dark
    Material.accent: theme.accent
    Material.roundedScale: theme.radius

    Shortcut {
        sequence: "q"
        context: Qt.ApplicationShortcut
        onActivated: Qt.quit()
    }

    Shortcut {
        sequence: "Escape"
        context: Qt.ApplicationShortcut
        onActivated: {
            if (helpDialog.visible)
                helpDialog.close()
            else
                Qt.quit()
        }
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
        anchors.margins: 14
        spacing: 10

        // ── TOP HEADER BAR ──
        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            // App Identity
            RowLayout {
                spacing: 8
                Text {
                    text: "󰄧"
                    font.pixelSize: 16
                    color: theme.accent
                }

                Text {
                    text: "ai-activity"
                    font.pixelSize: 14
                    font.bold: true
                    font.family: "monospace"
                    color: "#fafcfb"
                }

                Text {
                    text: "· " + backend.lastSyncStatus
                    font.pixelSize: 11
                    font.family: "monospace"
                    color: "#584e51"
                }
            }

            Item { Layout.fillWidth: true }

            // Range Segmented Pill
            TimeRangeBar {
                id: rangeBar
            }

            Item { Layout.fillWidth: true }

            // TRM Pill & Actions
            RowLayout {
                spacing: 6

                Rectangle {
                    implicitHeight: 30
                    implicitWidth: trmText.implicitWidth + 18
                    radius: 15
                    color: "#161517"
                    border.color: Qt.rgba(1, 1, 1, 0.06)
                    border.width: 1

                    Text {
                        id: trmText
                        anchors.centerIn: parent
                        text: "1 USD = " + Math.round(backend.trmRate).toLocaleString() + " COP"
                        font.pixelSize: 11
                        font.family: "monospace"
                        color: theme.accent
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: backend.syncTrm()
                    }
                }

                Rectangle {
                    implicitHeight: 30
                    implicitWidth: 30
                    radius: 15
                    color: refMouse.containsMouse ? "#201e22" : "#161517"
                    border.color: Qt.rgba(1, 1, 1, 0.06)
                    border.width: 1

                    Text {
                        anchors.centerIn: parent
                        text: "↻"
                        font.pixelSize: 13
                        color: "#fafcfb"
                    }

                    MouseArea {
                        id: refMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: backend.refresh()
                    }
                }

                Rectangle {
                    implicitHeight: 30
                    implicitWidth: 30
                    radius: 15
                    color: helpMouse.containsMouse ? "#201e22" : "#161517"
                    border.color: Qt.rgba(1, 1, 1, 0.06)
                    border.width: 1

                    Text {
                        anchors.centerIn: parent
                        text: "?"
                        font.pixelSize: 12
                        font.bold: true
                        color: "#7c777d"
                    }

                    MouseArea {
                        id: helpMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: helpDialog.open()
                    }
                }
            }
        }

        // ── METRICS KPI ROW ──
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            MetricCard {
                title: "total cost"
                primaryValue: "$" + backend.totalCostUsd.toFixed(4)
                secondaryValue: "~$" + Math.round(backend.totalCostCop).toLocaleString() + " COP"
            }

            MetricCard {
                title: "avg / turn"
                primaryValue: "$" + backend.avgCostPerTurnUsd.toFixed(4)
                secondaryValue: "~$" + Math.round(backend.avgCostPerTurnCop).toLocaleString() + " COP"
            }

            MetricCard {
                title: "tokens"
                primaryValue: backend.formatCompact(backend.totalTokens)
                secondaryValue: backend.formatCompact(backend.inputTokens) + " in · " + backend.formatCompact(backend.outputTokens) + " out"
            }

            MetricCard {
                title: "reasoning"
                primaryValue: backend.formatCompact(backend.thinkingTokens)
                secondaryValue: backend.totalTokens > 0 ? ((backend.thinkingTokens * 100.0 / backend.totalTokens).toFixed(1) + "% of total") : "0%"
            }

            MetricCard {
                title: "activity"
                primaryValue: backend.formatNumber(backend.totalTurns) + " turns"
                secondaryValue: backend.formatNumber(backend.totalSessions) + " sessions"
            }
        }

        // ── ACTIVITY HEATMAP ──
        ActivityHeatmap {
            id: heatmap
        }

        // ── BREAKDOWN PANELS ──
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 8

            BreakdownList {
                title: "projects"
                items: backend.projectBreakdown
            }

            BreakdownList {
                title: "models"
                items: backend.modelBreakdown
            }

            BreakdownList {
                title: "agents"
                items: backend.agentBreakdown
            }
        }

        // ── BOTTOM OMARCHY FOOTER ──
        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: 4
            spacing: 8

            Text {
                text: "󰄧 ai-activity"
                font.pixelSize: 11
                font.family: "monospace"
                color: "#584e51"
            }

            Text {
                text: "|"
                font.pixelSize: 11
                color: "#3a363d"
            }

            Text {
                text: "1-8 range · R refresh · ? shortcuts"
                font.pixelSize: 11
                font.family: "monospace"
                color: "#584e51"
            }

            Item { Layout.fillWidth: true }

            RowLayout {
                spacing: 6

                Text {
                    text: "Close"
                    font.pixelSize: 11
                    font.family: "monospace"
                    color: "#584e51"
                }

                Rectangle {
                    implicitWidth: 36
                    implicitHeight: 20
                    radius: 4
                    color: "#18171a"
                    border.color: Qt.rgba(1, 1, 1, 0.08)
                    border.width: 1

                    Text {
                        anchors.centerIn: parent
                        text: "Esc"
                        font.pixelSize: 10
                        font.bold: true
                        font.family: "monospace"
                        color: theme.accent
                    }
                }
            }
        }
    }

    // ── SHORTCUTS DIALOG ──
    Dialog {
        id: helpDialog
        anchors.centerIn: parent
        modal: true
        title: "shortcuts"
        standardButtons: Dialog.Close

        background: Rectangle {
            color: "#141315"
            radius: Math.max(8, Math.min(14, theme.radius))
            border.color: Qt.rgba(1, 1, 1, 0.08)
            border.width: 1
        }

        ColumnLayout {
            spacing: 10
            Repeater {
                model: [
                    { key: "1 - 8", desc: "Switch range (5h, 12h, 24h, 7d, 30d, 6m, 1y, all)" },
                    { key: "R", desc: "Reload logs and refresh session data" },
                    { key: "?", desc: "Toggle keyboard shortcuts help" },
                    { key: "Esc / Q", desc: "Quit application" }
                ]

                RowLayout {
                    spacing: 14
                    Rectangle {
                        implicitWidth: 64
                        implicitHeight: 22
                        color: "#1c1b1f"
                        radius: 4
                        border.color: Qt.rgba(1, 1, 1, 0.06)
                        border.width: 1

                        Text {
                            anchors.centerIn: parent
                            text: modelData.key
                            font.bold: true
                            font.pixelSize: 10
                            font.family: "monospace"
                            color: theme.accent
                        }
                    }
                    Text {
                        text: modelData.desc
                        font.pixelSize: 11
                        font.family: "monospace"
                        color: "#fafcfb"
                    }
                }
            }
        }
    }
}
