import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import STM32AiDeployer

// Recording / playback / export controls (plan docs/variable_watcher_plan.md
// Bolum 10). Kept as its own row below WatchToolbar rather than crammed into
// it — WatchToolbar is already a full single-row action bar.
RowLayout {
    id: root
    spacing: Theme.spacingSm

    readonly property bool _hasBackend: (typeof backend !== "undefined" && backend)
    readonly property var _speedOptions: [0, 0.25, 1, 2, 4]
    property int _speedIndex: 2   // 1x default

    function speedLabel(s) { return s === 0 ? "Duraklat" : (s + "x") }

    AppButton {
        text: (root._hasBackend && backend.watchRecording) ? "⏺ Kaydı Durdur" : "⏺ Kayıt Başlat"
        variant: (root._hasBackend && backend.watchRecording) ? "danger" : "secondary"
        enabled: root._hasBackend && backend.watchRunning && !backend.watchPlayback
        onClicked: {
            if (backend.watchRecording) backend.stopWatchRecording()
            else backend.startWatchRecording("")
        }
    }

    Rectangle { Layout.preferredWidth: 1; Layout.fillHeight: true; color: Theme.border }

    AppButton {
        text: "Demo Kaydını Oynat"
        variant: "secondary"
        enabled: root._hasBackend && !backend.watchRunning
        onClicked: backend.startWatchPlayback(backend.demoTracePath(), root._speedOptions[root._speedIndex])
    }

    RowLayout {
        visible: root._hasBackend && backend.watchPlayback
        spacing: Theme.spacingSm

        ComboField {
            id: speedField
            label: ""
            options: root._speedOptions.map(root.speedLabel)
            currentIndex: root._speedIndex
            Layout.preferredWidth: 110
            onActivated: (idx) => {
                root._speedIndex = idx
                backend.setWatchPlaybackSpeed(root._speedOptions[idx])
            }
        }
        AppButton {
            text: "Adım"
            variant: "ghost"
            enabled: root._speedOptions[root._speedIndex] === 0
            onClicked: backend.stepWatchPlayback()
        }
        AppButton {
            text: "Oynatmayı Durdur"
            variant: "danger"
            onClicked: backend.stopWatchPlayback()
        }
    }

    Item { Layout.fillWidth: true }

    AppButton {
        text: "AI Presetlerini Uygula"
        variant: "ghost"
        enabled: root._hasBackend && !backend.watchRunning
        onClicked: backend.applyWatchPresets()
    }
    AppButton {
        objectName: "watch.compareProfilesButton"
        text: "Profilleri Karşılaştır"
        variant: "ghost"
        enabled: root._hasBackend
        onClicked: compareDialog.open()
    }
    AppButton {
        text: "CSV Dışa Aktar"
        variant: "ghost"
        enabled: root._hasBackend
        onClicked: csvDialog.open()
    }
    AppButton {
        text: "Profili Kaydet"
        variant: "ghost"
        enabled: root._hasBackend && backend.watchItems.length > 0
        onClicked: { noteField.text = ""; profileDialog.open() }
    }

    ProfileCompareDialog { id: compareDialog }

    FileDialog {
        id: csvDialog
        title: "Görünen pencereyi CSV olarak dışa aktar"
        fileMode: FileDialog.SaveFile
        nameFilters: ["CSV dosyası (*.csv)"]
        onAccepted: backend.exportWatchCsv(String(selectedFile).replace("file:///", ""))
    }

    Popup {
        id: profileDialog
        modal: true
        anchors.centerIn: Overlay.overlay
        width: 360
        padding: Theme.spacingLg
        Overlay.modal: Rectangle { color: Theme.alpha("#000000", 0.55) }
        background: Rectangle { color: Theme.surface; radius: Theme.radiusLg; border.color: Theme.border }

        contentItem: ColumnLayout {
            spacing: Theme.spacingMd
            Text {
                text: "İzleme Profili Kaydet"
                color: Theme.text
                font.family: Theme.fontFamily; font.pixelSize: Theme.fontMd; font.weight: Font.DemiBold
            }
            TextField {
                id: noteField
                Layout.fillWidth: true
                placeholderText: "Not (opsiyonel)"
                color: Theme.text; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm
                background: Rectangle { radius: Theme.radiusSm; color: Theme.surfaceRaised; border.color: Theme.border }
                leftPadding: Theme.spacingSm; rightPadding: Theme.spacingSm
            }
            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                AppButton { text: "Vazgeç"; variant: "ghost"; onClicked: profileDialog.close() }
                AppButton {
                    text: "Kaydet"
                    onClicked: {
                        backend.saveWatchProfile(noteField.text)
                        profileDialog.close()
                    }
                }
            }
        }
    }
}
