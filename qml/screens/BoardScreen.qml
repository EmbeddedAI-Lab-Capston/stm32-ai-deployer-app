import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import STM32AiDeployer

Item {
    id: root

    readonly property var _boardRows: (typeof appState !== "undefined" && appState && appState.boardInfoRows.length > 0)
                                      ? appState.boardInfoRows : MockData.boardInfo
    property var _ports: ["COM5 (ST-Link)", "COM3", "COM7"]
    readonly property bool _connected: (typeof appState !== "undefined" && appState) ? appState.connected : true

    function refreshPorts() {
        if (typeof backend !== "undefined" && backend) {
            var p = backend.availablePorts()
            _ports = (p.length > 0) ? p : ["(port bulunamadı)"]
        }
    }
    Component.onCompleted: refreshPorts()

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth
        clip: true

        ColumnLayout {
            width: root.width
            spacing: Theme.spacingLg

            Item { Layout.fillWidth: true; Layout.preferredHeight: Theme.spacingXs }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.leftMargin: Theme.spacingLg
                Layout.rightMargin: Theme.spacingLg
                spacing: Theme.spacingLg

                SectionHeader {
                    title: "Kart Seçimi ve Bilgileri"
                    subtitle: "Aktif kartın özellikleri ve bağlantı yönetimi"
                    Layout.fillWidth: true
                }

                GridLayout {
                    Layout.fillWidth: true
                    columns: root.width > 1080 ? 2 : 1
                    columnSpacing: Theme.spacingMd
                    rowSpacing: Theme.spacingMd

                    // ── Active board info ─────────────────────────────────
                    Card {
                        title: "Aktif Kart Bilgileri"
                        Layout.fillWidth: true
                        Layout.preferredHeight: 460

                        ColumnLayout {
                            anchors.fill: parent
                            spacing: Theme.spacingXs

                            Repeater {
                                model: root._boardRows
                                delegate: RowLayout {
                                    Layout.fillWidth: true
                                    spacing: Theme.spacingMd
                                    Text {
                                        text: modelData[0]
                                        color: Theme.textMuted
                                        font.family: Theme.fontFamily
                                        font.pixelSize: Theme.fontSm
                                        Layout.preferredWidth: 110
                                    }
                                    Text {
                                        text: modelData[1]
                                        color: Theme.text
                                        font.family: Theme.fontFamily
                                        font.pixelSize: Theme.fontSm
                                        font.weight: Font.DemiBold
                                        elide: Text.ElideRight
                                        Layout.fillWidth: true
                                    }
                                }
                            }

                            Item { Layout.fillHeight: true }

                            RowLayout {
                                spacing: Theme.spacingSm
                                StatusPill { text: "Uyumlu"; status: "ok" }
                                Item { Layout.fillWidth: true }
                            }
                        }
                    }

                    // ── Selection + connection ────────────────────────────
                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 460
                        spacing: Theme.spacingMd

                        Card {
                            title: "Kart Seçimi"
                            Layout.fillWidth: true
                            Layout.fillHeight: true

                            ColumnLayout {
                                anchors.fill: parent
                                spacing: Theme.spacingSm

                                Repeater {
                                    model: MockData.boardPresets
                                    delegate: Rectangle {
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 64
                                        radius: Theme.radiusMd
                                        color: index === 1 ? Theme.primarySoft : Theme.surfaceRaised
                                        border.color: index === 1 ? Theme.primary : Theme.border
                                        border.width: index === 1 ? 2 : 1

                                        RowLayout {
                                            anchors.fill: parent
                                            anchors.margins: Theme.spacingMd
                                            spacing: Theme.spacingMd

                                            ColumnLayout {
                                                spacing: 2
                                                Layout.fillWidth: true
                                                Text {
                                                    text: modelData.name
                                                    color: Theme.text
                                                    font.family: Theme.fontFamily
                                                    font.pixelSize: Theme.fontMd
                                                    font.weight: Font.DemiBold
                                                }
                                                Text {
                                                    text: modelData.spec
                                                    color: Theme.textMuted
                                                    font.family: Theme.fontFamily
                                                    font.pixelSize: Theme.fontXs
                                                }
                                            }

                                            StatusPill { text: modelData.target; status: "info" }
                                        }
                                    }
                                }

                                Item { Layout.fillHeight: true }
                            }
                        }

                        Card {
                            title: "Bağlantı"
                            Layout.fillWidth: true
                            Layout.preferredHeight: 196

                            ColumnLayout {
                                anchors.fill: parent
                                spacing: Theme.spacingMd

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: Theme.spacingMd
                                    FormField { id: portField; label: "Port"; options: root._ports }
                                    FormField { id: baudField; label: "Baud"; options: ["115200", "230400", "921600", "460800"] }
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: Theme.spacingSm
                                    AppButton {
                                        text: root._connected ? "Bağlantıyı Kes" : "Bağlan"
                                        iconText: "⏻"
                                        variant: root._connected ? "danger" : "primary"
                                        onClicked: {
                                            if (typeof backend === "undefined" || !backend) return
                                            if (root._connected) {
                                                backend.disconnectSerial()
                                            } else {
                                                var port = root._ports[portField.currentIndex]
                                                var baud = parseInt(["115200","230400","921600","460800"][baudField.currentIndex])
                                                backend.connectSerial(port, baud)
                                            }
                                        }
                                    }
                                    AppButton {
                                        text: "Yenile"
                                        variant: "secondary"
                                        onClicked: root.refreshPorts()
                                    }
                                    Item { Layout.fillWidth: true }
                                    StatusPill {
                                        text: root._connected ? "Bağlı" : "Bağlı değil"
                                        status: root._connected ? "connected" : "offline"
                                    }
                                }

                                Item { Layout.fillHeight: true }
                            }
                        }
                    }
                }

                Item { Layout.fillWidth: true; Layout.preferredHeight: Theme.spacingLg }
            }
        }
    }
}
