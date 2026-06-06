import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import STM32AiDeployer

Item {
    id: root
    property int rev: 0
    property int nodeId: 1

    function nodeInfo() {
        root.rev
        if (typeof factorySim === "undefined" || !factorySim) return ({})
        return factorySim.node(root.nodeId)
    }
    function allNodes() {
        root.rev
        if (typeof factorySim === "undefined" || !factorySim) return []
        return factorySim.nodes()
    }
    function alarmModel() {
        root.rev
        if (typeof factorySim === "undefined" || !factorySim) return []
        return factorySim.alarms(60)
    }

    readonly property var info: nodeInfo()

    RowLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacingLg
        spacing: Theme.spacingMd

        // ── Left: node picker ──────────────────────────────────────────────
        Card {
            Layout.preferredWidth: 240
            Layout.fillHeight: true
            title: "Düğümler"
            subtitle: factorySim ? (factorySim.nodeCount + " STM32 node") : ""

            ListView {
                anchors.fill: parent
                clip: true
                model: root.allNodes()
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar { width: 8 }
                spacing: 3

                delegate: Rectangle {
                    width: ListView.view.width
                    height: 42
                    radius: Theme.radiusSm
                    property bool active: modelData.id === root.nodeId
                    color: active ? Theme.primarySoft : (nMouse.containsMouse ? Theme.surfaceHover : "transparent")

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: Theme.spacingSm
                        anchors.rightMargin: Theme.spacingSm
                        spacing: Theme.spacingXs
                        Rectangle {
                            Layout.preferredWidth: 8; Layout.preferredHeight: 8; radius: 4
                            color: Theme.statusColor(modelData.status)
                        }
                        Text {
                            text: modelData.name
                            color: active ? Theme.primary : Theme.text
                            font.family: Theme.fontFamily; font.pixelSize: Theme.fontXs
                            font.weight: active ? Font.DemiBold : Font.Normal
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                    }
                    MouseArea {
                        id: nMouse; anchors.fill: parent; hoverEnabled: true
                        onClicked: root.nodeId = modelData.id
                    }
                }
            }
        }

        // ── Center: node detail ────────────────────────────────────────────
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: Theme.spacingMd

            // header + metric tiles
            Card {
                Layout.fillWidth: true
                Layout.preferredHeight: 168

                ColumnLayout {
                    anchors.fill: parent
                    spacing: Theme.spacingSm

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.spacingSm
                        ColumnLayout {
                            spacing: 2
                            Layout.fillWidth: true
                            Text {
                                text: root.info.name !== undefined ? root.info.name : "—"
                                color: Theme.text
                                font.family: Theme.fontFamily; font.pixelSize: Theme.fontLg
                                font.weight: Font.Bold; elide: Text.ElideRight; Layout.fillWidth: true
                            }
                            Text {
                                text: root.info.zoneName !== undefined ? root.info.zoneName : ""
                                color: Theme.textMuted
                                font.family: Theme.fontFamily; font.pixelSize: Theme.fontXs
                                elide: Text.ElideRight; Layout.fillWidth: true
                            }
                        }
                        StatusPill {
                            status: root.info.status !== undefined ? root.info.status : "ok"
                            text: root.info.status === "error" ? "Kritik"
                                  : (root.info.status === "warning" ? "Uyarı" : "Normal")
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        spacing: Theme.spacingSm

                        Repeater {
                            model: [
                                { l: "Kart",      v: root.info.board !== undefined ? root.info.board : "—", a: Theme.primary },
                                { l: "Model",     v: root.info.model !== undefined ? root.info.model : "—", a: Theme.cyan },
                                { l: "Inference", v: root.info.infMs !== undefined ? root.info.infMs.toFixed(2) + " ms" : "—", a: Theme.success },
                                { l: "Boş RAM",   v: root.info.freeRamKb !== undefined ? root.info.freeRamKb.toFixed(0) + " KB" : "—", a: Theme.warning },
                                { l: "Sensör",    v: root.info.sensorCount !== undefined ? (root.info.sensorCount + " adet") : "—", a: Theme.purple }
                            ]
                            delegate: Rectangle {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                radius: Theme.radiusMd
                                color: Theme.surfaceRaised
                                border.color: Theme.border

                                ColumnLayout {
                                    anchors.fill: parent
                                    anchors.margins: Theme.spacingSm
                                    spacing: 4
                                    Text { text: modelData.l; color: Theme.textMuted
                                           font.family: Theme.fontFamily; font.pixelSize: Theme.fontXs }
                                    Item { Layout.fillHeight: true }
                                    Text { text: modelData.v; color: modelData.a
                                           font.family: Theme.fontFamily; font.pixelSize: Theme.fontMd
                                           font.weight: Font.Bold; elide: Text.ElideRight; Layout.fillWidth: true }
                                }
                            }
                        }
                    }
                }
            }

            // sensors (scrollable so it never overflows)
            Card {
                Layout.fillWidth: true
                Layout.fillHeight: true
                title: "Bağlı Sensörler"
                subtitle: "Canlı değerler ve eşik durumu"

                ScrollView {
                    anchors.fill: parent
                    clip: true
                    contentWidth: availableWidth
                    ScrollBar.vertical.policy: ScrollBar.AsNeeded

                    ColumnLayout {
                        width: parent.width
                        spacing: 2
                        Repeater {
                            model: root.info.sensors ? root.info.sensors : []
                            delegate: SensorRow { sensor: modelData }
                        }
                    }
                }
            }
        }

        // ── Right: global alarm feed ───────────────────────────────────────
        Card {
            Layout.preferredWidth: 300
            Layout.fillHeight: true
            title: "Olay Akışı"
            subtitle: "Fabrika geneli alarmlar"

            AlarmFeed {
                anchors.fill: parent
                model: root.alarmModel()
            }
        }
    }
}
