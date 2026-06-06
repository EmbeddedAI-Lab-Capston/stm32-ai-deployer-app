import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import STM32AiDeployer

Item {
    id: root

    signal settingsRequested()

    property string _modelName: ""
    property string _modelSource: ""
    property bool _inputSpecsLoaded: false
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

    function resetInputSpecs() {
        inputModel.clear()
        inputModel.append({ name: "input[0]", shape: "-", min: "0.0", max: "1.0" })
        root._inputSpecsLoaded = false
    }

    function loadDeployedModelInputs() {
        var info = (typeof backend !== "undefined" && backend) ? backend.deployedModelInfo() : ({})
        root._modelName = info.name || ((typeof appState !== "undefined" && appState) ? appState.lastModel : "")
        root._modelSource = info.hasReport ? info.reportPath : (info.hasModelPath ? info.modelPath : "")
        inputModel.clear()
        var specs = (typeof backend !== "undefined" && backend) ? backend.deployedModelInputSpecs() : []
        if (!specs || specs.length === 0) {
            inputModel.append({ name: "input[0]", shape: "-", min: "0.0", max: "1.0" })
            root._inputSpecsLoaded = false
            return
        }
        for (var i = 0; i < specs.length; i++) {
            inputModel.append({
                name: specs[i].name || ("input[" + i + "]"),
                label: specs[i].label || "",
                shape: specs[i].shape || "-",
                min: Number(specs[i].min || 0).toFixed(4),
                max: Number(specs[i].max || 1).toFixed(4)
            })
        }
        root._inputSpecsLoaded = true
    }

    function collectInputRanges() {
        var ranges = []
        for (var i = 0; i < inputModel.count; i++) {
            var row = inputModel.get(i)
            var minValue = parseFloat(row.min)
            var maxValue = parseFloat(row.max)
            if (isNaN(minValue)) minValue = 0.0
            if (isNaN(maxValue)) maxValue = 1.0
            ranges.push({
                name: row.name,
                min: minValue,
                max: maxValue
            })
        }
        return ranges
    }

    Component.onCompleted: loadDeployedModelInputs()

    ListModel { id: inputModel }

    Connections {
        target: (typeof appState !== "undefined") ? appState : null
        function onLastModelChanged() { root.loadDeployedModelInputs() }
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
                    backend.startBenchmarkWithInputs(parseInt(samplesField.text) || 20,
                                                     root.collectInputRanges(),
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
                    Layout.preferredHeight: 460

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: Theme.spacingMd

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: Theme.spacingXs
                            Text {
                                text: root._inputSpecsLoaded ? "Karttaki model inputlari okundu" : "Karttaki model icin input raporu bulunamadi"
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
                                        text: root._modelName.length > 0 ? root._modelName : "Karttaki model bilinmiyor"
                                        color: root._modelName.length > 0 ? Theme.text : Theme.textFaint
                                        font.family: Theme.fontFamily
                                        font.pixelSize: Theme.fontSm
                                        elide: Text.ElideLeft
                                    }
                                }
                                AppButton { text: "Yenile"; variant: "secondary"; onClicked: root.loadDeployedModelInputs() }
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

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: Theme.spacingXs
                            Text { text: "Input araliklari"; color: Theme.textMuted; font.family: Theme.fontFamily; font.pixelSize: Theme.fontXs; font.weight: Font.DemiBold }
                            ScrollView {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 128
                                clip: true

                                ColumnLayout {
                                    width: parent.width
                                    spacing: Theme.spacingXs
                                    Repeater {
                                        model: inputModel
                                        delegate: RowLayout {
                                            Layout.fillWidth: true
                                            spacing: Theme.spacingSm
                                            Text {
                                                Layout.fillWidth: true
                                                text: (model.label && model.label.length > 0 ? model.label : model.name)
                                                      + (model.label && model.label.length > 0 ? "  -  " + model.name : "")
                                                      + (model.shape !== "-" ? "  [" + model.shape + "]" : "")
                                                color: Theme.text
                                                font.family: Theme.fontFamily
                                                font.pixelSize: Theme.fontXs
                                                elide: Text.ElideRight
                                            }
                                            TextField {
                                                id: minInput
                                                Layout.preferredWidth: 84
                                                text: model.min
                                                enabled: !root._busy
                                                selectByMouse: true
                                                color: Theme.text
                                                font.family: Theme.fontFamily
                                                font.pixelSize: Theme.fontXs
                                                background: Rectangle { radius: Theme.radiusMd; color: Theme.surfaceRaised; border.color: minInput.activeFocus ? Theme.primary : Theme.border }
                                                leftPadding: Theme.spacingXs; rightPadding: Theme.spacingXs
                                                onEditingFinished: inputModel.setProperty(index, "min", text)
                                            }
                                            TextField {
                                                id: maxInput
                                                Layout.preferredWidth: 84
                                                text: model.max
                                                enabled: !root._busy
                                                selectByMouse: true
                                                color: Theme.text
                                                font.family: Theme.fontFamily
                                                font.pixelSize: Theme.fontXs
                                                background: Rectangle { radius: Theme.radiusMd; color: Theme.surfaceRaised; border.color: maxInput.activeFocus ? Theme.primary : Theme.border }
                                                leftPadding: Theme.spacingXs; rightPadding: Theme.spacingXs
                                                onEditingFinished: inputModel.setProperty(index, "max", text)
                                            }
                                        }
                                    }
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
                                    backend.startBenchmarkWithInputs(parseInt(samplesField.text) || 20,
                                                                     root.collectInputRanges(),
                                                                     parseInt(seedField.text) || 42)
                                }
                            }
                            AppButton {
                                text: "Iptal"
                                variant: "danger"
                                enabled: root._busy
                                onClicked: if (typeof backend !== "undefined" && backend) backend.cancelBenchmark()
                            }
                            AppButton { text: "Input Yenile"; variant: "secondary"; onClicked: root.loadDeployedModelInputs() }
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
}
