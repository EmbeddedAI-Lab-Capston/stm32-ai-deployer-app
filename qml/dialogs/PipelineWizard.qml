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

    readonly property var steps: ["Model Seç", "Sensör Ayarla", "Araç Doğrulama", "Derleme & Flash"]
    readonly property var quantOptions: ["INT8", "Dynamic Q", "Float32"]
    readonly property var sensorOptions: ["BME280 (I2C)", "MPU6050 (I2C)", "PDM Mikrofon (SAI)"]
    function currentBoardOptions() {
        if (typeof appState !== "undefined" && appState && appState.boardName.length > 0)
            return [appState.boardName, "STM32F4", "STM32H7", "STM32N6"]
        return ["STM32F4", "STM32H7", "STM32N6"]
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

                Item { Layout.fillHeight: true }
            }

            // ── Step 1: Sensor ────────────────────────────────────────────
            ColumnLayout {
                spacing: Theme.spacingMd

                Text { text: "Kartınıza bağlı sensörü ve hedef kartı seçin."
                    color: Theme.textMuted; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm }

                ComboField { label: "Hedef Kart"; options: root.currentBoardOptions()
                    Component.onCompleted: {
                        if (typeof appState !== "undefined" && appState && appState.boardName.length > 0)
                            root.targetBoard = appState.boardName
                        else root.targetBoard = "STM32F4"
                    }
                    onActivated: (i) => root.targetBoard = root.currentBoardOptions()[i]
                }

                ComboField { label: "Sensör Tipi"; options: root.sensorOptions
                    onActivated: (i) => root.sensorType = root.sensorOptions[i].split(" ")[0]
                }

                // Pin config (simplified for demo)
                Card {
                    title: "Varsayılan Pin Konfigürasyonu"
                    Layout.fillWidth: true; Layout.preferredHeight: 140
                    ColumnLayout {
                        anchors.fill: parent; spacing: Theme.spacingXs
                        Repeater {
                            model: root.sensorType === "PDM" ?
                                [["SAI İnstans", "SAI1"], ["CLK Port/Pin", "GPIOB / GPIO_PIN_5"], ["DATA Port/Pin", "GPIOB / GPIO_PIN_3"]] :
                                [["I2C İnstans", "I2C1"], ["SDA Port/Pin", "GPIOB / GPIO_PIN_7"], ["SCL Port/Pin", "GPIOB / GPIO_PIN_6"], ["I2C Adres", "0x76"]]
                            delegate: RowLayout {
                                Layout.fillWidth: true
                                Text { text: modelData[0]; color: Theme.textMuted; font.family: Theme.fontFamily; font.pixelSize: Theme.fontXs; Layout.preferredWidth: 120 }
                                Text { text: modelData[1]; color: Theme.text; font.family: Theme.fontFamily; font.pixelSize: Theme.fontXs; font.weight: Font.DemiBold }
                            }
                        }
                    }
                }
                Item { Layout.fillHeight: true }
            }

            // ── Step 2: Tool validation ───────────────────────────────────
            ColumnLayout {
                spacing: Theme.spacingMd

                Item { Layout.fillHeight: true }

                Text { text: "Araç yollarını ve çıktı dizinini doğrulayın."
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    color: Theme.textMuted; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm }

                Repeater {
                    model: typeof backend !== "undefined" && backend ? backend.toolPaths : []
                    delegate: Rectangle {
                        Layout.fillWidth: true; Layout.preferredHeight: 48
                        radius: Theme.radiusMd; color: Theme.surfaceRaised; border.color: Theme.border
                        RowLayout {
                            anchors.fill: parent; anchors.margins: Theme.spacingMd; spacing: Theme.spacingMd
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
                    StatusPill { text: "Hazır"; status: "ready" }
                }

                Repeater {
                    model: [
                        ["Model",          root.modelPath.length > 0 ? root.modelPath.split(/[\\/]/).pop() : "—"],
                        ["Kuantizasyon",   root.quantization],
                        ["Sensör",         root.sensorType],
                        ["Hedef Kart",     root.targetBoard || "STM32F4"],
                        ["Çıktı Dizini",   root.outputDir.length > 0 ? root.outputDir : "—"]
                    ]
                    delegate: RowLayout {
                        Layout.fillWidth: true
                        Text { text: modelData[0]; color: Theme.textMuted; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm; Layout.preferredWidth: 130 }
                        Text { text: modelData[1]; color: Theme.text; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm; font.weight: Font.DemiBold; elide: Text.ElideMiddle; Layout.fillWidth: true }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true; Layout.preferredHeight: 80
                    radius: Theme.radiusMd; color: Theme.primarySoft; border.color: Theme.primary
                    Text {
                        anchors.centerIn: parent; width: parent.width - Theme.spacingLg
                        text: "Pipeline Başlat düğmesine basınca:\n" +
                              "1) stedgeai C kodu üretir  2) arm-gcc derler  3) STM32_Programmer flash atar"
                        color: Theme.text; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm
                        horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap
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
                    : true
                onClicked: {
                    if (root.step < 3) { root.step++; return }
                    // Start pipeline — for now show a message (PipelineRunner integration in next pass)
                    startedNote.open()
                }
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

    Popup {
        id: startedNote
        modal: true; anchors.centerIn: Overlay.overlay; width: 380; padding: Theme.spacingLg
        background: Rectangle { color: Theme.surface; radius: Theme.radiusLg; border.color: Theme.border }
        contentItem: ColumnLayout {
            spacing: Theme.spacingMd
            Text { text: "Pipeline başlatıldı!"; color: Theme.success; font.family: Theme.fontFamily; font.pixelSize: Theme.fontMd; font.weight: Font.DemiBold }
            Text { Layout.fillWidth: true
                text: "stedgeai → arm-gcc → STM32_Programmer zinciri arka planda çalışıyor.\nFlash ekranındaki terminali takip edin."
                color: Theme.textMuted; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm; wrapMode: Text.WordWrap }
            AppButton { Layout.alignment: Qt.AlignRight; text: "Tamam"
                onClicked: { startedNote.close(); root.close() } }
        }
    }

    onOpened: step = 0
}
