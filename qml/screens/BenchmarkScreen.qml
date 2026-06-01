import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import STM32AiDeployer

Item {
    id: root

    property string _modelPath: ""
    readonly property bool _xcubeReady: {
        if (typeof backend === "undefined" || !backend) return false
        var tools = backend.toolPaths
        for (var i = 0; i < tools.length; i++)
            if (tools[i].key === "tools/xcubeai_cli_path") return tools[i].found
        return false
    }
    readonly property var _metricsMock: MockData.benchMetrics
    readonly property var _terminalMock: MockData.benchTerminal

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacingLg
        spacing: Theme.spacingLg

        // ── Header ────────────────────────────────────────────────────────
        RowLayout {
            Layout.fillWidth: true
            SectionHeader {
                title: "Kart Benchmark Testleri"
                subtitle: "Modeli kart üzerinde ölç ve raporla"
                Layout.fillWidth: true
            }
            StatusPill {
                text: root._xcubeReady ? "X-CUBE-AI hazır" : "X-CUBE-AI bulunamadı"
                status: root._xcubeReady ? "ready" : "error"
            }
            AppButton {
                text: "Benchmark Başlat"
                iconText: "▶"
                enabled: root._modelPath.length > 0
                onClicked: benchNote.open()
            }
        }

        GridLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            columns: root.width > 1100 ? 2 : 1
            columnSpacing: Theme.spacingMd; rowSpacing: Theme.spacingMd

            // ── Left: config + metrics ────────────────────────────────────
            ColumnLayout {
                Layout.fillWidth: true; Layout.fillHeight: true; spacing: Theme.spacingMd

                Card {
                    title: "Benchmark Ayarları"
                    Layout.fillWidth: true; Layout.preferredHeight: 260

                    ColumnLayout {
                        anchors.fill: parent; spacing: Theme.spacingMd

                        // model file picker
                        ColumnLayout {
                            Layout.fillWidth: true; spacing: Theme.spacingXs
                            Text {
                                text: "Model Dosyası (.tflite / .h5)"
                                color: Theme.textMuted
                                font.family: Theme.fontFamily; font.pixelSize: Theme.fontXs; font.weight: Font.DemiBold
                            }
                            RowLayout {
                                Layout.fillWidth: true; spacing: Theme.spacingSm
                                Rectangle {
                                    Layout.fillWidth: true; Layout.preferredHeight: 36
                                    radius: Theme.radiusMd; color: Theme.surfaceRaised; border.color: Theme.border
                                    Text {
                                        anchors.verticalCenter: parent.verticalCenter; anchors.left: parent.left
                                        anchors.leftMargin: Theme.spacingSm
                                        text: root._modelPath.length > 0
                                              ? root._modelPath.split(/[\\/]/).pop() : "Dosya seçilmedi"
                                        color: root._modelPath.length > 0 ? Theme.text : Theme.textFaint
                                        font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm
                                        elide: Text.ElideLeft; width: parent.width - Theme.spacingMd
                                    }
                                }
                                AppButton {
                                    text: "Seç"; variant: "secondary"
                                    onClicked: modelDialog.open()
                                }
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true; spacing: Theme.spacingMd
                            FormField { label: "Tekrar"; value: "20" }
                            FormField { label: "Seed"; value: "42" }
                        }
                        RowLayout {
                            Layout.fillWidth: true; spacing: Theme.spacingMd
                            FormField { label: "Min"; value: "0.0" }
                            FormField { label: "Max"; value: "1.0" }
                        }

                        RowLayout {
                            Layout.fillWidth: true; spacing: Theme.spacingSm
                            AppButton {
                                text: "Başlat"; iconText: "▶"
                                enabled: root._modelPath.length > 0
                                onClicked: benchNote.open()
                            }
                            AppButton { text: "Model Seç"; iconText: "📁"; variant: "secondary"
                                onClicked: modelDialog.open() }
                        }
                    }
                }

                Card {
                    title: "Metrikler"
                    Layout.fillWidth: true; Layout.fillHeight: true

                    ColumnLayout {
                        anchors.fill: parent; spacing: Theme.spacingXs

                        Repeater {
                            model: root._metricsMock
                            delegate: RowLayout {
                                Layout.fillWidth: true
                                Text {
                                    text: modelData[0]; color: Theme.textMuted
                                    font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm
                                    Layout.preferredWidth: 120
                                }
                                Text {
                                    text: modelData[1]; color: Theme.text
                                    font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm
                                    font.weight: Font.DemiBold; Layout.fillWidth: true
                                }
                            }
                        }
                        Item { Layout.fillHeight: true }
                    }
                }
            }

            // ── Right: terminal output ────────────────────────────────────
            Card {
                title: "Çıktı"; Layout.fillWidth: true; Layout.fillHeight: true

                ColumnLayout {
                    anchors.fill: parent; spacing: Theme.spacingSm
                    Terminal {
                        Layout.fillWidth: true; Layout.fillHeight: true
                        lines: root._terminalMock
                    }
                    RowLayout {
                        Layout.fillWidth: true; spacing: Theme.spacingSm
                        Item { Layout.fillWidth: true }
                        AppButton { text: "Temizle"; variant: "secondary" }
                    }
                }
            }
        }
    }

    // ── Model file dialog ──────────────────────────────────────────────────
    FileDialog {
        id: modelDialog
        title: "Model dosyası seçin"
        nameFilters: ["AI Model (*.tflite *.h5 *.onnx)", "Tüm dosyalar (*)"]
        onAccepted: root._modelPath = selectedFile.toString().replace("file:///", "")
    }

    // ── Placeholder for full XCubeAI integration ───────────────────────────
    Popup {
        id: benchNote
        modal: true; anchors.centerIn: Overlay.overlay; width: 420; padding: Theme.spacingLg
        background: Rectangle { color: Theme.surface; radius: Theme.radiusLg; border.color: Theme.border }
        contentItem: ColumnLayout {
            spacing: Theme.spacingMd
            Text { text: "Benchmark Başlatılıyor"; color: Theme.text; font.family: Theme.fontFamily
                font.pixelSize: Theme.fontMd; font.weight: Font.DemiBold }
            Text {
                Layout.fillWidth: true
                text: root._xcubeReady
                    ? ("stedgeai validate çalıştırılacak:\n" + root._modelPath)
                    : "X-CUBE-AI (stedgeai) bulunamadı.\nAraçlar → Ayarlar'dan stedgeai yolunu girin."
                color: root._xcubeReady ? Theme.textMuted : Theme.danger
                font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm; wrapMode: Text.WordWrap
            }
            RowLayout {
                Layout.fillWidth: true
                AppButton { text: "Kapat"; variant: "ghost"; onClicked: benchNote.close() }
                Item { Layout.fillWidth: true }
                AppButton {
                    visible: !root._xcubeReady
                    text: "Ayarları Aç"; variant: "secondary"
                    onClicked: { benchNote.close(); settingsRequested() }
                }
            }
        }
    }

    signal settingsRequested()
}
