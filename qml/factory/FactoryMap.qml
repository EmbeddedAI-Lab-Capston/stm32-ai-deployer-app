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

    // Floor-plan layout: fractional room rectangles + type glyph per zone.
    readonly property var layoutById: ({
        1: { x: 0.000, y: 0.000, w: 0.325, h: 0.46, glyph: "☣" },
        3: { x: 0.340, y: 0.000, w: 0.320, h: 0.46, glyph: "⚙" },
        4: { x: 0.675, y: 0.000, w: 0.325, h: 0.46, glyph: "📷" },
        2: { x: 0.000, y: 0.540, w: 0.520, h: 0.46, glyph: "🛢" },
        5: { x: 0.540, y: 0.540, w: 0.460, h: 0.46, glyph: "📡" }
    })

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth
        clip: true
        ScrollBar.vertical.policy: ScrollBar.AsNeeded

        ColumnLayout {
            width: root.width
            spacing: Theme.spacingMd

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
                        subtitle: "Üretim sahasının yerleşim planı — bir düğüme tıklayarak detayına inin"
                        Layout.fillWidth: true
                    }
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

                // ── Blueprint floor ─────────────────────────────────────────
                Rectangle {
                    id: floor
                    Layout.fillWidth: true
                    Layout.preferredHeight: Math.max(720, root.width * 0.46)
                    radius: Theme.radiusLg
                    color: Theme.bgElevated
                    border.color: Theme.borderStrong
                    clip: true

                    readonly property int pad: Theme.spacingLg
                    function roomX(r) { return pad + r.x * (width  - 2 * pad) }
                    function roomY(r) { return pad + r.y * (height - 2 * pad) }
                    function roomW(r) { return r.w * (width  - 2 * pad) }
                    function roomH(r) { return r.h * (height - 2 * pad) }

                    // blueprint grid + connection lines to the central hub
                    Canvas {
                        id: grid
                        anchors.fill: parent
                        onWidthChanged: requestPaint()
                        onHeightChanged: requestPaint()
                        onPaint: {
                            var ctx = getContext("2d")
                            ctx.reset()
                            // faint grid
                            ctx.strokeStyle = Qt.rgba(0.36, 0.41, 0.48, 0.18)
                            ctx.lineWidth = 1
                            var step = 30
                            for (var gx = step; gx < width; gx += step) {
                                ctx.beginPath(); ctx.moveTo(gx, 0); ctx.lineTo(gx, height); ctx.stroke()
                            }
                            for (var gy = step; gy < height; gy += step) {
                                ctx.beginPath(); ctx.moveTo(0, gy); ctx.lineTo(width, gy); ctx.stroke()
                            }
                            // dashed lines from each room centre to the hub
                            var cx = width / 2, cy = height / 2
                            ctx.strokeStyle = Qt.rgba(0.31, 0.55, 1.0, 0.30)
                            ctx.lineWidth = 1.5
                            ctx.setLineDash([5, 5])
                            var ids = [1, 2, 3, 4, 5]
                            for (var i = 0; i < ids.length; ++i) {
                                var r = root.layoutById[ids[i]]
                                if (!r) continue
                                var rx = floor.roomX(r) + floor.roomW(r) / 2
                                var ry = floor.roomY(r) + floor.roomH(r) / 2
                                ctx.beginPath(); ctx.moveTo(cx, cy); ctx.lineTo(rx, ry); ctx.stroke()
                            }
                            ctx.setLineDash([])
                        }
                    }

                    // rooms (zones)
                    Repeater {
                        model: root.zoneList()
                        delegate: ZoneBlock {
                            readonly property var rect: root.layoutById[modelData.id]
                            visible: rect !== undefined
                            x: rect ? floor.roomX(rect) : 0
                            y: rect ? floor.roomY(rect) : 0
                            width:  rect ? floor.roomW(rect) : 0
                            height: rect ? floor.roomH(rect) : 0
                            zone: modelData
                            nodes: root.nodesOf(modelData.id)
                            glyph: rect ? rect.glyph : ""
                            onZoneActivated: (id) => root.openZone(id)
                            onNodeActivated: (id) => root.openNode(id)
                        }
                    }

                    // central data-aggregation hub
                    Rectangle {
                        anchors.centerIn: parent
                        width: 132; height: 60
                        radius: Theme.radiusMd
                        color: Theme.surfaceRaised
                        border.color: Theme.primary
                        border.width: 1.5

                        // pulse ring
                        Rectangle {
                            anchors.centerIn: parent
                            width: parent.width; height: parent.height
                            radius: parent.radius
                            color: "transparent"
                            border.color: Theme.primary
                            border.width: 1
                            opacity: 0
                            SequentialAnimation on opacity {
                                running: true; loops: Animation.Infinite
                                NumberAnimation { from: 0.5; to: 0; duration: 1600; easing.type: Easing.OutCubic }
                            }
                            scale: 1.0
                            SequentialAnimation on scale {
                                running: true; loops: Animation.Infinite
                                NumberAnimation { from: 1.0; to: 1.5; duration: 1600; easing.type: Easing.OutCubic }
                            }
                        }

                        ColumnLayout {
                            anchors.centerIn: parent
                            spacing: 2
                            Text {
                                Layout.alignment: Qt.AlignHCenter
                                text: "☁ VERİ MERKEZİ"
                                color: Theme.primary
                                font.family: Theme.fontFamily; font.pixelSize: Theme.fontXs; font.weight: Font.Bold
                            }
                            Text {
                                Layout.alignment: Qt.AlignHCenter
                                text: (typeof factorySim !== "undefined" && factorySim)
                                      ? (Math.round(factorySim.throughput) + " inf/s") : ""
                                color: Theme.textMuted
                                font.family: Theme.monoFamily; font.pixelSize: 10
                            }
                        }
                    }
                }

                Item { Layout.fillWidth: true; Layout.preferredHeight: Theme.spacingLg }
            }
        }
    }
}
