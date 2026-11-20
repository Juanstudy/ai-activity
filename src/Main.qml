import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

import "components"

ApplicationWindow {
    id: window
    visible: true
    width: theme.dp(1280)
    height: theme.dp(720)
    minimumWidth: theme.dp(980)
    minimumHeight: theme.dp(600)
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

    Shortcut {
        sequences: ["Ctrl++", "Ctrl+="]
        context: Qt.ApplicationShortcut
        onActivated: theme.setUiScale(Math.min(2.0, theme.uiScale + 0.05))
    }

    Shortcut {
        sequence: "Ctrl+-"
        context: Qt.ApplicationShortcut
        onActivated: theme.setUiScale(Math.max(0.8, theme.uiScale - 0.05))
    }

    Shortcut {
        sequence: "Ctrl+0"
        context: Qt.ApplicationShortcut
        onActivated: theme.setUiScale(1.18)
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: theme.dp(14)
        spacing: theme.dp(10)

        // ── TOP HEADER BAR ──
        RowLayout {
            Layout.fillWidth: true
            spacing: theme.dp(12)

            // App Identity
            RowLayout {
                spacing: theme.dp(8)
                Text {
                    text: "󰄧"
                    font.pixelSize: theme.dp(16)
                    color: theme.accent
                }

                Text {
                    text: "ai-activity"
                    font.pixelSize: theme.dp(14)
                    font.bold: true
                    font.family: "monospace"
                    color: "#fafcfb"
                }

                Text {
                    text: "· " + backend.lastSyncStatus
                    font.pixelSize: theme.dp(11)
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
                spacing: theme.dp(6)

                Rectangle {
                    implicitHeight: theme.dp(30)
                    implicitWidth: trmText.implicitWidth + theme.dp(18)
                    radius: theme.dp(15)
                    color: "#161517"
                    border.color: Qt.rgba(1, 1, 1, 0.06)
                    border.width: 1

                    Text {
                        id: trmText
                        anchors.centerIn: parent
                        text: "1 USD = " + Math.round(backend.trmRate).toLocaleString() + " COP"
                        font.pixelSize: theme.dp(11)
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
                    implicitHeight: theme.dp(30)
                    implicitWidth: theme.dp(30)
                    radius: theme.dp(15)
                    color: refMouse.containsMouse ? "#201e22" : "#161517"
                    border.color: Qt.rgba(1, 1, 1, 0.06)
                    border.width: 1

                    Text {
                        anchors.centerIn: parent
                        text: "↻"
                        font.pixelSize: theme.dp(13)
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
                    implicitHeight: theme.dp(30)
                    implicitWidth: theme.dp(30)
                    radius: theme.dp(15)
                    color: helpMouse.containsMouse ? "#201e22" : "#161517"
                    border.color: Qt.rgba(1, 1, 1, 0.06)
                    border.width: 1

                    Text {
                        anchors.centerIn: parent
                        text: "?"
                        font.pixelSize: theme.dp(12)
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

        // ── SCROLLABLE DASHBOARD VIEW ──
        ScrollView {
            id: mainScroll
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
            ScrollBar.vertical.policy: ScrollBar.AsNeeded
            contentWidth: availableWidth

            ColumnLayout {
                width: mainScroll.availableWidth
                spacing: theme.dp(24)

                // ── HERO VIEW (Heatmap + Consolidated KPI Panels) ──
                ColumnLayout {
                    id: heroSection
                    Layout.fillWidth: true
                    spacing: theme.dp(16)

                    // 1. ACTIVITY HEATMAP (On Top)
                    ActivityHeatmap {
                        id: heatmap
                        Layout.fillWidth: true
                    }

                    // 2. CONSOLIDATED KPI PANELS (Clean 2-panel architecture with interior dividers)
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: theme.dp(14)

                        // ── PANEL 1: COSTS & FINANCIALS ──
                        Rectangle {
                            id: costPanel
                            Layout.fillWidth: true
                            Layout.preferredWidth: 1
                            implicitHeight: theme.dp(168)
                            color: "#141315"
                            radius: Math.max(theme.dp(8), Math.min(theme.dp(12), theme.radius))
                            border.color: Qt.rgba(1, 1, 1, 0.05)
                            border.width: 1

                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: theme.dp(16)
                                spacing: theme.dp(12)

                                // Header: Title + Range Badge
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: theme.dp(8)

                                    Rectangle {
                                        width: theme.dp(5)
                                        height: theme.dp(5)
                                        radius: theme.dp(2)
                                        color: theme.accent
                                    }

                                    Text {
                                        text: "COSTS & FINANCIALS"
                                        font.pixelSize: theme.dp(11)
                                        font.bold: true
                                        font.family: "monospace"
                                        color: "#7c777d"
                                    }

                                    Item { Layout.fillWidth: true }

                                    Rectangle {
                                        implicitHeight: theme.dp(20)
                                        implicitWidth: costBadgeText.implicitWidth + theme.dp(12)
                                        radius: theme.dp(10)
                                        color: Qt.rgba(1, 1, 1, 0.05)
                                        border.color: Qt.rgba(1, 1, 1, 0.08)
                                        border.width: 1

                                        Text {
                                            id: costBadgeText
                                            anchors.centerIn: parent
                                            text: backend.currentRange.toUpperCase()
                                            font.pixelSize: theme.dp(10)
                                            font.bold: true
                                            font.family: "monospace"
                                            color: "#a09aa3"
                                        }
                                    }
                                }

                                // Subtle 1px Divider
                                Rectangle {
                                    Layout.fillWidth: true
                                    height: 1
                                    color: Qt.rgba(1, 1, 1, 0.04)
                                }

                                // Metrics Content: Total Cost | Avg per Turn
                                RowLayout {
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    spacing: theme.dp(18)

                                    // Metric 1: Total Cost
                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        Layout.fillHeight: true
                                        spacing: theme.dp(4)

                                        Text {
                                            text: "TOTAL COST"
                                            font.pixelSize: theme.dp(10)
                                            font.family: "monospace"
                                            color: "#584e51"
                                        }

                                        Text {
                                            text: "$" + backend.totalCostUsd.toFixed(4)
                                            font.pixelSize: theme.dp(30)
                                            font.bold: true
                                            font.family: "monospace"
                                            color: "#fafcfb"
                                            elide: Text.ElideRight
                                            Layout.fillWidth: true
                                        }

                                        Text {
                                            text: "~$" + Math.round(backend.totalCostCop).toLocaleString() + " COP"
                                            font.pixelSize: theme.dp(12)
                                            font.family: "monospace"
                                            color: "#8e888f"
                                            elide: Text.ElideRight
                                            Layout.fillWidth: true
                                        }

                                        Text {
                                            text: "1 USD ≈ " + Math.round(backend.trmRate).toLocaleString() + " COP"
                                            font.pixelSize: theme.dp(10)
                                            font.family: "monospace"
                                            color: "#463f47"
                                            elide: Text.ElideRight
                                            Layout.fillWidth: true
                                        }
                                    }

                                    // 1px Vertical Divider
                                    Rectangle {
                                        width: 1
                                        Layout.fillHeight: true
                                        color: Qt.rgba(1, 1, 1, 0.05)
                                    }

                                    // Metric 2: Avg per Turn
                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        Layout.fillHeight: true
                                        spacing: theme.dp(4)

                                        Text {
                                            text: "AVERAGE / TURN"
                                            font.pixelSize: theme.dp(10)
                                            font.family: "monospace"
                                            color: "#584e51"
                                        }

                                        Text {
                                            text: "$" + backend.avgCostPerTurnUsd.toFixed(4)
                                            font.pixelSize: theme.dp(26)
                                            font.bold: true
                                            font.family: "monospace"
                                            color: "#fafcfb"
                                            elide: Text.ElideRight
                                            Layout.fillWidth: true
                                        }

                                        Text {
                                            text: "~$" + Math.round(backend.avgCostPerTurnCop).toLocaleString() + " COP"
                                            font.pixelSize: theme.dp(12)
                                            font.family: "monospace"
                                            color: "#8e888f"
                                            elide: Text.ElideRight
                                            Layout.fillWidth: true
                                        }

                                        Text {
                                            text: "across " + backend.formatNumber(backend.totalTurns) + " turns"
                                            font.pixelSize: theme.dp(10)
                                            font.family: "monospace"
                                            color: "#463f47"
                                            elide: Text.ElideRight
                                            Layout.fillWidth: true
                                        }
                                    }
                                }
                            }
                        }

                        // ── PANEL 2: ACTIVITY & USAGE ──
                        Rectangle {
                            id: activityPanel
                            Layout.fillWidth: true
                            Layout.preferredWidth: 1.25
                            implicitHeight: theme.dp(168)
                            color: "#141315"
                            radius: Math.max(theme.dp(8), Math.min(theme.dp(12), theme.radius))
                            border.color: Qt.rgba(1, 1, 1, 0.05)
                            border.width: 1

                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: theme.dp(16)
                                spacing: theme.dp(12)

                                // Header: Title + Streak Pill
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: theme.dp(8)

                                    Text {
                                        text: "󰈸"
                                        font.pixelSize: theme.dp(13)
                                        color: theme.accent
                                    }

                                    Text {
                                        text: "ACTIVITY & USAGE"
                                        font.pixelSize: theme.dp(11)
                                        font.bold: true
                                        font.family: "monospace"
                                        color: "#7c777d"
                                    }

                                    Item { Layout.fillWidth: true }

                                    // Streak Badge
                                    Rectangle {
                                        implicitHeight: theme.dp(22)
                                        implicitWidth: streakPillRow.implicitWidth + theme.dp(14)
                                        radius: theme.dp(11)
                                        color: Qt.rgba(Qt.color(theme.accent).r, Qt.color(theme.accent).g, Qt.color(theme.accent).b, 0.12)
                                        border.color: Qt.rgba(Qt.color(theme.accent).r, Qt.color(theme.accent).g, Qt.color(theme.accent).b, 0.32)
                                        border.width: 1

                                        RowLayout {
                                            id: streakPillRow
                                            anchors.centerIn: parent
                                            spacing: theme.dp(5)

                                            Text {
                                                text: "󰈸"
                                                font.pixelSize: theme.dp(11)
                                                color: theme.accent
                                            }

                                            Text {
                                                text: backend.currentStreak + (backend.currentStreak === 1 ? " DAY STREAK" : " DAYS STREAK") + " (BEST: " + backend.maxStreak + ")"
                                                font.pixelSize: theme.dp(10)
                                                font.bold: true
                                                font.family: "monospace"
                                                color: theme.accent
                                            }
                                        }
                                    }
                                }

                                // Subtle 1px Divider
                                Rectangle {
                                    Layout.fillWidth: true
                                    height: 1
                                    color: Qt.rgba(1, 1, 1, 0.04)
                                }

                                // Metrics Content: Tokens | Reasoning | Turns & Sessions
                                RowLayout {
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    spacing: theme.dp(16)

                                    // Metric 2A: Tokens
                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        Layout.fillHeight: true
                                        spacing: theme.dp(4)

                                        Text {
                                            text: "TOKENS"
                                            font.pixelSize: theme.dp(10)
                                            font.family: "monospace"
                                            color: "#584e51"
                                        }

                                        Text {
                                            text: backend.formatCompact(backend.totalTokens)
                                            font.pixelSize: theme.dp(26)
                                            font.bold: true
                                            font.family: "monospace"
                                            color: "#fafcfb"
                                            elide: Text.ElideRight
                                            Layout.fillWidth: true
                                        }

                                        // Ratio Bar (in vs out)
                                        Rectangle {
                                            Layout.fillWidth: true
                                            implicitHeight: theme.dp(3)
                                            radius: theme.dp(1.5)
                                            color: Qt.rgba(1, 1, 1, 0.08)
                                            clip: true

                                            Rectangle {
                                                height: parent.height
                                                width: parent.width * (backend.totalTokens > 0 ? (backend.inputTokens / backend.totalTokens) : 0.5)
                                                radius: theme.dp(1.5)
                                                color: theme.accent
                                            }
                                        }

                                        Text {
                                            text: backend.formatCompact(backend.inputTokens) + " in · " + backend.formatCompact(backend.outputTokens) + " out"
                                            font.pixelSize: theme.dp(11)
                                            font.family: "monospace"
                                            color: "#8e888f"
                                            elide: Text.ElideRight
                                            Layout.fillWidth: true
                                        }
                                    }

                                    // 1px Vertical Divider
                                    Rectangle {
                                        width: 1
                                        Layout.fillHeight: true
                                        color: Qt.rgba(1, 1, 1, 0.05)
                                    }

                                    // Metric 2B: Reasoning
                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        Layout.fillHeight: true
                                        spacing: theme.dp(4)

                                        Text {
                                            text: "REASONING"
                                            font.pixelSize: theme.dp(10)
                                            font.family: "monospace"
                                            color: "#584e51"
                                        }

                                        Text {
                                            text: backend.formatCompact(backend.thinkingTokens)
                                            font.pixelSize: theme.dp(26)
                                            font.bold: true
                                            font.family: "monospace"
                                            color: "#fafcfb"
                                            elide: Text.ElideRight
                                            Layout.fillWidth: true
                                        }

                                        Text {
                                            text: backend.totalTokens > 0 ? ((backend.thinkingTokens * 100.0 / backend.totalTokens).toFixed(1) + "% of total") : "0%"
                                            font.pixelSize: theme.dp(11)
                                            font.family: "monospace"
                                            color: theme.accent
                                            elide: Text.ElideRight
                                            Layout.fillWidth: true
                                        }

                                        Text {
                                            text: "deep reasoning"
                                            font.pixelSize: theme.dp(10)
                                            font.family: "monospace"
                                            color: "#463f47"
                                            elide: Text.ElideRight
                                            Layout.fillWidth: true
                                        }
                                    }

                                    // 1px Vertical Divider
                                    Rectangle {
                                        width: 1
                                        Layout.fillHeight: true
                                        color: Qt.rgba(1, 1, 1, 0.05)
                                    }

                                    // Metric 2C: Turns & Sessions
                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        Layout.fillHeight: true
                                        spacing: theme.dp(4)

                                        Text {
                                            text: "TURNS & SESS"
                                            font.pixelSize: theme.dp(10)
                                            font.family: "monospace"
                                            color: "#584e51"
                                        }

                                        Text {
                                            text: backend.formatNumber(backend.totalTurns)
                                            font.pixelSize: theme.dp(26)
                                            font.bold: true
                                            font.family: "monospace"
                                            color: "#fafcfb"
                                            elide: Text.ElideRight
                                            Layout.fillWidth: true
                                        }

                                        Text {
                                            text: backend.formatNumber(backend.totalSessions) + " sessions"
                                            font.pixelSize: theme.dp(11)
                                            font.family: "monospace"
                                            color: "#8e888f"
                                            elide: Text.ElideRight
                                            Layout.fillWidth: true
                                        }

                                        Text {
                                            text: (backend.totalSessions > 0 ? (backend.totalTurns / backend.totalSessions).toFixed(1) : "0") + " turns/sess"
                                            font.pixelSize: theme.dp(10)
                                            font.family: "monospace"
                                            color: "#463f47"
                                            elide: Text.ElideRight
                                            Layout.fillWidth: true
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                // ── SECTION BREAK (Usage Breakdown) ──
                RowLayout {
                    Layout.fillWidth: true
                    spacing: theme.dp(10)

                    Rectangle {
                        width: theme.dp(6)
                        height: theme.dp(6)
                        radius: theme.dp(2)
                        color: theme.accent
                    }

                    Text {
                        text: qsTr("USAGE BREAKDOWN")
                        font.pixelSize: theme.dp(11)
                        font.bold: true
                        font.family: "monospace"
                        color: "#7c777d"
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        height: 1
                        color: Qt.rgba(1, 1, 1, 0.06)
                    }

                    Text {
                        text: qsTr("projects · models · agents")
                        font.pixelSize: theme.dp(10)
                        font.family: "monospace"
                        color: "#584e51"
                    }
                }

                // ── BREAKDOWN PANELS (Asymmetric 60/40 Layout) ──
                RowLayout {
                    Layout.fillWidth: true
                    implicitHeight: theme.dp(450)
                    spacing: theme.dp(14)

                    // Left Column: Projects (60% width, generous space for long paths)
                    BreakdownList {
                        title: "projects"
                        items: backend.projectBreakdown
                        Layout.fillWidth: true
                        Layout.preferredWidth: 60
                        Layout.fillHeight: true
                    }

                    // Right Column: Models & Agents Stack (40% width)
                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.preferredWidth: 40
                        Layout.fillHeight: true
                        spacing: theme.dp(14)

                        // Models Breakdown (Flexible height)
                        BreakdownList {
                            title: "models"
                            items: backend.modelBreakdown
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                        }

                        // Agents Breakdown (Compact panel sized for focused agent list)
                        BreakdownList {
                            title: "agents"
                            items: backend.agentBreakdown
                            Layout.fillWidth: true
                            implicitHeight: theme.dp(136)
                            Layout.maximumHeight: theme.dp(160)
                        }
                    }
                }

                Item {
                    implicitHeight: theme.dp(8)
                }
            }
        }

        // ── BOTTOM OMARCHY FOOTER ──
        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: theme.dp(4)
            spacing: theme.dp(8)

            Text {
                text: "󰄧 ai-activity"
                font.pixelSize: theme.dp(11)
                font.family: "monospace"
                color: "#584e51"
            }

            Text {
                text: "|"
                font.pixelSize: theme.dp(11)
                color: "#3a363d"
            }

            Text {
                text: "1-8 range · R refresh · ? shortcuts"
                font.pixelSize: theme.dp(11)
                font.family: "monospace"
                color: "#584e51"
            }

            Item { Layout.fillWidth: true }

            RowLayout {
                spacing: theme.dp(6)

                Text {
                    text: "Close"
                    font.pixelSize: theme.dp(11)
                    font.family: "monospace"
                    color: "#584e51"
                }

                Rectangle {
                    implicitWidth: theme.dp(36)
                    implicitHeight: theme.dp(20)
                    radius: theme.dp(4)
                    color: "#18171a"
                    border.color: Qt.rgba(1, 1, 1, 0.08)
                    border.width: 1

                    Text {
                        anchors.centerIn: parent
                        text: "Esc"
                        font.pixelSize: theme.dp(10)
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
            radius: Math.max(theme.dp(8), Math.min(theme.dp(14), theme.radius))
            border.color: Qt.rgba(1, 1, 1, 0.08)
            border.width: 1
        }

        ColumnLayout {
            spacing: theme.dp(10)
            Repeater {
                model: [
                    { key: "1 - 8", desc: "Switch range (5h, 12h, 24h, 7d, 30d, 6m, 1y, all)" },
                    { key: "R", desc: "Reload logs and refresh session data" },
                    { key: "?", desc: "Toggle keyboard shortcuts help" },
                    { key: "Ctrl + / -", desc: "Zoom in / out UI scale" },
                    { key: "Ctrl + 0", desc: "Reset UI scale" },
                    { key: "Esc / Q", desc: "Quit application" }
                ]

                RowLayout {
                    spacing: theme.dp(14)
                    Rectangle {
                        implicitWidth: theme.dp(84)
                        implicitHeight: theme.dp(24)
                        color: "#1c1b1f"
                        radius: theme.dp(4)
                        border.color: Qt.rgba(1, 1, 1, 0.06)
                        border.width: 1

                        Text {
                            anchors.centerIn: parent
                            text: modelData.key
                            font.bold: true
                            font.pixelSize: theme.dp(10)
                            font.family: "monospace"
                            color: theme.accent
                        }
                    }
                    Text {
                        text: modelData.desc
                        font.pixelSize: theme.dp(11)
                        font.family: "monospace"
                        color: "#fafcfb"
                    }
                }
            }
        }
    }
}
