import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import STM32AiDeployer

Item {
    id: root
    property int rev: 0
    signal openZone(int zoneId)
    signal openNode(int nodeId)

    function zoneList() {
        root.rev
        if (typeof factorySim === "undefined" || !factorySim) return []
        return factorySim.zones()
    }
    function nodesOf(zoneId) {
        root.rev
        if (typeof factorySim === "undefined" || !factorySim) return []
        return factorySim.nodesInZone(zoneId)
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
                spacing: Theme.spacingMd

                RowLayout {
                    Layout.fillWidth: true
                    SectionHeader {
                        title: "Fabrika Krokisi"
                        subtitle: "Üretim sahasının bölge yerleşimi ve STM32 düğümleri — bir düğüme tıklayın"
                        Layout.fillWidth: true
                    }

                    // legend
                    RowLayout {
                        spacing: Theme.spacingMd
                        Repeater {
                            model: [
                                { c: Theme.success, t: "Normal" },
                                { c: Theme.warning, t: "Uyarı" },
                                { c: Theme.danger,  t: "Kritik" }
                            ]
                            delegate: RowLayout {
                                spacing: Theme.spacingXs
                                Rectangle { width: 9; height: 9; radius: 5; color: modelData.c }
                                Text { text: modelData.t; color: Theme.textMuted
                                       font.family: Theme.fontFamily; font.pixelSize: Theme.fontXs }
                            }
                        }
                    }
                }

                // factory floor: zones laid out in a responsive grid
                GridLayout {
                    Layout.fillWidth: true
                    columns: root.width > 1320 ? 3 : (root.width > 880 ? 2 : 1)
                    columnSpacing: Theme.spacingMd
                    rowSpacing: Theme.spacingMd

                    Repeater {
                        model: root.zoneList()
                        delegate: ZoneBlock {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 280
                            zone: modelData
                            nodes: root.nodesOf(modelData.id)
                            onZoneActivated: (id) => root.openZone(id)
                            onNodeActivated: (id) => root.openNode(id)
                        }
                    }
                }

                Item { Layout.fillWidth: true; Layout.preferredHeight: Theme.spacingLg }
            }
        }
    }
}
