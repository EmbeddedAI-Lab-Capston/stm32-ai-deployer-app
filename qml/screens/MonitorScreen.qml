import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import STM32AiDeployer

Item {
    id: root

    readonly property var _lines: (typeof backend !== "undefined" && backend && backend.monitorLines.length > 0)
                                  ? backend.monitorLines : MockData.monitorTerminal
    readonly property bool _connected: (typeof appState !== "undefined" && appState) ? appState.connected : true
    readonly property bool _simRunning: (typeof backend !== "undefined" && backend) ? backend.simRunning : false
    readonly property string _connText: _connected
        ? ((typeof appState !== "undefined" && appState)
           ? (appState.activePort + " @ " + appState.activeBaud) : "Bağlı")
        : "Bağlı değil"

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacingLg
        spacing: Theme.spacingLg

        // ── Header ────────────────────────────────────────────────────────
        RowLayout {
            Layout.fillWidth: true
            SectionHeader {
                title: "UART Monitör"
                subtitle: "Canlı § protokol akışı ve metrikler"
                Layout.fillWidth: true
            }
            StatusPill {
                text: root._connected ? root._connText : "Bağlı değil"
                status: root._connected ? "connected" : "offline"
            }
            AppButton {
                text: "Temizle"; variant: "secondary"
                onClicked: if (typeof backend !== "undefined" && backend) backend.clearMonitor()
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: Theme.spacingMd

            // ── Terminal (left, fills most of width) ──────────────────────
            Card {
                title: "Terminal"
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumWidth: 360

                Terminal {
                    anchors.fill: parent
                    lines: root._lines
                }
            }

            // ── Right column: metric cards + simulation ───────────────────
            ColumnLayout {
                Layout.preferredWidth: 300
                Layout.fillWidth: false
                Layout.fillHeight: true
                spacing: Theme.spacingMd

                // Metric cards (live AppState values)
                Repeater {
                    model: [
                        { title: "Inference",  icon: "speed",  accentProp: "primary" },
                        { title: "RAM",        icon: "memory", accentProp: "success" },
                        { title: "Son Tahmin", icon: "target", accentProp: "cyan"    }
                    ]
                    delegate: Card {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 90
                        padding: Theme.spacingMd

                        RowLayout {
                            anchors.fill: parent
                            spacing: Theme.spacingMd

                            Rectangle {
                                Layout.preferredWidth: 42
                                Layout.preferredHeight: 42
                                radius: Theme.radiusMd
                                color: Theme.alpha(Theme[modelData.accentProp], 0.16)
                                MetricIcon {
                                    anchors.centerIn: parent
                                    width: 22; height: 22
                                    name: modelData.icon
                                    color: Theme[modelData.accentProp]
                                }
                            }

                            ColumnLayout {
                                spacing: 2; Layout.fillWidth: true
                                Text {
                                    text: modelData.title
                                    color: Theme.textMuted
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontSm
                                }
                                Text {
                                    text: {
                                        if (typeof appState === "undefined" || !appState) return "—"
                                        if (modelData.accentProp === "primary")
                                            return appState.lastInfMs > 0
                                                   ? (appState.lastInfMs.toFixed(2) + " ms") : "—"
                                        if (modelData.accentProp === "success")
                                            return "—"
                                        return appState.lastModel.length > 0
                                               ? (appState.lastModel + " · %" + appState.lastAcc) : "—"
                                    }
                                    color: Theme.text
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontXl
                                    font.weight: Font.Bold
                                }
                            }
                        }
                    }
                }

                // Simulation card
                Card {
                    title: "Simülasyon"
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: Theme.spacingMd

                        RowLayout {
                            Layout.fillWidth: true
                            Text {
                                text: "Aralık (ms)"
                                color: Theme.textMuted
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontXs
                                font.weight: Font.DemiBold
                            }
                        }

                        TextField {
                            id: intervalField
                            Layout.fillWidth: true
                            text: "500"
                            enabled: !root._simRunning
                            color: Theme.text
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSm
                            selectByMouse: true
                            background: Rectangle {
                                radius: Theme.radiusMd
                                color: Theme.surfaceRaised
                                border.color: intervalField.activeFocus ? Theme.primary : Theme.border
                            }
                            leftPadding: Theme.spacingSm; rightPadding: Theme.spacingSm
                        }

                        // min / max row
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Theme.spacingSm

                            ColumnLayout { spacing: 4; Layout.fillWidth: true
                                Text { text: "Min"; color: Theme.textMuted; font.family: Theme.fontFamily; font.pixelSize: Theme.fontXs }
                                TextField {
                                    id: minField; Layout.fillWidth: true; text: "0.0"; enabled: !root._simRunning
                                    color: Theme.text; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm
                                    selectByMouse: true
                                    background: Rectangle { radius: Theme.radiusMd; color: Theme.surfaceRaised; border.color: minField.activeFocus ? Theme.primary : Theme.border }
                                    leftPadding: Theme.spacingSm; rightPadding: Theme.spacingSm
                                }
                            }

                            ColumnLayout { spacing: 4; Layout.fillWidth: true
                                Text { text: "Max"; color: Theme.textMuted; font.family: Theme.fontFamily; font.pixelSize: Theme.fontXs }
                                TextField {
                                    id: maxField; Layout.fillWidth: true; text: "1.0"; enabled: !root._simRunning
                                    color: Theme.text; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm
                                    selectByMouse: true
                                    background: Rectangle { radius: Theme.radiusMd; color: Theme.surfaceRaised; border.color: maxField.activeFocus ? Theme.primary : Theme.border }
                                    leftPadding: Theme.spacingSm; rightPadding: Theme.spacingSm
                                }
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Theme.spacingSm
                            AppButton {
                                text: root._simRunning ? "Çalışıyor…" : "Başlat"
                                iconText: "▶"
                                enabled: !root._simRunning
                                onClicked: {
                                    if (typeof backend === "undefined" || !backend) return
                                    backend.startSimulation(
                                        parseInt(intervalField.text) || 500,
                                        parseFloat(minField.text)    || 0.0,
                                        parseFloat(maxField.text)    || 1.0)
                                }
                            }
                            AppButton {
                                text: "Durdur"; iconText: "■"; variant: "danger"
                                enabled: root._simRunning
                                onClicked: if (typeof backend !== "undefined" && backend) backend.stopSimulation()
                            }
                        }

                        Item { Layout.fillHeight: true }
                    }
                }
            }
        }
    }
}
