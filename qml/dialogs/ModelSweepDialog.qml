import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import STM32AiDeployer

// Faz 10.6: pick several .tflite models + one board, run compile→flash→
// watch→save-profile for each in turn (ModelSweepRunner via
// Backend::startModelSweep). Results land in the existing "İzleme
// Profilleri" tab (Faz 10.1's AnalysisScreen sub-tab) — no new results UI
// needed here beyond a live progress list.
Popup {
    id: root
    modal: true
    anchors.centerIn: Overlay.overlay
    width: 620
    height: 560
    padding: Theme.spacingLg
    closePolicy: Popup.CloseOnEscape

    readonly property bool _hasBackend: (typeof backend !== "undefined" && backend)
    readonly property var  _status: root._hasBackend ? backend.sweepStatus : ({})
    readonly property bool _running: root._status.running === true

    property var _modelPaths: []
    readonly property var _boardOptions: ["STM32F4", "STM32H7", "STM32N6"]
    readonly property var _sensorOptions: ["BME280", "MPU6050", "PDM_MIC"]
    property int _boardIndex: 0
    property int _sensorIndex: 0

    function stageLabel(s) {
        switch (s) {
        case "compiling":  return "Derleniyor/Flash…"
        case "connecting": return "ST-Link'e bağlanılıyor…"
        case "watching":   return "Örnekleniyor…"
        case "saving":     return "Profil kaydediliyor…"
        case "done":       return "Bitti"
        default:           return "Bekliyor"
        }
    }

    Overlay.modal: Rectangle { color: Theme.alpha("#000000", 0.55) }
    background: Rectangle { color: Theme.surface; radius: Theme.radiusLg; border.color: Theme.border }

    contentItem: ColumnLayout {
        spacing: Theme.spacingMd

        Text {
            text: "Çoklu Model Süpürme"
            color: Theme.text
            font.family: Theme.fontFamily; font.pixelSize: Theme.fontMd; font.weight: Font.DemiBold
        }
        Text {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: "Seçilen her model için sırayla: derle → flash → izleyiciye bağlan → örnekle → profil kaydet. Bir modelin başarısızlığı süpürmeyi durdurmaz."
            color: Theme.textMuted
            font.family: Theme.fontFamily; font.pixelSize: Theme.fontXs
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spacingSm
            AppButton {
                text: "Model Ekle…"
                variant: "secondary"
                enabled: !root._running
                onClicked: modelDialog.open()
            }
            ComboField {
                label: "Kart"
                options: root._boardOptions
                currentIndex: root._boardIndex
                enabled: !root._running
                onActivated: (i) => root._boardIndex = i
            }
            ComboField {
                label: "Sensör"
                options: root._sensorOptions
                currentIndex: root._sensorIndex
                enabled: !root._running
                onActivated: (i) => root._sensorIndex = i
            }
            SpinBox {
                id: secondsSpin
                from: 5; to: 300; value: 20; stepSize: 5
                enabled: !root._running
            }
            Text { text: "sn/model"; color: Theme.textMuted; font.family: Theme.fontFamily; font.pixelSize: Theme.fontXs }
        }

        Text {
            text: "Modeller (" + root._modelPaths.length + ")"
            color: Theme.text; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm; font.weight: Font.DemiBold
        }
        ListView {
            Layout.fillWidth: true
            Layout.preferredHeight: 90
            clip: true
            model: root._modelPaths
            spacing: 1
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar { width: 8 }
            delegate: Rectangle {
                width: ListView.view.width
                height: 26
                color: "transparent"
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Theme.spacingXs
                    Text {
                        Layout.fillWidth: true
                        text: modelData
                        color: Theme.text; font.family: Theme.monoFamily; font.pixelSize: Theme.fontXs
                        elide: Text.ElideMiddle
                    }
                    AppButton {
                        text: "×"; variant: "ghost"; implicitWidth: 28; implicitHeight: 22
                        enabled: !root._running
                        onClicked: {
                            var arr = root._modelPaths.slice()
                            arr.splice(index, 1)
                            root._modelPaths = arr
                        }
                    }
                }
            }
        }

        Text {
            text: "İlerleme"
            color: Theme.text; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm; font.weight: Font.DemiBold
        }
        Text {
            visible: root._running
            text: root._status.currentModel
                  ? ("[" + (root._status.currentIndex + 1) + "/" + root._status.totalModels + "] "
                     + root._status.currentModel + " — " + root.stageLabel(root._status.stage))
                  : ""
            color: Theme.primary
            font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm
        }
        ListView {
            objectName: "sweep.progressList"
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: root._status.results || []
            spacing: 2
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar { width: 8 }
            delegate: RowLayout {
                width: ListView.view.width
                spacing: Theme.spacingSm
                StatusPill {
                    text: modelData.status === "ok" ? "OK" : "HATA"
                    status: modelData.status === "ok" ? "ready" : "error"
                }
                Text {
                    Layout.fillWidth: true
                    text: modelData.model + (modelData.status === "ok" ? "" : (" — " + modelData.status))
                    color: Theme.text; font.family: Theme.fontFamily; font.pixelSize: Theme.fontXs
                    elide: Text.ElideRight
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Item { Layout.fillWidth: true }
            AppButton {
                objectName: "sweep.cancelButton"
                text: "İptal"
                variant: "danger"
                visible: root._running
                onClicked: if (root._hasBackend) backend.cancelModelSweep()
            }
            AppButton {
                objectName: "sweep.startButton"
                text: root._running ? "Çalışıyor…" : "Başlat"
                variant: "primary"
                enabled: !root._running && root._modelPaths.length > 0
                onClicked: {
                    if (!root._hasBackend) return
                    backend.startModelSweep(root._modelPaths,
                                             root._boardOptions[root._boardIndex],
                                             root._sensorOptions[root._sensorIndex],
                                             secondsSpin.value)
                }
            }
            AppButton { text: "Kapat"; variant: "ghost"; onClicked: root.close() }
        }
    }

    FileDialog {
        id: modelDialog
        title: "Model dosyaları seçin (.tflite)"
        fileMode: FileDialog.OpenFiles
        nameFilters: ["TFLite modelleri (*.tflite)", "Tüm dosyalar (*)"]
        onAccepted: {
            var arr = root._modelPaths.slice()
            for (var i = 0; i < selectedFiles.length; i++) {
                var p = String(selectedFiles[i]).replace("file:///", "")
                if (arr.indexOf(p) < 0) arr.push(p)
            }
            root._modelPaths = arr
        }
    }
}
