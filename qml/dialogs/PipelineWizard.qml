import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import STM32AiDeployer

Popup {
    id: root
    modal: true
    anchors.centerIn: Overlay.overlay
    width: 680
    height: 540
    padding: 0
    closePolicy: Popup.CloseOnEscape

    Overlay.modal: Rectangle { color: Theme.alpha("#000000", 0.55) }

    // Wizard state
    property int  step: 0       // 0-3
    property string modelPath: ""
    property string modelName: ""
    property string quantization: "INT8"
    property string sensorType: "BME280"
    property string targetBoard: ""
    property string outputDir: ""
    property string i2cInstance: "I2C1"
    property string sdaPort: "GPIOB"
    property string sdaPin: "GPIO_PIN_9"
    property string sclPort: "GPIOB"
    property string sclPin: "GPIO_PIN_8"
    property string i2cAddress: "0x76"
    property string saiInstance: "SAI1"
    property string clkPort: "GPIOB"
    property string clkPin: "GPIO_PIN_5"
    property string dataPort: "GPIOB"
    property string dataPin: "GPIO_PIN_3"
    readonly property string activeBoardName: (typeof appState !== "undefined" && appState && appState.boardName.length > 0)
                                             ? appState.boardName : ""
    readonly property string activePortName: (typeof appState !== "undefined" && appState && appState.activePort.length > 0)
                                            ? appState.activePort : ""

    readonly property var steps: ["Model Seç", "Sensör Ayarla", "Araç Doğrulama", "Derleme & Flash"]
    readonly property var quantOptions: ["INT8", "Dynamic Q", "Float32"]
    readonly property var sensorOptions: ["BME280 (I2C)", "MPU6050 (I2C)", "PDM Mikrofon (SAI)"]
    function currentBoardOptions() {
        if (typeof appState !== "undefined" && appState && appState.boardName.length > 0)
            return [appState.boardName, "STM32F4", "STM32H7", "STM32N6", "NUCLEO-N657X0-Q"]
        return ["STM32F4", "STM32H7", "STM32N6", "NUCLEO-N657X0-Q"]
    }
    function refreshFromAppState() {
        if (root.activeBoardName.length > 0)
            root.targetBoard = root.activeBoardName
        else if (root.targetBoard.length === 0)
            root.targetBoard = "STM32F4"
    }

    function pipelineConfig() {
        var isPdm = root.sensorType === "PDM" || root.sensorType === "PDM_MIC"
        return {
            modelPath: root.modelPath,
            modelName: root.modelName.length > 0 ? root.modelName : root.modelPath.split(/[\\/]/).pop().replace(/\.\w+$/,""),
            architecture: "Bilinmiyor",
            quantization: root.quantization,
            sensorType: isPdm ? "PDM_MIC" : root.sensorType,
            protocol: isPdm ? "SAI" : "I2C",
            targetBoard: root.targetBoard || root.activeBoardName || "STM32F4",
            outputDir: root.outputDir,
            i2cInstance: root.i2cInstance,
            sdaPort: root.sdaPort,
            sdaPin: root.sdaPin,
            sclPort: root.sclPort,
            sclPin: root.sclPin,
            i2cAddress: root.i2cAddress,
            saiInstance: root.saiInstance,
            clkPort: root.clkPort,
            clkPin: root.clkPin,
            dataPort: root.dataPort,
            dataPin: root.dataPin
        }
    }

    background: Rectangle {
        color: Theme.surface; radius: Theme.radiusLg; border.color: Theme.border
    }

    contentItem: ColumnLayout {
        spacing: 0

        // ── Title bar ─────────────────────────────────────────────────────
        Rectangle {
            Layout.fillWidth: true; Layout.preferredHeight: 58
            color: Theme.bgElevated; radius: Theme.radiusLg
            Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: parent.radius; color: parent.color }
            Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.border }

            RowLayout {
                anchors.fill: parent; anchors.margins: Theme.spacingMd; spacing: Theme.spacingMd
                Text {
                    text: "Pipeline Sihirbazı — Adım " + (root.step + 1) + " / 4"
                    color: Theme.text; font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontMd; font.weight: Font.DemiBold
                    Layout.fillWidth: true
                }
                Text { text: root.steps[root.step]; color: Theme.primary; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm }
                Rectangle {
                    Layout.preferredWidth: 32; Layout.preferredHeight: 32; radius: Theme.radiusSm
                    color: xMouse.containsMouse ? Theme.danger : "transparent"
                    Text { anchors.centerIn: parent; text: "✕"; color: xMouse.containsMouse ? "#FFF" : Theme.textMuted; font.pixelSize: Theme.fontSm }
                    MouseArea { id: xMouse; anchors.fill: parent; hoverEnabled: true; onClicked: root.close() }
                }
            }
        }

        // ── Step indicator ────────────────────────────────────────────────
        RowLayout {
            Layout.fillWidth: true; Layout.margins: Theme.spacingMd; spacing: Theme.spacingMd
            Repeater {
                model: root.steps
                delegate: RowLayout {
                    spacing: Theme.spacingXs
                    Rectangle {
                        Layout.preferredWidth: 26; Layout.preferredHeight: 26; radius: 13
                        color: index <= root.step ? Theme.primary : Theme.surfaceRaised
                        border.color: index <= root.step ? Theme.primary : Theme.border
                        Text { anchors.centerIn: parent; text: String(index + 1)
                            color: index <= root.step ? "#FFF" : Theme.textFaint
                            font.family: Theme.fontFamily; font.pixelSize: Theme.fontXs; font.weight: Font.Bold }
                    }
                    Text {
                        text: modelData; color: index === root.step ? Theme.text : Theme.textFaint
                        font.family: Theme.fontFamily; font.pixelSize: Theme.fontXs
                        font.weight: index === root.step ? Font.DemiBold : Font.Normal
                    }
                    Rectangle {
                        visible: index < root.steps.length - 1
                        Layout.preferredWidth: 24; Layout.preferredHeight: 2
                        color: index < root.step ? Theme.primary : Theme.border
                    }
                }
            }
        }

        Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Theme.border }

        // ── Page content ──────────────────────────────────────────────────
        StackLayout {
            Layout.fillWidth: true; Layout.fillHeight: true; Layout.margins: Theme.spacingLg
            currentIndex: root.step

            // ── Step 0: Model ─────────────────────────────────────────────
            ColumnLayout {
                spacing: Theme.spacingMd

                Text { text: "Flash atılacak .tflite veya .h5 modelini seçin."
                    color: Theme.textMuted; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm }

                RowLayout {
                    Layout.fillWidth: true; spacing: Theme.spacingSm
                    Rectangle {
                        Layout.fillWidth: true; Layout.preferredHeight: 40
                        radius: Theme.radiusMd; color: Theme.surfaceRaised; border.color: Theme.border
                        Text {
                            anchors.verticalCenter: parent.verticalCenter; anchors.left: parent.left; anchors.leftMargin: Theme.spacingSm
                            text: root.modelPath.length > 0 ? root.modelPath.split(/[\\/]/).pop() : "model.tflite seçin…"
                            color: root.modelPath.length > 0 ? Theme.text : Theme.textFaint
                            font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm
                            elide: Text.ElideLeft; width: parent.width - Theme.spacingMd
                        }
                    }
                    AppButton { text: "Gözat"; variant: "secondary"; onClicked: modelPickDlg.open() }
                }

                ComboField { label: "Kuantizasyon"; options: root.quantOptions
                    onActivated: (i) => root.quantization = root.quantOptions[i] }

                ColumnLayout { spacing: Theme.spacingXs; Layout.fillWidth: true
                    Text { text: "Model Adı (opsiyonel)"; color: Theme.textMuted; font.family: Theme.fontFamily; font.pixelSize: Theme.fontXs; font.weight: Font.DemiBold }
                    TextField { id: modelNameField; Layout.fillWidth: true
                        text: root.modelName; placeholderText: "anomaly_cnn"
                        color: Theme.text; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm
                        onTextChanged: root.modelName = text
                        background: Rectangle { radius: Theme.radiusMd; color: Theme.surfaceRaised; border.color: modelNameField.activeFocus ? Theme.primary : Theme.border }
                        leftPadding: Theme.spacingSm; rightPadding: Theme.spacingSm }
                }

            }

            // ── Step 1: Sensor ────────────────────────────────────────────
            ColumnLayout {
                spacing: Theme.spacingMd

                Text { text: "Kartınıza bağlı sensörü ve hedef kartı seçin."
                    color: Theme.textMuted; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm }

                ComboField { label: "Hedef Kart"; options: root.currentBoardOptions()
                    Component.onCompleted: {
                        root.refreshFromAppState()
                    }
                    onActivated: (i) => root.targetBoard = root.currentBoardOptions()[i]
                }

                ComboField { label: "Sensör Tipi"; options: root.sensorOptions
                    onActivated: (i) => {
                        root.sensorType = root.sensorOptions[i].split(" ")[0]
                        root.i2cAddress = root.sensorType === "MPU6050" ? "0xD0" : "0x76"
                    }
                }

                Card {
                    title: "Pin Konfigürasyonu"
                    Layout.fillWidth: true; Layout.preferredHeight: 180
                    ColumnLayout {
                        anchors.fill: parent; spacing: Theme.spacingSm
                        visible: root.sensorType !== "PDM"
                        RowLayout { Layout.fillWidth: true; spacing: Theme.spacingSm
                            PinField { Layout.fillWidth: true; text: root.i2cInstance; placeholderText: "I2C1"; onTextChanged: root.i2cInstance = text }
                            PinField { Layout.fillWidth: true; text: root.i2cAddress; placeholderText: "0x76"; onTextChanged: root.i2cAddress = text }
                        }
                        RowLayout { Layout.fillWidth: true; spacing: Theme.spacingSm
                            PinField { Layout.fillWidth: true; text: root.sdaPort; placeholderText: "SDA Port"; onTextChanged: root.sdaPort = text }
                            PinField { Layout.fillWidth: true; text: root.sdaPin; placeholderText: "SDA Pin"; onTextChanged: root.sdaPin = text }
                        }
                        RowLayout { Layout.fillWidth: true; spacing: Theme.spacingSm
                            PinField { Layout.fillWidth: true; text: root.sclPort; placeholderText: "SCL Port"; onTextChanged: root.sclPort = text }
                            PinField { Layout.fillWidth: true; text: root.sclPin; placeholderText: "SCL Pin"; onTextChanged: root.sclPin = text }
                        }
                    }
                    ColumnLayout {
                        anchors.fill: parent; spacing: Theme.spacingSm
                        visible: root.sensorType === "PDM"
                        RowLayout { Layout.fillWidth: true; spacing: Theme.spacingSm
                            PinField { Layout.fillWidth: true; text: root.saiInstance; placeholderText: "SAI1"; onTextChanged: root.saiInstance = text }
                            PinField { Layout.fillWidth: true; text: root.clkPort; placeholderText: "CLK Port"; onTextChanged: root.clkPort = text }
                            PinField { Layout.fillWidth: true; text: root.clkPin; placeholderText: "CLK Pin"; onTextChanged: root.clkPin = text }
                        }
                        RowLayout { Layout.fillWidth: true; spacing: Theme.spacingSm
                            PinField { Layout.fillWidth: true; text: root.dataPort; placeholderText: "DATA Port"; onTextChanged: root.dataPort = text }
                            PinField { Layout.fillWidth: true; text: root.dataPin; placeholderText: "DATA Pin"; onTextChanged: root.dataPin = text }
                        }
                    }
                }
            }

            // ── Step 2: Tool validation ───────────────────────────────────
            ColumnLayout {
                spacing: Theme.spacingSm

                Item { Layout.fillHeight: true }

                Text { text: "Araç yollarını ve çıktı dizinini doğrulayın."
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    color: Theme.textMuted; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm }

                Repeater {
                    model: typeof backend !== "undefined" && backend ? backend.toolPaths : []
                    delegate: Rectangle {
                        Layout.fillWidth: true; Layout.preferredHeight: 54
                        radius: Theme.radiusMd; color: Theme.surfaceRaised; border.color: Theme.border
                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: Theme.spacingMd; anchors.rightMargin: Theme.spacingMd
                            anchors.topMargin: Theme.spacingXs; anchors.bottomMargin: Theme.spacingXs
                            spacing: Theme.spacingMd
                            Rectangle {
                                Layout.preferredWidth: 10; Layout.preferredHeight: 10; radius: 5
                                Layout.alignment: Qt.AlignVCenter
                                color: modelData.found ? Theme.success : Theme.danger
                            }
                            ColumnLayout { spacing: 2; Layout.fillWidth: true
                                Layout.alignment: Qt.AlignVCenter
                                Text { text: modelData.name; color: Theme.text; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm; font.weight: Font.DemiBold }
                                Text { text: modelData.found ? modelData.path : "Bulunamadı — Ayarlar'dan ekleyin"; color: modelData.found ? Theme.textFaint : Theme.danger; font.family: Theme.monoFamily; font.pixelSize: Theme.fontXs; elide: Text.ElideMiddle; Layout.fillWidth: true }
                            }
                        }
                    }
                }

                ColumnLayout { spacing: Theme.spacingXs; Layout.fillWidth: true; Layout.topMargin: Theme.spacingSm
                    Text { text: "Çıktı Dizini"; color: Theme.textMuted; font.family: Theme.fontFamily; font.pixelSize: Theme.fontXs; font.weight: Font.DemiBold }
                    RowLayout { Layout.fillWidth: true; spacing: Theme.spacingSm
                        Rectangle {
                            Layout.fillWidth: true; Layout.preferredHeight: 36
                            radius: Theme.radiusMd; color: Theme.surfaceRaised; border.color: Theme.border
                            Text { anchors.verticalCenter: parent.verticalCenter; anchors.left: parent.left; anchors.leftMargin: Theme.spacingSm
                                text: root.outputDir.length > 0 ? root.outputDir : "Çıktı dizini seçin…"
                                color: root.outputDir.length > 0 ? Theme.text : Theme.textFaint
                                font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm; elide: Text.ElideLeft; width: parent.width - Theme.spacingMd }
                        }
                        AppButton { text: "Seç"; variant: "secondary"; onClicked: outDirDlg.open() }
                    }
                }
                Item { Layout.fillHeight: true }
            }

            // ── Step 3: Build & Flash ─────────────────────────────────────
            ColumnLayout {
                spacing: Theme.spacingMd

                RowLayout {
                    Layout.fillWidth: true
                    Text { text: "Pipeline özeti"; color: Theme.text; font.family: Theme.fontFamily; font.pixelSize: Theme.fontMd; font.weight: Font.DemiBold; Layout.fillWidth: true }
                    StatusPill {
                        text: (typeof backend !== "undefined" && backend) ? backend.pipelineStage : "Hazir"
                        status: (typeof backend !== "undefined" && backend && backend.pipelineBusy) ? "warning" : "ready"
                    }
                }

                Repeater {
                    model: [
                        ["Model",          root.modelPath.length > 0 ? root.modelPath.split(/[\\/]/).pop() : "—"],
                        ["Kuantizasyon",   root.quantization],
                        ["Sensör",         root.sensorType],
                        ["Hedef Kart",     root.targetBoard || root.activeBoardName || "STM32F4"],
                        ["Aktif COM",      root.activePortName.length > 0 ? root.activePortName : "Bagli degil"],
                        ["Çıktı Dizini",   root.outputDir.length > 0 ? root.outputDir : "—"]
                    ]
                    delegate: RowLayout {
                        Layout.fillWidth: true
                        Text { text: modelData[0]; color: Theme.textMuted; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm; Layout.preferredWidth: 130 }
                        Text { text: modelData[1]; color: Theme.text; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm; font.weight: Font.DemiBold; elide: Text.ElideMiddle; Layout.fillWidth: true }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true; Layout.preferredHeight: 70
                    radius: Theme.radiusMd; color: Theme.primarySoft; border.color: Theme.primary
                    Text {
                        anchors.centerIn: parent; width: parent.width - Theme.spacingLg
                        text: "Pipeline Başlat düğmesine basınca:\n" +
                              "1) stedgeai C kodu üretir  2) arm-gcc derler  3) STM32_Programmer flash atar"
                        color: Theme.text; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm
                        horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.spacingSm
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 38
                        radius: Theme.radiusMd
                        color: Theme.surfaceRaised
                        border.color: root.outputDir.length > 0 ? Theme.border : Theme.warning
                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.left: parent.left
                            anchors.leftMargin: Theme.spacingSm
                            width: parent.width - Theme.spacingMd
                            text: root.outputDir.length > 0 ? root.outputDir : "Cikti dizini secin"
                            color: root.outputDir.length > 0 ? Theme.text : Theme.warning
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSm
                            elide: Text.ElideLeft
                        }
                    }
                    AppButton { text: "Dizin Sec"; variant: "secondary"; onClicked: outDirDlg.open() }
                }
                Terminal {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    lines: (typeof backend !== "undefined" && backend) ? backend.pipelineLines : []
                }
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 18
                    radius: Theme.radiusSm
                    color: Theme.surfaceRaised
                    border.color: Theme.border
                    Rectangle {
                        height: parent.height
                        width: parent.width * Math.max(0, Math.min(((typeof backend !== "undefined" && backend) ? backend.pipelineProgress : 0) / 100, 1))
                        radius: parent.radius
                        color: Theme.success
                    }
                    Text {
                        anchors.centerIn: parent
                        text: ((typeof backend !== "undefined" && backend) ? backend.pipelineProgress : 0) + "%"
                        color: Theme.text
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontXs
                        font.weight: Font.DemiBold
                    }
                }
                Item { Layout.fillHeight: true }
            }
        }

        Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Theme.border }

        // ── Navigation buttons ────────────────────────────────────────────
        RowLayout {
            Layout.fillWidth: true; Layout.margins: Theme.spacingMd; spacing: Theme.spacingSm

            AppButton { text: "İptal"; variant: "ghost"; onClicked: root.close() }
            Item { Layout.fillWidth: true }

            AppButton {
                visible: root.step > 0
                text: "← Geri"; variant: "secondary"
                onClicked: root.step--
            }
            AppButton {
                text: root.step < 3 ? "İleri →" : "✦ Pipeline Başlat"
                enabled: root.step < 3
                    ? (root.step === 0 ? root.modelPath.length > 0 : true)
                    : !(typeof backend !== "undefined" && backend && backend.pipelineBusy)
                onClicked: {
                    if (root.step < 3) { root.step++; return }
                    if (root.outputDir.length === 0) { outDirDlg.open(); return }
                    root.refreshFromAppState()
                    if (typeof backend !== "undefined" && backend)
                        backend.runPipeline(root.pipelineConfig())
                }
            }
            AppButton {
                visible: typeof backend !== "undefined" && backend && backend.pipelineBusy
                text: "İptal"; variant: "danger"
                onClicked: backend.cancelPipeline()
            }
        }
    }

    // Dialogs
    FileDialog {
        id: modelPickDlg
        title: "Model dosyası seçin"
        nameFilters: ["AI Model (*.tflite *.h5 *.onnx)", "Tüm (*)"]
        onAccepted: {
            root.modelPath = selectedFile.toString().replace("file:///","")
            if (root.modelName.length === 0)
                root.modelName = root.modelPath.split(/[\\/]/).pop().replace(/\.\w+$/,"")
        }
    }

    FolderDialog {
        id: outDirDlg
        title: "Çıktı dizini seçin"
        onAccepted: root.outputDir = selectedFolder.toString().replace("file:///","")
    }

    onOpened: {
        step = 0
        refreshFromAppState()
    }

    component PinField: TextField {
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
            Behavior on border.color { ColorAnimation { duration: Theme.animFast } }
        }
    }
}
