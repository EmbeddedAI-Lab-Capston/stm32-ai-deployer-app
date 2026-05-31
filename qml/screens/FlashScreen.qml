import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import STM32AiDeployer

Item {
    id: root

    property string _fwPath: ""
    property string _fwInfo: ""
    readonly property var _arch: ["1D CNN", "MLP", "LSTM", "TC-ResNet"]
    readonly property var _quant: ["INT8", "Dynamic Q", "Float32"]
    readonly property var _lines: (typeof backend !== "undefined" && backend && backend.flashLines.length > 0)
                                  ? backend.flashLines : MockData.flashTerminal
    readonly property int  _progress: (typeof backend !== "undefined" && backend) ? backend.flashProgress : 100
    readonly property bool _busy: (typeof backend !== "undefined" && backend) ? backend.flashBusy : false
    readonly property bool _connected: (typeof appState !== "undefined" && appState) ? appState.connected : true

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
                onClicked: pipelineNote.open()
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
                            color: Theme.primarySoft
                            border.color: Theme.primary
                            border.width: 2
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
                            color: Theme.surfaceRaised
                            border.color: Theme.border
                            Text {
                                anchors.centerIn: parent
                                text: "AI Modelinden Üret\n.tflite / .h5"
                                horizontalAlignment: Text.AlignHCenter
                                color: Theme.textMuted
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSm
                            }
                            MouseArea { anchors.fill: parent; onClicked: pipelineNote.open() }
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
                            FormField { id: archField; label: "Mimari"; options: root._arch }
                            FormField { id: quantField; label: "Kuantizasyon"; options: root._quant }
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
                            onClicked: if (typeof backend !== "undefined" && backend) backend.clearFlashLog()
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

    // ── Pipeline wizard placeholder (full wizard wired in a later pass) ──────
    Popup {
        id: pipelineNote
        modal: true
        anchors.centerIn: Overlay.overlay
        width: 420
        padding: Theme.spacingLg
        background: Rectangle { color: Theme.surface; radius: Theme.radiusLg; border.color: Theme.border }
        contentItem: ColumnLayout {
            spacing: Theme.spacingMd
            Text {
                text: "Pipeline Sihirbazı"
                color: Theme.text
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontMd
                font.weight: Font.DemiBold
            }
            Text {
                Layout.fillWidth: true
                text: ".tflite → C kodu → derleme → flash akışı. Bu sihirbaz bir sonraki adımda tam bağlanacaktır."
                color: Theme.textMuted
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSm
                wrapMode: Text.WordWrap
            }
            AppButton { Layout.alignment: Qt.AlignRight; text: "Tamam"; onClicked: pipelineNote.close() }
        }
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
}
