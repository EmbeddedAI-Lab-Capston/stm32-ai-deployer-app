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
            Layout.preferredWidth: 250
            Layout.fillHeight: true
            title: "Node Listesi"
            subtitle: "Detay için bir düğüm seçin"

            ListView {
                anchors.fill: parent
                clip: true
                model: root.allNodes()
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar { width: 8 }
                spacing: 3

                delegate: Rectangle {
                    width: ListView.view.width
                    height: 44
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

            // header + metrics
            Card {
                Layout.fillWidth: true
                Layout.preferredHeight: 150
                title: root.info.name !== undefined ? root.info.name : "—"
                subtitle: root.info.zoneName !== undefined ? root.info.zoneName : ""

                RowLayout {
                    anchors.fill: parent
                    spacing: Theme.spacingMd

                    Repeater {
                        model: [
                            { l: "Kart",      v: root.info.board !== undefined ? root.info.board : "—", a: Theme.primary },
                            { l: "Model",     v: root.info.model !== undefined ? root.info.model : "—", a: Theme.cyan },
                            { l: "Inference", v: root.info.infMs !== undefined ? root.info.infMs.toFixed(2) + " ms" : "—", a: Theme.success },
                            { l: "Boş RAM",   v: root.info.freeRamKb !== undefined ? root.info.freeRamKb.toFixed(0) + " KB" : "—", a: Theme.warning },
                            { l: "Durum",     v: root.info.status === "error" ? "Kritik" : (root.info.status === "warning" ? "Uyarı" : "Normal"),
                              a: Theme.statusColor(root.info.status !== undefined ? root.info.status : "ok") }
                        ]
                        delegate: ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 4
                            Text { text: modelData.l; color: Theme.textMuted
                                   font.family: Theme.fontFamily; font.pixelSize: Theme.fontXs }
                            Text { text: modelData.v; color: modelData.a
                                   font.family: Theme.fontFamily; font.pixelSize: Theme.fontLg
                                   font.weight: Font.Bold; elide: Text.ElideRight; Layout.fillWidth: true }
                        }
                    }
                }
            }

            // sensors
            Card {
                Layout.fillWidth: true
                Layout.fillHeight: true
                title: "Bağlı Sensörler"
                subtitle: "Canlı değerler ve eşik durumu"

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 2
                    Repeater {
                        model: root.info.sensors ? root.info.sensors : []
                        delegate: SensorRow { sensor: modelData }
                    }
                    Item { Layout.fillHeight: true }
                }
            }
        }

        // ── Right: global alarm feed ───────────────────────────────────────
        ColumnLayout {
            Layout.preferredWidth: 320
            Layout.fillHeight: true
            spacing: Theme.spacingSm

            SectionHeader {
                title: "Olay Akışı"
                subtitle: "Fabrika geneli alarmlar"
                Layout.fillWidth: true
            }
            AlarmFeed {
                Layout.fillWidth: true
                Layout.fillHeight: true
                model: root.alarmModel()
            }
        }
    }
}
