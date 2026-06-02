import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import STM32AiDeployer

Item {
    id: root

    property string _fwPath: ""
    property string _fwInfo: ""
    property int _sourceMode: 0 // 0=ready firmware, 1=AI model pipeline
    property int _pipeStep: 0
    property string _modelPath: ""
    property string _modelName: ""
    property string _pipeOutputDir: ""
    property string _pipeQuant: "INT8"
    property string _sensorType: "BME280"
    property string _i2cInstance: "I2C1"
    property string _i2cAddress: "0x76"
    property string _sdaPort: "GPIOB"
    property string _sdaPin: "GPIO_PIN_9"
    property string _sclPort: "GPIOB"
    property string _sclPin: "GPIO_PIN_8"
    property string _saiInstance: "SAI1"
    property string _clkPort: "GPIOB"
    property string _clkPin: "GPIO_PIN_5"
    property string _dataPort: "GPIOB"
    property string _dataPin: "GPIO_PIN_3"
    readonly property var _arch: ["1D CNN", "MLP", "LSTM", "TC-ResNet"]
    readonly property var _quant: ["INT8", "Dynamic Q", "Float32"]
    readonly property var _sensorOptions: ["BME280 (I2C)", "MPU6050 (I2C)", "PDM Mikrofon (SAI)"]
    readonly property string _activeBoard: (typeof appState !== "undefined" && appState && appState.boardName.length > 0) ? appState.boardName : "STM32F4"
    readonly property string _activePort: (typeof appState !== "undefined" && appState && appState.activePort.length > 0) ? appState.activePort : ""
    readonly property var _lines: (typeof backend !== "undefined" && backend && backend.flashLines.length > 0)
                                  ? backend.flashLines
                                  : ((typeof backend !== "undefined" && backend && backend.pipelineLines.length > 0)
                                     ? backend.pipelineLines : MockData.flashTerminal)
    readonly property int  _progress: (typeof backend !== "undefined" && backend)
                                      ? (backend.flashBusy ? backend.flashProgress : backend.pipelineProgress) : 100
    readonly property bool _busy: (typeof backend !== "undefined" && backend) ? (backend.flashBusy || backend.pipelineBusy) : false
    readonly property bool _connected: (typeof appState !== "undefined" && appState) ? appState.connected : true

    function pipelineConfig() {
        var isPdm = root._sensorType === "PDM" || root._sensorType === "PDM_MIC"
        return {
            modelPath: root._modelPath,
            modelName: root._modelName.length > 0 ? root._modelName : root._modelPath.split(/[\\/]/).pop().replace(/\.\w+$/,""),
            architecture: "Bilinmiyor",
            quantization: root._pipeQuant,
            sensorType: isPdm ? "PDM_MIC" : root._sensorType,
            protocol: isPdm ? "SAI" : "I2C",
            targetBoard: root._activeBoard,
            outputDir: root._pipeOutputDir,
            i2cInstance: root._i2cInstance,
            sdaPort: root._sdaPort,
            sdaPin: root._sdaPin,
            sclPort: root._sclPort,
            sclPin: root._sclPin,
            i2cAddress: root._i2cAddress,
            saiInstance: root._saiInstance,
            clkPort: root._clkPort,
            clkPin: root._clkPin,
            dataPort: root._dataPort,
            dataPin: root._dataPin
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacingLg
        spacing: Theme.spacingLg

        RowLayout {
            Layout.fillWidth: true
            SectionHeader {
                title: "Model & Flash"
                subtitle: "Firmware üret, derle ve karta yükle"
                Layout.fillWidth: true
            }
            AppButton {
                text: "Pipeline Sihirbazı"
                iconText: "✦"
                visible: false
                onClicked: root._sourceMode = 1
            }
        }

        GridLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            columns: root.width > 1100 ? 2 : 1
            columnSpacing: Theme.spacingMd
            rowSpacing: Theme.spacingMd

            // ── Left: configuration ───────────────────────────────────────
            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: Theme.spacingMd

                Card {
                    title: "Firmware Kaynağı"
                    Layout.fillWidth: true
                    Layout.preferredHeight: 110

                    RowLayout {
                        anchors.fill: parent
                        spacing: Theme.spacingSm

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            radius: Theme.radiusMd
                            color: root._sourceMode === 0 ? Theme.primarySoft : Theme.surfaceRaised
                            border.color: root._sourceMode === 0 ? Theme.primary : Theme.border
                            border.width: root._sourceMode === 0 ? 2 : 1
                            Text {
                                anchors.centerIn: parent
                                text: "Hazır Firmware\n.hex / .bin / .elf"
                                horizontalAlignment: Text.AlignHCenter
                                color: Theme.text
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSm
                                font.weight: Font.DemiBold
                            }
                        }
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            radius: Theme.radiusMd
                            color: root._sourceMode === 1 ? Theme.primarySoft : Theme.surfaceRaised
                            border.color: root._sourceMode === 1 ? Theme.primary : Theme.border
                            border.width: root._sourceMode === 1 ? 2 : 1
                            Text {
                                anchors.centerIn: parent
                                text: "AI Modelinden Üret\n.tflite / .h5"
                                horizontalAlignment: Text.AlignHCenter
                                color: Theme.textMuted
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSm
                            }
                            MouseArea { anchors.fill: parent; onClicked: root._sourceMode = 1 }
                        }
                    }
                }

                Card {
                    title: "Model Bilgileri"
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: Theme.spacingMd
                        visible: root._sourceMode === 0

                        // editable model name
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: Theme.spacingXs
                            Text {
                                text: "Model Adı"
                                color: Theme.textMuted
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontXs
                                font.weight: Font.DemiBold
                            }
                            TextField {
                                id: modelNameField
                                Layout.fillWidth: true
                                text: "anomaly_cnn"
                                color: Theme.text
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSm
                                selectByMouse: true
                                background: Rectangle {
                                    radius: Theme.radiusMd
                                    color: Theme.surfaceRaised
                                    border.color: modelNameField.activeFocus ? Theme.primary : Theme.border
                                }
                                leftPadding: Theme.spacingSm
                                rightPadding: Theme.spacingSm
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Theme.spacingMd
                            ComboField { id: archField; label: "Mimari"; options: root._arch }
                            ComboField { id: quantField; label: "Kuantizasyon"; options: root._quant }
                        }

                        // firmware path + pick
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: Theme.spacingXs
                            Text {
                                text: "Firmware Dosyası"
                                color: Theme.textMuted
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontXs
                                font.weight: Font.DemiBold
                            }
                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 52
                                radius: Theme.radiusMd
                                color: Theme.surfaceRaised
                                border.color: Theme.border
                                RowLayout {
                                    anchors.fill: parent
                                    anchors.margins: Theme.spacingMd
                                    Text {
                                        text: root._fwPath.length > 0
                                              ? root._fwPath.split(/[\\/]/).pop() : "Dosya seçilmedi"
                                        color: root._fwPath.length > 0 ? Theme.text : Theme.textFaint
                                        font.family: Theme.fontFamily
                                        font.pixelSize: Theme.fontSm
                                        elide: Text.ElideMiddle
                                        Layout.fillWidth: true
                                    }
                                    Text {
                                        visible: root._fwInfo.length > 0
                                        text: root._fwInfo
                                        color: Theme.textMuted
                                        font.family: Theme.fontFamily
                                        font.pixelSize: Theme.fontXs
                                    }
                                }
                            }
                        }

                        CheckBox {
                            id: simCheck
                            text: "Simülasyon modu (karta gerçekten yazma)"
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSm
                            contentItem: Text {
                                text: simCheck.text
                                color: Theme.textMuted
                                font: simCheck.font
                                leftPadding: simCheck.indicator.width + Theme.spacingXs
                                verticalAlignment: Text.AlignVCenter
                            }
                        }

                        Item { Layout.fillHeight: true }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Theme.spacingSm
                            AppButton {
                                text: root._busy ? "Yükleniyor…" : "Karta Yükle"
                                iconText: "▲"
                                enabled: !root._busy && root._fwPath.length > 0
                                onClicked: {
                                    if (typeof backend === "undefined" || !backend) return
                                    backend.flashFirmware(root._fwPath, modelNameField.text,
                                                          root._arch[archField.currentIndex],
                                                          root._quant[quantField.currentIndex],
                                                          simCheck.checked)
                                }
                            }
                            AppButton {
                                text: "Dosya Seç"
                                iconText: "📁"
                                variant: "secondary"
                                onClicked: fwDialog.open()
                            }
                            AppButton {
                                visible: root._busy
                                text: "İptal"
                                variant: "danger"
                                onClicked: if (typeof backend !== "undefined" && backend) backend.cancelFlash()
                            }
                            Item { Layout.fillWidth: true }
                            StatusPill {
                                text: root._connected ? "ST-Link bağlı" : "ST-Link yok"
                                status: root._connected ? "connected" : "offline"
                            }
                        }
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: Theme.spacingMd
                        visible: root._sourceMode === 1

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Theme.spacingXs
                            Repeater {
                                model: ["Model", "Sensor", "Arac", "Baslat"]
                                delegate: Rectangle {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 28
                                    radius: Theme.radiusSm
                                    color: index === root._pipeStep ? Theme.primarySoft : Theme.surfaceRaised
                                    border.color: index === root._pipeStep ? Theme.primary : Theme.border
                                    Text {
                                        anchors.centerIn: parent
                                        text: (index + 1) + ". " + modelData
                                        color: index === root._pipeStep ? Theme.text : Theme.textMuted
                                        font.family: Theme.fontFamily
                                        font.pixelSize: Theme.fontXs
                                        font.weight: index === root._pipeStep ? Font.DemiBold : Font.Normal
                                    }
                                }
                            }
                        }

                        StackLayout {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            currentIndex: root._pipeStep

                            ColumnLayout {
                                spacing: Theme.spacingMd
                                Text { text: "AI modeli sec"; color: Theme.text; font.family: Theme.fontFamily; font.pixelSize: Theme.fontMd; font.weight: Font.DemiBold }
                                RowLayout {
                                    Layout.fillWidth: true
                                    Rectangle {
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 42
                                        radius: Theme.radiusMd
                                        color: Theme.surfaceRaised
                                        border.color: Theme.border
                                        Text {
                                            anchors.verticalCenter: parent.verticalCenter
                                            anchors.left: parent.left
                                            anchors.leftMargin: Theme.spacingSm
                                            width: parent.width - Theme.spacingMd
                                            text: root._modelPath.length > 0 ? root._modelPath.split(/[\\/]/).pop() : "model.tflite / .h5 secin"
                                            color: root._modelPath.length > 0 ? Theme.text : Theme.textFaint
                                            font.family: Theme.fontFamily
                                            font.pixelSize: Theme.fontSm
                                            elide: Text.ElideLeft
                                        }
                                    }
                                    AppButton { text: "Sec"; variant: "secondary"; onClicked: aiModelDialog.open() }
                                }
                                ComboField {
                                    label: "Kuantizasyon"
                                    options: root._quant
                                    onActivated: (idx) => root._pipeQuant = root._quant[idx]
                                }
                                PinInput {
                                    Layout.fillWidth: true
                                    text: root._modelName
                                    placeholderText: "Model adi"
                                    onTextChanged: root._modelName = text
                                }
                                Item { Layout.fillHeight: true }
                            }

                            ColumnLayout {
                                spacing: Theme.spacingMd
                                Text { text: "Kart ve sensor"; color: Theme.text; font.family: Theme.fontFamily; font.pixelSize: Theme.fontMd; font.weight: Font.DemiBold }
                                RowLayout {
                                    Layout.fillWidth: true
                                    StatusPill { text: root._activeBoard; status: root._activeBoard.indexOf("N6") >= 0 ? "experimental" : "ready" }
                                    StatusPill { text: root._activePort.length > 0 ? root._activePort : "COM yok"; status: root._activePort.length > 0 ? "connected" : "offline" }
                                    Item { Layout.fillWidth: true }
                                }
                                ComboField {
                                    label: "Sensor tipi"
                                    options: root._sensorOptions
                                    onActivated: (idx) => {
                                        root._sensorType = root._sensorOptions[idx].split(" ")[0]
                                        root._i2cAddress = root._sensorType === "MPU6050" ? "0xD0" : "0x76"
                                    }
                                }
                                ColumnLayout {
                                    visible: root._sensorType !== "PDM"
                                    Layout.fillWidth: true
                                    spacing: Theme.spacingSm
                                    RowLayout { Layout.fillWidth: true; spacing: Theme.spacingSm
                                        PinInput { Layout.fillWidth: true; text: root._i2cInstance; placeholderText: "I2C1"; onTextChanged: root._i2cInstance = text }
                                        PinInput { Layout.fillWidth: true; text: root._i2cAddress; placeholderText: "0x76"; onTextChanged: root._i2cAddress = text }
                                    }
                                    RowLayout { Layout.fillWidth: true; spacing: Theme.spacingSm
                                        PinInput { Layout.fillWidth: true; text: root._sdaPort; placeholderText: "SDA Port"; onTextChanged: root._sdaPort = text }
                                        PinInput { Layout.fillWidth: true; text: root._sdaPin; placeholderText: "SDA Pin"; onTextChanged: root._sdaPin = text }
                                    }
                                    RowLayout { Layout.fillWidth: true; spacing: Theme.spacingSm
                                        PinInput { Layout.fillWidth: true; text: root._sclPort; placeholderText: "SCL Port"; onTextChanged: root._sclPort = text }
                                        PinInput { Layout.fillWidth: true; text: root._sclPin; placeholderText: "SCL Pin"; onTextChanged: root._sclPin = text }
                                    }
                                }
                                ColumnLayout {
                                    visible: root._sensorType === "PDM"
                                    Layout.fillWidth: true
                                    spacing: Theme.spacingSm
                                    RowLayout { Layout.fillWidth: true; spacing: Theme.spacingSm
                                        PinInput { Layout.fillWidth: true; text: root._saiInstance; placeholderText: "SAI1"; onTextChanged: root._saiInstance = text }
                                        PinInput { Layout.fillWidth: true; text: root._clkPort; placeholderText: "CLK Port"; onTextChanged: root._clkPort = text }
                                    }
                                    RowLayout { Layout.fillWidth: true; spacing: Theme.spacingSm
                                        PinInput { Layout.fillWidth: true; text: root._clkPin; placeholderText: "CLK Pin"; onTextChanged: root._clkPin = text }
                                        PinInput { Layout.fillWidth: true; text: root._dataPort; placeholderText: "DATA Port"; onTextChanged: root._dataPort = text }
                                        PinInput { Layout.fillWidth: true; text: root._dataPin; placeholderText: "DATA Pin"; onTextChanged: root._dataPin = text }
                                    }
                                }
                                Item { Layout.fillHeight: true }
                            }

                            ColumnLayout {
                                spacing: Theme.spacingSm
                                Text { text: "Arac dogrulama ve cikti dizini"; color: Theme.text; font.family: Theme.fontFamily; font.pixelSize: Theme.fontMd; font.weight: Font.DemiBold }
                                Repeater {
                                    model: typeof backend !== "undefined" && backend ? backend.toolPaths : []
                                    delegate: Rectangle {
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 44
                                        radius: Theme.radiusMd
                                        color: Theme.surfaceRaised
                                        border.color: Theme.border
                                        RowLayout {
                                            anchors.fill: parent
                                            anchors.margins: Theme.spacingSm
                                            spacing: Theme.spacingSm
                                            Rectangle { Layout.preferredWidth: 9; Layout.preferredHeight: 9; radius: 5; color: modelData.found ? Theme.success : Theme.danger }
                                            ColumnLayout {
                                                Layout.fillWidth: true
                                                spacing: 1
                                                Text { Layout.fillWidth: true; text: modelData.name; color: Theme.text; font.family: Theme.fontFamily; font.pixelSize: Theme.fontXs; font.weight: Font.DemiBold; elide: Text.ElideRight }
                                                Text { Layout.fillWidth: true; text: modelData.found ? modelData.path : "Bulunamadi"; color: modelData.found ? Theme.textFaint : Theme.danger; font.family: Theme.monoFamily; font.pixelSize: Theme.fontXs; elide: Text.ElideMiddle }
                                            }
                                        }
                                    }
                                }
                                RowLayout {
                                    Layout.fillWidth: true
                                    Rectangle {
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 38
                                        radius: Theme.radiusMd
                                        color: Theme.surfaceRaised
                                        border.color: root._pipeOutputDir.length > 0 ? Theme.border : Theme.warning
                                        Text {
                                            anchors.verticalCenter: parent.verticalCenter
                                            anchors.left: parent.left
                                            anchors.leftMargin: Theme.spacingSm
                                            width: parent.width - Theme.spacingMd
                                            text: root._pipeOutputDir.length > 0 ? root._pipeOutputDir : "Cikti dizini secin"
                                            color: root._pipeOutputDir.length > 0 ? Theme.text : Theme.warning
                                            font.family: Theme.fontFamily
                                            font.pixelSize: Theme.fontSm
                                            elide: Text.ElideLeft
                                        }
                                    }
                                    AppButton { text: "Dizin"; variant: "secondary"; onClicked: aiOutputDialog.open() }
                                }
                                Item { Layout.fillHeight: true }
                            }

                            ColumnLayout {
                                spacing: Theme.spacingSm
                                Text { text: "Derleme ve flash"; color: Theme.text; font.family: Theme.fontFamily; font.pixelSize: Theme.fontMd; font.weight: Font.DemiBold }
                                Repeater {
                                    model: [
                                        ["Model", root._modelPath.length > 0 ? root._modelPath.split(/[\\/]/).pop() : "-"],
                                        ["Kart", root._activeBoard],
                                        ["COM", root._activePort.length > 0 ? root._activePort : "-"],
                                        ["Sensor", root._sensorType],
                                        ["Cikti", root._pipeOutputDir.length > 0 ? root._pipeOutputDir : "-"]
                                    ]
                                    delegate: RowLayout {
                                        Layout.fillWidth: true
                                        Text { text: modelData[0]; color: Theme.textMuted; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm; Layout.preferredWidth: 78 }
                                        Text { text: modelData[1]; color: Theme.text; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm; font.weight: Font.DemiBold; Layout.fillWidth: true; elide: Text.ElideMiddle }
                                    }
                                }
                                Item { Layout.fillHeight: true }
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            AppButton {
                                text: "Geri"
                                variant: "secondary"
                                enabled: root._pipeStep > 0 && !(typeof backend !== "undefined" && backend && backend.pipelineBusy)
                                onClicked: root._pipeStep--
                            }
                            Item { Layout.fillWidth: true }
                            AppButton {
                                text: root._pipeStep < 3 ? "Ileri" : ((typeof backend !== "undefined" && backend && backend.pipelineBusy) ? "Calisiyor..." : "Pipeline Baslat")
                                enabled: !(typeof backend !== "undefined" && backend && backend.pipelineBusy)
                                onClicked: {
                                    if (root._pipeStep === 0 && root._modelPath.length === 0) { aiModelDialog.open(); return }
                                    if (root._pipeStep === 2 && root._pipeOutputDir.length === 0) { aiOutputDialog.open(); return }
                                    if (root._pipeStep < 3) { root._pipeStep++; return }
                                    if (root._pipeOutputDir.length === 0) { aiOutputDialog.open(); return }
                                    if (typeof backend !== "undefined" && backend) backend.runPipeline(root.pipelineConfig())
                                }
                            }
                            AppButton {
                                visible: typeof backend !== "undefined" && backend && backend.pipelineBusy
                                text: "Iptal"
                                variant: "danger"
                                onClicked: backend.cancelPipeline()
                            }
                        }
                    }
                }
            }

            // ── Right: terminal ───────────────────────────────────────────
            Card {
                title: "Çıktı Terminali"
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
                        ProgressShim { Layout.fillWidth: true; value: root._progress / 100 }
                        AppButton {
                            text: "Temizle"
                            variant: "secondary"
                            onClicked: {
                                if (typeof backend === "undefined" || !backend) return
                                backend.clearFlashLog()
                                backend.clearPipelineLog()
                            }
                        }
                    }
                }
            }
        }
    }

    // ── File picker ────────────────────────────────────────────────────────
    FileDialog {
        id: fwDialog
        title: "Firmware dosyası seçin"
        nameFilters: ["Firmware (*.hex *.bin *.elf)", "Tüm dosyalar (*)"]
        onAccepted: {
            var p = selectedFile.toString().replace("file:///", "")
            root._fwPath = p
            if (typeof backend !== "undefined" && backend)
                root._fwInfo = backend.fileInfo(p)
        }
    }

    FileDialog {
        id: aiModelDialog
        title: "AI model dosyasi secin"
        nameFilters: ["AI Model (*.tflite *.h5 *.onnx)", "Tum dosyalar (*)"]
        onAccepted: {
            var p = selectedFile.toString().replace("file:///", "")
            root._modelPath = p
            if (root._modelName.length === 0)
                root._modelName = p.split(/[\\/]/).pop().replace(/\.\w+$/,"")
        }
    }

    FolderDialog {
        id: aiOutputDialog
        title: "Pipeline cikti dizini secin"
        onAccepted: root._pipeOutputDir = selectedFolder.toString().replace("file:///", "")
    }

    // small inline progress bar component
    component ProgressShim: Rectangle {
        property real value: 0.0
        implicitHeight: 20
        radius: Theme.radiusSm
        color: Theme.surfaceRaised
        border.color: Theme.border
        Rectangle {
            width: parent.width * Math.max(0, Math.min(parent.value, 1))
            height: parent.height
            radius: parent.radius
            color: Theme.success
            Behavior on width { NumberAnimation { duration: Theme.animNormal } }
        }
        Text {
            anchors.centerIn: parent
            text: Math.round(parent.value * 100) + "%"
            color: Theme.text
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontXs
            font.weight: Font.DemiBold
        }
    }

    component PinInput: TextField {
        implicitHeight: 38
        color: Theme.text
        placeholderTextColor: Theme.textFaint
        font.family: Theme.monoFamily
        font.pixelSize: Theme.fontSm
        selectByMouse: true
        verticalAlignment: Text.AlignVCenter
        leftPadding: Theme.spacingSm
        rightPadding: Theme.spacingSm
        background: Rectangle {
            radius: Theme.radiusMd
            color: Theme.surfaceRaised
            border.color: parent.activeFocus ? Theme.primary : Theme.border
        }
    }
}
