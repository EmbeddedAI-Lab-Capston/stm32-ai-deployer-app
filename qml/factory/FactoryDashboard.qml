import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import STM32AiDeployer

Item {
    id: root
    property int rev: 0
    signal openZone(int zoneId)

    function kpiCards() {
        root.rev // dependency
        if (typeof factorySim === "undefined" || !factorySim) return []
        return [
            { title: "Aktif Node", value: factorySim.nodeCount + "", subtitle: factorySim.zoneCount + " üretim bölgesi", icon: "N", accent: Theme.primary },
            { title: "Toplam Sensör", value: factorySim.sensorCount + "", subtitle: "Canlı veri akışı", icon: "S", accent: Theme.cyan },
            { title: "Anlık Alarm", value: factorySim.activeAlarmCount + "", subtitle: factorySim.criticalCount + " kritik · " + factorySim.warningCount + " uyarı",
              icon: "!", accent: factorySim.criticalCount > 0 ? Theme.danger : (factorySim.warningCount > 0 ? Theme.warning : Theme.success) },
            { title: "Ort. Inference", value: (factorySim.avgInfUs / 1000).toFixed(2) + " ms", subtitle: "Tüm node'lar ortalaması", icon: "T", accent: Theme.success },
            { title: "Throughput", value: Math.round(factorySim.throughput) + " inf/s", subtitle: "Saniyelik toplam çıkarım", icon: "⚡", accent: Theme.purple }
        ]
    }

    function zoneRows() {
        root.rev
        if (typeof factorySim === "undefined" || !factorySim) return []
        return factorySim.zones()
    }

    function alarmModel() {
        root.rev
        if (typeof factorySim === "undefined" || !factorySim) return []
        return factorySim.alarms(40)
    }

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth
        clip: true
        ScrollBar.vertical.policy: ScrollBar.AsNeeded

        ColumnLayout {
            width: root.width
            spacing: Theme.spacingLg

            Item { Layout.fillWidth: true; Layout.preferredHeight: Theme.spacingMd }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.leftMargin: Theme.spacingLg
                Layout.rightMargin: Theme.spacingLg
                spacing: Theme.spacingLg

                RowLayout {
                    Layout.fillWidth: true
                    SectionHeader {
                        title: "Fabrika Genel Bakış"
                        subtitle: "Tüm üretim bölgelerindeki gömülü AI node'larının canlı durumu"
                        Layout.fillWidth: true
                    }
                    StatusPill {
                        text: (typeof factorySim !== "undefined" && factorySim && factorySim.running) ? "Simülasyon aktif" : "Duraklatıldı"
                        status: (typeof factorySim !== "undefined" && factorySim && factorySim.running) ? "running" : "warning"
                    }
                }

                // KPI cards
                GridLayout {
                    Layout.fillWidth: true
                    columns: root.width > 1280 ? 5 : (root.width > 820 ? 3 : 1)
                    columnSpacing: Theme.spacingMd
                    rowSpacing: Theme.spacingMd
                    Repeater {
                        model: root.kpiCards()
                        delegate: InfoCard {
                            title: modelData.title
                            value: modelData.value
                            subtitle: modelData.subtitle
                            iconText: modelData.icon
                            accent: modelData.accent
                        }
                    }
                }

                GridLayout {
                    Layout.fillWidth: true
                    columns: root.width > 1180 ? 2 : 1
                    columnSpacing: Theme.spacingMd
                    rowSpacing: Theme.spacingMd

                    // Zone health
                    Card {
                        title: "Bölge Sağlığı"
                        subtitle: "Her bölgedeki node durum dağılımı"
                        Layout.fillWidth: true
                        Layout.preferredHeight: 360

                        ColumnLayout {
                            anchors.fill: parent
                            spacing: Theme.spacingSm

                            Repeater {
                                model: root.zoneRows()
                                delegate: Rectangle {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 56
                                    radius: Theme.radiusSm
                                    color: zoneMouse.containsMouse ? Theme.surfaceHover : Theme.surfaceRaised

                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.leftMargin: Theme.spacingSm
                                        anchors.rightMargin: Theme.spacingSm
                                        spacing: Theme.spacingSm

                                        Rectangle {
                                            Layout.preferredWidth: 26; Layout.preferredHeight: 26
                                            radius: Theme.radiusSm
                                            color: Theme.alpha(Theme.statusColor(modelData.status), 0.18)
                                            Text {
                                                anchors.centerIn: parent; text: modelData.id
                                                color: Theme.statusColor(modelData.status)
                                                font.family: Theme.fontFamily; font.weight: Font.Bold
                                                font.pixelSize: Theme.fontSm
                                            }
                                        }
                                        ColumnLayout {
                                            spacing: 0
                                            Layout.fillWidth: true
                                            Text {
                                                text: modelData.name; color: Theme.text
                                                font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm
                                                font.weight: Font.DemiBold; elide: Text.ElideRight
                                                Layout.fillWidth: true
                                            }
                                            Text {
                                                text: modelData.nodeCount + " node · " + modelData.sensorCount + " sensör"
                                                color: Theme.textFaint
                                                font.family: Theme.fontFamily; font.pixelSize: Theme.fontXs
                                            }
                                        }
                                        StatusPill {
                                            status: modelData.status
                                            text: (modelData.criticalCount + modelData.warningCount) > 0
                                                  ? ((modelData.criticalCount + modelData.warningCount) + " olay") : "Normal"
                                        }
                                    }
                                    MouseArea {
                                        id: zoneMouse
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        onClicked: root.openZone(modelData.id)
                                    }
                                }
                            }
                            Item { Layout.fillHeight: true }
                        }
                    }

                    // Recent alarms
                    Card {
                        title: "Canlı Olay Akışı"
                        subtitle: "Eşik aşımları ve anomali tespitleri"
                        Layout.fillWidth: true
                        Layout.preferredHeight: 360

                        AlarmFeed {
                            anchors.fill: parent
                            model: root.alarmModel()
                        }
                    }
                }

                Item { Layout.fillWidth: true; Layout.preferredHeight: Theme.spacingLg }
            }
        }
    }
}
