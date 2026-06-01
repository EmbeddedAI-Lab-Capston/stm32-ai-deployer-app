import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import STM32AiDeployer

Item {
    id: root

    // Sub-tab index: 0=Benchmark, 1=Simulation, 2=Sensor, 3=Compiled
    property int subIndex: 0
    property string exportMessage: ""
    property bool exportOk: true
    property int selectedRecordId: -1
    property var selectedCells: []
    property string boardFilter: "Tüm Kartlar"
    property string typeFilter: "Tüm Türler"

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

    // Load records from backend
    function rowsForIndex(i) {
        if (typeof backend === "undefined" || !backend) return mockRows(i)
        var rec
        if      (i === 0) rec = backend.benchmarkRecords
        else if (i === 1) rec = backend.simulationRecords
        else if (i === 2) rec = backend.sensorRecords
        else              rec = backend.compiledRecords

        return rec || []
    }

    function mockRows(i) {
        return []
    }

    function rowCells(row) {
        if (row && row.cells !== undefined) return row.cells
        return row || []
    }

    function rowsAsCells(rows) {
        var out = []
        for (var i = 0; i < rows.length; ++i) out.push(rowCells(rows[i]))
        return out
    }

    function boardColumn(i) { return i === 3 ? 3 : 2 }
    function typeColumn(i) { return i === 3 ? 2 : 6 }

    function filterOptions(col, prefix) {
        var seen = {}
        var out = [prefix]
        var rows = rowsForIndex(root.subIndex)
        for (var i = 0; i < rows.length; ++i) {
            var cells = rowCells(rows[i])
            var v = cells[col]
            if (v && v !== "—" && !seen[v]) {
                seen[v] = true
                out.push(v)
            }
        }
        return out
    }

    function filteredRows() {
        var rows = rowsForIndex(root.subIndex)
        var out = []
        var bCol = boardColumn(root.subIndex)
        var tCol = typeColumn(root.subIndex)
        for (var i = 0; i < rows.length; ++i) {
            var cells = rowCells(rows[i])
            var boardOk = root.boardFilter === "Tüm Kartlar" || cells[bCol] === root.boardFilter
            var typeOk = root.typeFilter === "Tüm Türler" || cells[tCol] === root.typeFilter
            if (boardOk && typeOk) out.push(rows[i])
        }
        return out
    }

    // Summary stats computed from current records
    function summaryCards(i) {
        var rows = rowsAsCells(filteredRows())
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
        if (i === 1 || i === 2) {
            return [
                { title:"Toplam Kayıt",  value: String(total), accent: Theme.primary },
                { title:"Ort. Süre",     value: averageTime(rows, 8), accent: Theme.success },
                { title:"Ort. Güven",    value: averagePercent(rows, 11), accent: Theme.cyan },
                { title:i === 1 ? "Label Sayısı" : "Model Sayısı",
                  value:i === 1 ? uniqueLabels(rows, 11) : uniqueCount(rows, 5),
                  accent: Theme.warning }
            ]
        }
        if (i === 3) {
            return [
                { title:"Toplam Kayıt",  value: String(total),  accent: Theme.primary },
                { title:"Kart Sayısı",   value: uniqueCount(rows, 3), accent: Theme.cyan },
                { title:"Model Sayısı",  value: uniqueCount(rows, 1), accent: Theme.success },
                { title:"Firmware",      value: uniqueCount(rows, 10), accent: Theme.warning }
            ]
        }
        return [
            { title:"Toplam Kayıt",  value: String(total),  accent: Theme.primary },
            { title:"—", value:"—", accent: Theme.cyan },
            { title:"—", value:"—", accent: Theme.success },
            { title:"—", value:"—", accent: Theme.warning }
        ]
    }

    function averageTime(rows, col) {
        var sum = 0
        var count = 0
        for (var k = 0; k < rows.length; ++k) {
            var n = parseNumber(rows[k][col])
            if (!isNaN(n)) {
                sum += n
                count++
            }
        }
        return count > 0 ? (sum / count).toFixed(2) + " ms" : "—"
    }

    function averagePercent(rows, col) {
        var sum = 0
        var count = 0
        for (var k = 0; k < rows.length; ++k) {
            var match = String(rows[k][col] || "").match(/(\d+(?:[.,]\d+)?)\s*%/)
            if (match) {
                sum += parseFloat(match[1].replace(",", "."))
                count++
            }
        }
        return count > 0 ? "%" + Math.round(sum / count) : "—"
    }

    function uniqueLabels(rows, col) {
        var seen = {}
        for (var k = 0; k < rows.length; ++k) {
            var raw = String(rows[k][col] || "")
            var label = raw.replace(/\s+\d+(?:[.,]\d+)?\s*%.*$/, "").trim()
            if (label.length > 0 && label !== "—")
                seen[label] = true
        }
        return String(Object.keys(seen).length)
    }

    function uniqueCount(rows, col) {
        var seen = {}
        for (var k = 0; k < rows.length; ++k) {
            var v = rows[k][col]; if (v && v !== "—") seen[v] = true
        }
        return String(Object.keys(seen).length)
    }

    function chartTitle(i) {
        return i === 3 ? "Model Kaynakları" : "Inference Süresi"
    }

    function chartSubtitle(i) {
        return i === 3 ? "Weights / MACC karşılaştırması" : "Model karşılaştırması"
    }

    function parseNumber(text) {
        if (!text || text === "—") return NaN
        var n = parseFloat(String(text).replace(",", "."))
        return n
    }

    function exportBaseName() {
        return "stm32_ai_" + root._subTabs[root.subIndex].toLowerCase()
            .replace(/\s+/g, "_").replace(/ü/g, "u").replace(/ö/g, "o")
            .replace(/ı/g, "i").replace(/ğ/g, "g").replace(/ş/g, "s")
            .replace(/ç/g, "c")
    }

    function localPath(url) {
        return String(url).replace("file:///", "")
    }

    function showExportResult(ok, path, type) {
        root.exportOk = ok
        root.exportMessage = ok
            ? (type + " dosyası kaydedildi:\n" + path)
            : (type + " dosyası kaydedilemedi.")
        exportNote.open()
    }

    function exportRows() {
        var rows = rowsForIndex(root.subIndex)
        return rowsAsCells(filteredRows())
    }

    // Bar chart values from inference/resource columns.
    function barData(i, rows) {
        var out = []
        var maxValue = 0
        for (var k = 0; k < Math.min(rows.length, 6); ++k) {
            var cells = rowCells(rows[k])
            var raw = i === 3 ? (cells[9] || cells[8]) : cells[8]
            var value = parseNumber(raw)
            if (!isNaN(value)) maxValue = Math.max(maxValue, value)
        }
        for (var j = 0; j < Math.min(rows.length, 6); ++j) {
            var c = rowCells(rows[j])
            var label = i === 3 ? (c[1] || ("Model " + (j + 1))) : (c[5] || ("Kayıt " + (j + 1)))
            var text = i === 3 ? (c[9] || c[8] || "—") : (c[8] || "—")
            var n = parseNumber(text)
            out.push({
                label: label,
                text: isNaN(n) ? text : (i === 3 ? text : (n.toFixed(2) + " ms")),
                value: isNaN(n) || maxValue <= 0 ? 0 : Math.max(0.04, n / maxValue)
            })
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
                onClicked: { var s = root.subIndex; root.subIndex = -1; root.subIndex = s; root.selectedRecordId = -1 } }
            AppButton {
                text: "Sil"; iconText: "×"; variant: "danger"
                enabled: root.selectedRecordId >= 0
                onClicked: {
                    if (typeof backend !== "undefined" && backend && root.selectedRecordId >= 0) {
                        backend.deleteAnalysisRecord(root.selectedRecordId)
                        root.selectedRecordId = -1
                    }
                }
            }
            AppButton {
                text: "Tekrar Yükle"; iconText: "▲"; variant: "secondary"
                visible: root.subIndex === 3
                enabled: root.selectedRecordId >= 0
                onClicked: if (typeof backend !== "undefined" && backend) backend.flashCompiledModel(root.selectedRecordId)
            }
            AppButton {
                text: "CSV Dışa Aktar"; iconText: "↓"; variant: "secondary"
                enabled: root.exportRows().length > 0
                onClicked: csvDialog.open()
            }
            AppButton {
                text: "PDF Rapor"; iconText: "↓"; variant: "secondary"
                enabled: root.exportRows().length > 0
                onClicked: pdfDialog.open()
            }
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
                    MouseArea { anchors.fill: parent; onClicked: {
                        root.subIndex = index
                        root.boardFilter = "Tüm Kartlar"
                        root.typeFilter = "Tüm Türler"
                        root.selectedRecordId = -1
                    } }
                }
            }
            Item { Layout.fillWidth: true }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spacingMd
            ComboField {
                label: "Kart"
                options: root.filterOptions(root.boardColumn(root.subIndex), "Tüm Kartlar")
                Layout.preferredWidth: 260
                onActivated: (idx) => root.boardFilter = options[idx]
            }
            ComboField {
                label: "Tür"
                options: root.filterOptions(root.typeColumn(root.subIndex), "Tüm Türler")
                Layout.preferredWidth: 220
                onActivated: (idx) => root.typeFilter = options[idx]
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
                        rows: root.filteredRows()
                        onRowSelected: (id, cells, rowIndex) => {
                            root.selectedRecordId = id
                            root.selectedCells = cells
                        }
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
                title: root.chartTitle(root.subIndex); subtitle: root.chartSubtitle(root.subIndex)
                Layout.preferredWidth: root.subIndex === 3 ? 460 : 380
                Layout.fillWidth: false; Layout.fillHeight: true

                ColumnLayout {
                    anchors.fill: parent; spacing: 0

                    RowLayout {
                        id: barChart
                        Layout.fillWidth: true; Layout.fillHeight: true
                        spacing: Theme.spacingSm

                        Repeater {
                            model: root.barData(root.subIndex, root.filteredRows())
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
                            visible: root.filteredRows().length === 0
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

    FileDialog {
        id: csvDialog
        title: "CSV dosyasını kaydet"
        fileMode: FileDialog.SaveFile
        nameFilters: ["CSV dosyası (*.csv)", "Tüm dosyalar (*)"]
        currentFile: root.exportBaseName() + ".csv"
        onAccepted: {
            var path = root.localPath(selectedFile)
            var ok = typeof backend !== "undefined" && backend
                ? backend.exportAnalysisCsv(path, root.colsForIndex(root.subIndex), root.exportRows())
                : false
            root.showExportResult(ok, path, "CSV")
        }
    }

    FileDialog {
        id: pdfDialog
        title: "PDF raporunu kaydet"
        fileMode: FileDialog.SaveFile
        nameFilters: ["PDF dosyası (*.pdf)", "Tüm dosyalar (*)"]
        currentFile: root.exportBaseName() + ".pdf"
        onAccepted: {
            var path = root.localPath(selectedFile)
            var ok = typeof backend !== "undefined" && backend
                ? backend.exportAnalysisPdf(path, "STM32 AI " + root._subTabs[root.subIndex] + " Raporu",
                                            root.colsForIndex(root.subIndex), root.exportRows())
                : false
            root.showExportResult(ok, path, "PDF")
        }
    }

    Popup {
        id: exportNote
        modal: true
        anchors.centerIn: Overlay.overlay
        width: 420
        padding: Theme.spacingLg
        background: Rectangle { color: Theme.surface; radius: Theme.radiusLg; border.color: Theme.border }
        contentItem: ColumnLayout {
            spacing: Theme.spacingMd
            Text {
                text: root.exportOk ? "Kaydedildi" : "Kaydetme başarısız"
                color: root.exportOk ? Theme.success : Theme.danger
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontMd
                font.weight: Font.DemiBold
            }
            Text {
                Layout.fillWidth: true
                text: root.exportMessage
                color: Theme.textMuted
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSm
                wrapMode: Text.WrapAnywhere
            }
            AppButton { Layout.alignment: Qt.AlignRight; text: "Tamam"; onClicked: exportNote.close() }
        }
    }
}
