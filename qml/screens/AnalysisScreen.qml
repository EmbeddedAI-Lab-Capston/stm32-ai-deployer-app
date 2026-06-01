import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import STM32AiDeployer

Item {
    id: root

    // Sub-tab index: 0=Benchmark, 1=Simulation, 2=Sensor, 3=Compiled
    property int subIndex: 0

    readonly property var _subTabs: ["Benchmark", "Simülasyon", "Gerçek Sensör", "Derlenen Modeller"]

    // Column headers per kind
    readonly property var _cols: [
        // benchmark / simulation / sensor
        [{ title:"Tarih",w:120},{title:"Oturum",w:110},{title:"Kart",w:0},
         {title:"Chip",w:0},{title:"CPU",w:0},{title:"Model",w:0},
         {title:"Tür",w:60},{title:"Sensör",w:80},{title:"Ort",w:80},
         {title:"RAM",w:70},{title:"Weights",w:80},{title:"Sonuç",w:0}],
        // compiled
        [{title:"Tarih",w:120},{title:"Model",w:0},{title:"Tür",w:60},
         {title:"Kart",w:0},{title:"Chip",w:0},{title:"Sensör",w:80},
         {title:"Input",w:70},{title:"Params",w:80},{title:"MACC",w:80},
         {title:"Weights",w:80},{title:"Firmware",w:0},{title:"Arşiv",w:0}]
    ]

    function colsForIndex(i) { return i === 3 ? _cols[1] : _cols[0] }

    // Load records from backend or fall back to mock
    function rowsForIndex(i) {
        if (typeof backend === "undefined" || !backend) return mockRows(i)
        var rec
        if      (i === 0) rec = backend.benchmarkRecords
        else if (i === 1) rec = backend.simulationRecords
        else if (i === 2) rec = backend.sensorRecords
        else              rec = backend.compiledRecords

        if (!rec || rec.length === 0) return mockRows(i)
        var out = []
        for (var j = 0; j < rec.length; ++j) {
            var r = rec[j]
            out.push(r.cells)
        }
        return out
    }

    function mockRows(i) {
        if (i === 0) return MockData.analysisRows
        if (i === 1) return [["—","—","—","—","—","—","—","—","—","—","—","Kayıt yok"]]
        if (i === 2) return [["—","—","—","—","—","—","—","—","—","—","—","Kayıt yok"]]
        return [["—","—","—","—","—","—","—","—","—","—","—","Kayıt yok"]]
    }

    // Summary stats computed from current records
    function summaryCards(i) {
        var rows = rowsForIndex(i)
        var total = rows.length
        if (i === 0) {
            // find fastest inference
            var best = "—"
            for (var k = 0; k < rows.length; ++k) {
                var t = rows[k][8]; if (t && t !== "—" && (best === "—" || t < best)) best = t
            }
            return [
                { title:"Toplam Kayıt",  value: String(total),  accent: Theme.primary },
                { title:"En İyi Ort.",   value: best === "—" ? "—" : best, accent: Theme.success },
                { title:"Kart Sayısı",   value: uniqueCount(rows, 2), accent: Theme.cyan },
                { title:"Model Sayısı",  value: uniqueCount(rows, 5), accent: Theme.warning }
            ]
        }
        return [
            { title:"Toplam Kayıt",  value: String(total),  accent: Theme.primary },
            { title:"—", value:"—", accent: Theme.cyan },
            { title:"—", value:"—", accent: Theme.success },
            { title:"—", value:"—", accent: Theme.warning }
        ]
    }

    function uniqueCount(rows, col) {
        var seen = {}
        for (var k = 0; k < rows.length; ++k) {
            var v = rows[k][col]; if (v && v !== "—") seen[v] = true
        }
        return String(Object.keys(seen).length)
    }

    // Bar chart values from inference column (col 8)
    function barData(rows) {
        var out = []
        for (var k = 0; k < Math.min(rows.length, 6); ++k) {
            var label = rows[k][5] || ("Kayıt " + (k+1))
            var t = rows[k][8] || "0 ms"
            var ms = parseFloat(t)
            out.push({ label: label, text: isNaN(ms) ? t : (ms.toFixed(2) + " ms"), value: isNaN(ms) ? 0.1 : Math.min(ms / 5.0, 1.0) })
        }
        return out
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacingLg
        spacing: Theme.spacingMd

        // ── Header ────────────────────────────────────────────────────────
        RowLayout {
            Layout.fillWidth: true
            SectionHeader {
                title: "Analiz"
                subtitle: "Karşılaştırmalı performans ve kaynak analizi"
                Layout.fillWidth: true
            }
            AppButton { text: "Yenile"; iconText: "↻"; variant: "secondary"
                onClicked: { var s = root.subIndex; root.subIndex = -1; root.subIndex = s } }
            AppButton { text: "CSV"; iconText: "⭳"; variant: "secondary" }
            AppButton { text: "PDF"; iconText: "⭳"; variant: "secondary" }
        }

        // ── Sub-tab pills ─────────────────────────────────────────────────
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spacingXs
            Repeater {
                model: root._subTabs
                delegate: Rectangle {
                    Layout.preferredHeight: 34
                    Layout.preferredWidth: pillText.implicitWidth + Theme.spacingLg
                    radius: Theme.radiusMd
                    property bool active: index === root.subIndex
                    color: active ? Theme.primarySoft : Theme.surface
                    border.color: active ? Theme.primary : Theme.border
                    Text {
                        id: pillText; anchors.centerIn: parent; text: modelData
                        color: active ? Theme.text : Theme.textMuted
                        font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm
                        font.weight: active ? Font.DemiBold : Font.Normal
                    }
                    MouseArea { anchors.fill: parent; onClicked: root.subIndex = index }
                }
            }
            Item { Layout.fillWidth: true }
        }

        // ── Summary cards ─────────────────────────────────────────────────
        GridLayout {
            Layout.fillWidth: true
            columns: root.width > 1100 ? 4 : 2
            columnSpacing: Theme.spacingMd; rowSpacing: Theme.spacingMd
            Repeater {
                model: root.summaryCards(root.subIndex)
                delegate: InfoCard {
                    title: modelData.title; value: modelData.value; accent: modelData.accent
                }
            }
        }

        // ── Table + chart ─────────────────────────────────────────────────
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: Theme.spacingMd

            Card {
                title: "Kayıtlar"
                Layout.fillWidth: true; Layout.fillHeight: true; Layout.minimumWidth: 400

                ColumnLayout {
                    anchors.fill: parent; spacing: Theme.spacingXs

                    DataTable {
                        id: mainTable
                        Layout.fillWidth: true; Layout.fillHeight: true
                        columns: root.colsForIndex(root.subIndex)
                        rows: root.rowsForIndex(root.subIndex)
                    }

                    Text {
                        visible: mainTable.rows.length === 0
                        Layout.fillWidth: true; Layout.fillHeight: true
                        text: "Henüz kayıt yok.\nSimülasyon başlatın veya kart bağlayın."
                        color: Theme.textFaint
                        font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        wrapMode: Text.WordWrap
                    }
                }
            }

            Card {
                title: "Inference Süresi"; subtitle: "Model karşılaştırması"
                Layout.preferredWidth: 380; Layout.fillWidth: false; Layout.fillHeight: true

                ColumnLayout {
                    anchors.fill: parent; spacing: 0

                    RowLayout {
                        id: barChart
                        Layout.fillWidth: true; Layout.fillHeight: true
                        spacing: Theme.spacingSm

                        Repeater {
                            model: root.barData(root.rowsForIndex(root.subIndex))
                            delegate: ColumnLayout {
                                Layout.fillWidth: true; Layout.fillHeight: true; spacing: 4

                                Text {
                                    Layout.fillWidth: true; text: modelData.text
                                    color: Theme.textMuted; font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontXs
                                    horizontalAlignment: Text.AlignHCenter
                                }

                                Item {
                                    Layout.fillWidth: true; Layout.fillHeight: true
                                    Rectangle {
                                        anchors.bottom: parent.bottom; anchors.horizontalCenter: parent.horizontalCenter
                                        width: Math.max(16, parent.width * 0.5)
                                        height: Math.max(4, (parent.height - 4) * Math.max(0.03, modelData.value))
                                        radius: Theme.radiusSm
                                        gradient: Gradient {
                                            GradientStop { position: 0.0; color: Theme.primary }
                                            GradientStop { position: 1.0; color: Theme.cyan }
                                        }
                                        Behavior on height { NumberAnimation { duration: 300 } }
                                    }
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: modelData.label.length > 12
                                          ? modelData.label.substring(0, 10) + "…" : modelData.label
                                    color: Theme.textFaint; font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontXs
                                    horizontalAlignment: Text.AlignHCenter
                                    elide: Text.ElideRight
                                }
                            }
                        }

                        // placeholder when no rows
                        Text {
                            visible: root.rowsForIndex(root.subIndex).length === 0
                            Layout.fillWidth: true; Layout.fillHeight: true
                            text: "Kayıt\nyok"; color: Theme.textFaint
                            font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm
                            horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                        }
                    }
                }
            }
        }
    }
}
