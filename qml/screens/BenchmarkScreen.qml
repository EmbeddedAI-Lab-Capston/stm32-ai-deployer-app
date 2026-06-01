import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import STM32AiDeployer

Item {
    id: root

    signal settingsRequested()

    property string _modelPath: ""
    readonly property bool _busy: (typeof backend !== "undefined" && backend) ? backend.benchmarkBusy : false
    readonly property var _lines: (typeof backend !== "undefined" && backend) ? backend.benchmarkLines : []
    readonly property var _metrics: (typeof backend !== "undefined" && backend) ? backend.benchmarkMetrics : ({})
    readonly property bool _xcubeReady: {
        if (typeof backend === "undefined" || !backend) return false
        var tools = backend.toolPaths
        for (var i = 0; i < tools.length; i++)
            if (tools[i].key === "tools/xcubeai_cli_path") return tools[i].found
        return false
    }

    function metricRows() {
        var m = root._metrics || {}
        return [
            ["Model", m.model || "-"],
            ["Ortalama", m.avg || "-"],
            ["Min / Max", (m.min || "-") + " / " + (m.max || "-")],
            ["RAM", m.ram || "-"],
            ["Free RAM", m.freeRam || "-"],
            ["Ornek", m.samples ? String(m.samples) : "-"]
        ]
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacingLg
        spacing: Theme.spacingLg

        RowLayout {
            Layout.fillWidth: true
            SectionHeader {
                title: "Kart Benchmark Testleri"
                subtitle: "UART BENCH komutu ile kart uzerinde olcum al"
                Layout.fillWidth: true
            }
            StatusPill {
                text: root._xcubeReady ? "X-CUBE-AI hazir" : "X-CUBE-AI yok"
                status: root._xcubeReady ? "ready" : "warning"
            }
            AppButton {
                text: root._busy ? "Calisiyor..." : "Benchmark Baslat"
                iconText: ">"
                enabled: !root._busy
                onClicked: {
                    if (typeof backend === "undefined" || !backend) return
                    backend.startBenchmark(parseInt(samplesField.text) || 20,
                                           parseFloat(minField.text) || 0.0,
                                           parseFloat(maxField.text) || 1.0,
                                           parseInt(seedField.text) || 42)
                }
            }
        }

        GridLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            columns: root.width > 1100 ? 2 : 1
            columnSpacing: Theme.spacingMd
            rowSpacing: Theme.spacingMd

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: Theme.spacingMd

                Card {
                    title: "Benchmark Ayarlari"
                    Layout.fillWidth: true
                    Layout.preferredHeight: 330

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: Theme.spacingMd

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: Theme.spacingXs
                            Text {
                                text: "Model dosyasi opsiyonel; UART BENCH icin zorunlu degil"
                                color: Theme.textMuted
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontXs
                                font.weight: Font.DemiBold
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: Theme.spacingSm
                                Rectangle {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 36
                                    radius: Theme.radiusMd
                                    color: Theme.surfaceRaised
                                    border.color: Theme.border
                                    Text {
                                        anchors.verticalCenter: parent.verticalCenter
                                        anchors.left: parent.left
                                        anchors.leftMargin: Theme.spacingSm
                                        width: parent.width - Theme.spacingMd
                                        text: root._modelPath.length > 0 ? root._modelPath.split(/[\\/]/).pop() : "Dosya secilmedi"
                                        color: root._modelPath.length > 0 ? Theme.text : Theme.textFaint
                                        font.family: Theme.fontFamily
                                        font.pixelSize: Theme.fontSm
                                        elide: Text.ElideLeft
                                    }
                                }
                                AppButton { text: "Sec"; variant: "secondary"; onClicked: modelDialog.open() }
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Theme.spacingMd
                            ColumnLayout {
                                spacing: Theme.spacingXs
                                Layout.fillWidth: true
                                Text { text: "Tekrar"; color: Theme.textMuted; font.family: Theme.fontFamily; font.pixelSize: Theme.fontXs; font.weight: Font.DemiBold }
                                TextField {
                                    id: samplesField
                                    Layout.fillWidth: true
                                    text: "20"
                                    enabled: !root._busy
                                    selectByMouse: true
                                    inputMethodHints: Qt.ImhDigitsOnly
                                    color: Theme.text
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontSm
                                    background: Rectangle { radius: Theme.radiusMd; color: Theme.surfaceRaised; border.color: samplesField.activeFocus ? Theme.primary : Theme.border }
                                    leftPadding: Theme.spacingSm; rightPadding: Theme.spacingSm
                                }
                            }
                            ColumnLayout {
                                spacing: Theme.spacingXs
                                Layout.fillWidth: true
                                Text { text: "Seed"; color: Theme.textMuted; font.family: Theme.fontFamily; font.pixelSize: Theme.fontXs; font.weight: Font.DemiBold }
                                TextField {
                                    id: seedField
                                    Layout.fillWidth: true
                                    text: "42"
                                    enabled: !root._busy
                                    selectByMouse: true
                                    inputMethodHints: Qt.ImhDigitsOnly
                                    color: Theme.text
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontSm
                                    background: Rectangle { radius: Theme.radiusMd; color: Theme.surfaceRaised; border.color: seedField.activeFocus ? Theme.primary : Theme.border }
                                    leftPadding: Theme.spacingSm; rightPadding: Theme.spacingSm
                                }
                            }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Theme.spacingMd
                            ColumnLayout {
                                spacing: Theme.spacingXs
                                Layout.fillWidth: true
                                Text { text: "Min"; color: Theme.textMuted; font.family: Theme.fontFamily; font.pixelSize: Theme.fontXs; font.weight: Font.DemiBold }
                                TextField {
                                    id: minField
                                    Layout.fillWidth: true
                                    text: "0.0"
                                    enabled: !root._busy
                                    selectByMouse: true
                                    color: Theme.text
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontSm
                                    background: Rectangle { radius: Theme.radiusMd; color: Theme.surfaceRaised; border.color: minField.activeFocus ? Theme.primary : Theme.border }
                                    leftPadding: Theme.spacingSm; rightPadding: Theme.spacingSm
                                }
                            }
                            ColumnLayout {
                                spacing: Theme.spacingXs
                                Layout.fillWidth: true
                                Text { text: "Max"; color: Theme.textMuted; font.family: Theme.fontFamily; font.pixelSize: Theme.fontXs; font.weight: Font.DemiBold }
                                TextField {
                                    id: maxField
                                    Layout.fillWidth: true
                                    text: "1.0"
                                    enabled: !root._busy
                                    selectByMouse: true
                                    color: Theme.text
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontSm
                                    background: Rectangle { radius: Theme.radiusMd; color: Theme.surfaceRaised; border.color: maxField.activeFocus ? Theme.primary : Theme.border }
                                    leftPadding: Theme.spacingSm; rightPadding: Theme.spacingSm
                                }
                            }
                        }

                        Rectangle {
                            visible: !root._xcubeReady
                            Layout.fillWidth: true
                            Layout.preferredHeight: 42
                            radius: Theme.radiusMd
                            color: Theme.alpha(Theme.warning, 0.12)
                            border.color: Theme.warning
                            Text {
                                anchors.centerIn: parent
                                width: parent.width - Theme.spacingMd
                                text: "stedgeai bulunamadi; BENCH UART akisi yine calisir."
                                color: Theme.textMuted
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontXs
                                horizontalAlignment: Text.AlignHCenter
                                elide: Text.ElideRight
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Theme.spacingSm
                            AppButton {
                                text: root._busy ? "Calisiyor..." : "Baslat"
                                iconText: ">"
                                enabled: !root._busy
                                onClicked: {
                                    if (typeof backend === "undefined" || !backend) return
                                    backend.startBenchmark(parseInt(samplesField.text) || 20,
                                                           parseFloat(minField.text) || 0.0,
                                                           parseFloat(maxField.text) || 1.0,
                                                           parseInt(seedField.text) || 42)
                                }
                            }
                            AppButton {
                                text: "Iptal"
                                variant: "danger"
                                enabled: root._busy
                                onClicked: if (typeof backend !== "undefined" && backend) backend.cancelBenchmark()
                            }
                            AppButton { text: "Model Sec"; variant: "secondary"; onClicked: modelDialog.open() }
                        }
                    }
                }

                Card {
                    title: "Metrikler"
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: Theme.spacingXs
                        Repeater {
                            model: root.metricRows()
                            delegate: RowLayout {
                                Layout.fillWidth: true
                                Text {
                                    text: modelData[0]
                                    color: Theme.textMuted
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontSm
                                    Layout.preferredWidth: 120
                                }
                                Text {
                                    text: modelData[1]
                                    color: Theme.text
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontSm
                                    font.weight: Font.DemiBold
                                    Layout.fillWidth: true
                                    elide: Text.ElideRight
                                }
                            }
                        }
                        Item { Layout.fillHeight: true }
                    }
                }
            }

            Card {
                title: "Cikti"
                Layout.fillWidth: true
                Layout.fillHeight: true

                ColumnLayout {
                    anchors.fill: parent
                    spacing: Theme.spacingSm
                    Terminal {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        lines: root._lines
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.spacingSm
                        Item { Layout.fillWidth: true }
                        AppButton {
                            text: "Temizle"
                            variant: "secondary"
                            enabled: !root._busy
                            onClicked: if (typeof backend !== "undefined" && backend) backend.clearBenchmarkLog()
                        }
                    }
                }
            }
        }
    }

    FileDialog {
        id: modelDialog
        title: "Model dosyasi secin"
        nameFilters: ["AI Model (*.tflite *.h5 *.onnx)", "Tum dosyalar (*)"]
        onAccepted: root._modelPath = selectedFile.toString().replace("file:///", "")
    }

}
