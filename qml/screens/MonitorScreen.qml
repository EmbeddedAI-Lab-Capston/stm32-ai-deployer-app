import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import STM32AiDeployer

Item {
    id: root

    readonly property var _lines: (typeof backend !== "undefined" && backend && backend.monitorLines.length > 0)
                                  ? backend.monitorLines : MockData.monitorTerminal
    readonly property bool _connected: (typeof appState !== "undefined" && appState) ? appState.connected : true
    readonly property string _connText: (typeof appState !== "undefined" && appState && appState.connected)
                                        ? (appState.activePort + " @ " + appState.activeBaud) : "Bağlı değil"

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacingLg
        spacing: Theme.spacingLg

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
                text: "Temizle"
                iconText: "🗑"
                variant: "secondary"
                onClicked: if (typeof backend !== "undefined" && backend) backend.clearMonitor()
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: Theme.spacingMd

            // ── Terminal ──────────────────────────────────────────────────
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

            // ── Metrics column ────────────────────────────────────────────
            ColumnLayout {
                Layout.preferredWidth: 320
                Layout.fillWidth: false
                Layout.fillHeight: true
                spacing: Theme.spacingMd

                Repeater {
                    model: MockData.monitorMetrics
                    delegate: Card {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 110
                        padding: Theme.spacingMd

                        RowLayout {
                            anchors.fill: parent
                            spacing: Theme.spacingMd

                            Rectangle {
                                Layout.preferredWidth: 48
                                Layout.preferredHeight: 48
                                radius: Theme.radiusMd
                                color: Theme.alpha(modelData.accent, 0.16)
                                MetricIcon {
                                    anchors.centerIn: parent
                                    width: 24
                                    height: 24
                                    name: modelData.icon
                                    color: modelData.accent
                                }
                            }

                            ColumnLayout {
                                spacing: 2
                                Layout.fillWidth: true
                                Text {
                                    text: modelData.title
                                    color: Theme.textMuted
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontSm
                                }
                                Text {
                                    text: modelData.value
                                    color: Theme.text
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontXl
                                    font.weight: Font.Bold
                                }
                            }
                        }
                    }
                }

                Card {
                    title: "Simülasyon"
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: Theme.spacingMd
                        FormField { label: "Aralık (ms)"; value: "500" }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Theme.spacingSm
                            AppButton { text: "Başlat"; iconText: "▶" }
                            AppButton { text: "Durdur"; iconText: "■"; variant: "danger" }
                        }
                        Item { Layout.fillHeight: true }
                    }
                }
            }
        }
    }
}
