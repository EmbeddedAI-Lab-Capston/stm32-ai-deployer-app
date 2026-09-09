import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import STM32AiDeployer

// Model 1 vs Model 2 saved-session comparison (plan
// docs/variable_watcher_plan.md Bolum 11.6). Naming note (CLAUDE.md): this
// is NOT the Register Inspector's "Snapshot A -> Snapshot B" — different
// concept, so the UI here says "Oturum 1/2" / "Model 1/2", never "A/B".
Popup {
    id: root
    modal: true
    anchors.centerIn: Overlay.overlay
    width: 560
    padding: Theme.spacingLg
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    readonly property bool _hasBackend: (typeof backend !== "undefined" && backend)
    property var _profiles: []
    property int _idxA: 0
    property int _idxB: 1
    property var _result: ({})

    function refreshProfiles() { _profiles = root._hasBackend ? backend.watchProfiles() : [] }
    function labelFor(p) { return (p.model || "?") + " / " + (p.board || "?") + "  (" + (p.itemCount || 0) + " kalem)" }

    function recompute() {
        if (!root._hasBackend || _profiles.length < 2) { _result = {}; return }
        const idA = _profiles[Math.min(_idxA, _profiles.length - 1)].id
        const idB = _profiles[Math.min(_idxB, _profiles.length - 1)].id
        _result = backend.compareWatchProfiles(idA, idB)
    }

    onOpened: { refreshProfiles(); recompute() }

    Overlay.modal: Rectangle { color: Theme.alpha("#000000", 0.55) }
    background: Rectangle { color: Theme.surface; radius: Theme.radiusLg; border.color: Theme.border }

    contentItem: ColumnLayout {
        spacing: Theme.spacingMd

        Text {
            text: "Profil Karşılaştırma"
            color: Theme.text
            font.family: Theme.fontFamily; font.pixelSize: Theme.fontMd; font.weight: Font.DemiBold
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spacingSm
            ComboField {
                label: "Oturum 1"
                options: root._profiles.map(root.labelFor)
                currentIndex: root._idxA
                Layout.fillWidth: true
                onActivated: (idx) => { root._idxA = idx; root.recompute() }
            }
            ComboField {
                label: "Oturum 2"
                options: root._profiles.map(root.labelFor)
                currentIndex: root._idxB
                Layout.fillWidth: true
                onActivated: (idx) => { root._idxB = idx; root.recompute() }
            }
        }

        Text {
            visible: root._profiles.length < 2
            text: "Karşılaştırmak için en az 2 kaydedilmiş izleme profili gerekiyor (Kayıt Başlat → Profili Kaydet)."
            color: Theme.textFaint
            font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm
            Layout.preferredWidth: 480
            wrapMode: Text.WordWrap
        }

        ColumnLayout {
            visible: root._profiles.length >= 2 && root._result.rows !== undefined
            Layout.fillWidth: true
            spacing: 2

            RowLayout {
                Layout.fillWidth: true
                Text { text: "Metrik"; Layout.preferredWidth: 190; color: Theme.primary; font.pixelSize: Theme.fontXs; font.weight: Font.Bold; font.family: Theme.fontFamily }
                Text { text: "Oturum 1"; Layout.preferredWidth: 85; color: Theme.primary; font.pixelSize: Theme.fontXs; font.weight: Font.Bold; font.family: Theme.fontFamily }
                Text { text: "Oturum 2"; Layout.preferredWidth: 85; color: Theme.primary; font.pixelSize: Theme.fontXs; font.weight: Font.Bold; font.family: Theme.fontFamily }
                Text { text: "Δ"; Layout.preferredWidth: 75; color: Theme.primary; font.pixelSize: Theme.fontXs; font.weight: Font.Bold; font.family: Theme.fontFamily }
                Text { text: "Δ%"; Layout.fillWidth: true; color: Theme.primary; font.pixelSize: Theme.fontXs; font.weight: Font.Bold; font.family: Theme.fontFamily }
            }
            Repeater {
                model: root._result.rows || []
                delegate: RowLayout {
                    Layout.fillWidth: true
                    Text {
                        text: modelData.metric; Layout.preferredWidth: 190
                        color: Theme.text; font.pixelSize: Theme.fontXs; font.family: Theme.fontFamily; elide: Text.ElideRight
                    }
                    Text {
                        text: modelData.hasMatch ? Number(modelData.a).toFixed(2) : "—"
                        Layout.preferredWidth: 85; color: Theme.textMuted; font.pixelSize: Theme.fontXs; font.family: Theme.monoFamily
                    }
                    Text {
                        text: modelData.hasMatch ? Number(modelData.b).toFixed(2) : "—"
                        Layout.preferredWidth: 85; color: Theme.textMuted; font.pixelSize: Theme.fontXs; font.family: Theme.monoFamily
                    }
                    Text {
                        text: modelData.hasMatch ? Number(modelData.delta).toFixed(2) : "karşılığı yok"
                        Layout.preferredWidth: 75
                        color: !modelData.hasMatch ? Theme.textFaint : (modelData.delta > 0 ? Theme.danger : Theme.success)
                        font.pixelSize: Theme.fontXs; font.family: Theme.monoFamily
                    }
                    Text {
                        text: (modelData.hasMatch && modelData.deltaPct !== undefined) ? (Number(modelData.deltaPct).toFixed(1) + "%") : "—"
                        Layout.fillWidth: true; color: Theme.textMuted; font.pixelSize: Theme.fontXs; font.family: Theme.monoFamily
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Item { Layout.fillWidth: true }
            AppButton { objectName: "watch.compareDialogCloseButton"; text: "Kapat"; variant: "ghost"; onClicked: root.close() }
        }
    }
}
