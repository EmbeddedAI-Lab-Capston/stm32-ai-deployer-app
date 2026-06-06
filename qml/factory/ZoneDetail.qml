import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import STM32AiDeployer

Item {
    id: root
    property int rev: 0
    property int zoneId: 1
    signal openNode(int nodeId)

    function zoneInfo() {
        root.rev
        if (typeof factorySim === "undefined" || !factorySim) return ({})
        return factorySim.zoneSummary(root.zoneId)
    }
    function nodeList() {
        root.rev
        if (typeof factorySim === "undefined" || !factorySim) return []
        return factorySim.nodesInZone(root.zoneId)
    }
    function allZones() {
        if (typeof factorySim === "undefined" || !factorySim) return []
        return factorySim.zones()
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

                // zone selector pills
                RowLayout {
                    Layout.fillWidth: true
                    SectionHeader {
                        title: root.zoneInfo().name !== undefined ? root.zoneInfo().name : "Bölge Detayı"
                        subtitle: root.zoneInfo().hint !== undefined ? root.zoneInfo().hint : ""
                        Layout.fillWidth: true
                    }
                    StatusPill {
                        status: root.zoneInfo().status !== undefined ? root.zoneInfo().status : "ok"
                        text: root.zoneInfo().status === "error" ? "Kritik"
                              : (root.zoneInfo().status === "warning" ? "Uyarı" : "Normal")
                    }
                }

                Flow {
                    Layout.fillWidth: true
                    spacing: Theme.spacingXs
                    Repeater {
                        model: root.allZones()
                        delegate: Rectangle {
                            height: 32
                            width: zLabel.implicitWidth + Theme.spacingLg
                            radius: Theme.radiusSm
                            property bool active: modelData.id === root.zoneId
                            color: active ? Theme.primarySoft : (zMouse.containsMouse ? Theme.surfaceHover : Theme.surfaceRaised)
                            border.color: active ? Theme.primary : Theme.border
                            Text {
                                id: zLabel
                                anchors.centerIn: parent
                                text: modelData.id + " · " + modelData.name.split(" ")[0]
                                color: active ? Theme.primary : Theme.textMuted
                                font.family: Theme.fontFamily; font.pixelSize: Theme.fontXs
                                font.weight: active ? Font.DemiBold : Font.Normal
                            }
                            MouseArea {
                                id: zMouse; anchors.fill: parent; hoverEnabled: true
                                onClicked: root.zoneId = modelData.id
                            }
                        }
                    }
                }

                // node cards grid
                GridLayout {
                    Layout.fillWidth: true
                    columns: root.width > 1280 ? 2 : 1
                    columnSpacing: Theme.spacingMd
                    rowSpacing: Theme.spacingMd

                    Repeater {
                        model: root.nodeList()
                        delegate: Card {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 60 + 52 * (modelData.sensors ? modelData.sensors.length : 0)
                            title: modelData.name
                            subtitle: modelData.board + " · " + modelData.model
                                      + " · " + modelData.infMs.toFixed(2) + " ms"

                            ColumnLayout {
                                anchors.fill: parent
                                spacing: 2

                                RowLayout {
                                    Layout.fillWidth: true
                                    StatusPill {
                                        status: modelData.status
                                        text: modelData.status === "error" ? "Kritik"
                                              : (modelData.status === "warning" ? "Uyarı" : "Normal")
                                    }
                                    Item { Layout.fillWidth: true }
                                    Text {
                                        text: "Boş RAM: " + modelData.freeRamKb.toFixed(0) + " KB"
                                        color: Theme.textFaint
                                        font.family: Theme.fontFamily; font.pixelSize: Theme.fontXs
                                    }
                                    Rectangle {
                                        Layout.preferredWidth: detailLbl.implicitWidth + Theme.spacingSm
                                        Layout.preferredHeight: 24
                                        radius: Theme.radiusSm
                                        color: openMouse.containsMouse ? Theme.surfaceHover : "transparent"
                                        border.color: Theme.border
                                        Text {
                                            id: detailLbl
                                            anchors.centerIn: parent; text: "Detay →"
                                            color: Theme.primary
                                            font.family: Theme.fontFamily; font.pixelSize: Theme.fontXs
                                            font.weight: Font.DemiBold
                                        }
                                        MouseArea {
                                            id: openMouse; anchors.fill: parent; hoverEnabled: true
                                            onClicked: root.openNode(modelData.id)
                                        }
                                    }
                                }

                                Repeater {
                                    model: modelData.sensors ? modelData.sensors : []
                                    delegate: SensorRow { sensor: modelData }
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
