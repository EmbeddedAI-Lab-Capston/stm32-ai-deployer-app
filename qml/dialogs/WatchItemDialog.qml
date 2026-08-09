import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import STM32AiDeployer

// Manual address / type / scale entry (plan docs/variable_watcher_plan.md
// Bolum 7.1). Address validation against the loaded ELF's symbol range (if
// any) happens on the C++ side (VariableWatcher::addAddress) — a warning,
// never a block.
//
// NOTE: qml/components/FormField.qml is display-only (acceptedButtons:
// Qt.NoButton, no real text input), so the labelled fields below are
// inlined directly — same pattern SettingsDialog.qml uses for its LLM
// fields, rather than a shared component.
Popup {
    id: root
    modal: true
    anchors.centerIn: Overlay.overlay
    width: 420
    padding: Theme.spacingLg
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    readonly property bool _hasBackend: (typeof backend !== "undefined" && backend)
    readonly property var _types: ["u8", "i8", "u16", "i16", "u32", "i32", "u64", "i64", "f32", "f64"]
    readonly property var _formats: ["dec", "hex", "bin"]

    onOpened: {
        labelField.text = ""
        addrField.text = ""
        scaleField.text = "1.0"
        offsetField.text = "0.0"
        unitField.text = ""
        typeField.currentIndex = 4   // u32
        formatField.currentIndex = 0 // dec
    }

    Overlay.modal: Rectangle { color: Theme.alpha("#000000", 0.55) }
    background: Rectangle { color: Theme.surface; radius: Theme.radiusLg; border.color: Theme.border }

    contentItem: ColumnLayout {
        spacing: Theme.spacingMd

        Text {
            text: "Manuel Adres Ekle"
            color: Theme.text
            font.family: Theme.fontFamily; font.pixelSize: Theme.fontMd; font.weight: Font.DemiBold
        }

        ColumnLayout {
            spacing: 2
            Layout.fillWidth: true
            Text { text: "Etiket"; color: Theme.textMuted; font.family: Theme.fontFamily; font.pixelSize: Theme.fontXs }
            TextField {
                id: labelField
                Layout.fillWidth: true
                placeholderText: "ör. myCounter"
                color: Theme.text; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm
                background: Rectangle { radius: Theme.radiusSm; color: Theme.surfaceRaised; border.color: Theme.border }
                leftPadding: Theme.spacingSm; rightPadding: Theme.spacingSm
            }
        }
        ColumnLayout {
            spacing: 2
            Layout.fillWidth: true
            Text { text: "Adres (hex)"; color: Theme.textMuted; font.family: Theme.fontFamily; font.pixelSize: Theme.fontXs }
            TextField {
                id: addrField
                Layout.fillWidth: true
                placeholderText: "0x24000123"
                color: Theme.text; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm
                background: Rectangle { radius: Theme.radiusSm; color: Theme.surfaceRaised; border.color: Theme.border }
                leftPadding: Theme.spacingSm; rightPadding: Theme.spacingSm
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spacingSm
            ComboField { id: typeField; label: "Tip"; options: root._types; Layout.fillWidth: true }
            ComboField { id: formatField; label: "Biçim"; options: root._formats; Layout.fillWidth: true }
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spacingSm
            ColumnLayout {
                spacing: 2
                Layout.fillWidth: true
                Text { text: "Ölçek"; color: Theme.textMuted; font.family: Theme.fontFamily; font.pixelSize: Theme.fontXs }
                TextField {
                    id: scaleField
                    Layout.fillWidth: true
                    placeholderText: "1.0"
                    color: Theme.text; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm
                    background: Rectangle { radius: Theme.radiusSm; color: Theme.surfaceRaised; border.color: Theme.border }
                    leftPadding: Theme.spacingSm; rightPadding: Theme.spacingSm
                }
            }
            ColumnLayout {
                spacing: 2
                Layout.fillWidth: true
                Text { text: "Ofset"; color: Theme.textMuted; font.family: Theme.fontFamily; font.pixelSize: Theme.fontXs }
                TextField {
                    id: offsetField
                    Layout.fillWidth: true
                    placeholderText: "0.0"
                    color: Theme.text; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm
                    background: Rectangle { radius: Theme.radiusSm; color: Theme.surfaceRaised; border.color: Theme.border }
                    leftPadding: Theme.spacingSm; rightPadding: Theme.spacingSm
                }
            }
            ColumnLayout {
                spacing: 2
                Layout.fillWidth: true
                Text { text: "Birim"; color: Theme.textMuted; font.family: Theme.fontFamily; font.pixelSize: Theme.fontXs }
                TextField {
                    id: unitField
                    Layout.fillWidth: true
                    placeholderText: "ms"
                    color: Theme.text; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm
                    background: Rectangle { radius: Theme.radiusSm; color: Theme.surfaceRaised; border.color: Theme.border }
                    leftPadding: Theme.spacingSm; rightPadding: Theme.spacingSm
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Item { Layout.fillWidth: true }
            AppButton { text: "Vazgeç"; variant: "ghost"; onClicked: root.close() }
            AppButton {
                text: "Ekle"
                enabled: addrField.text.trim().length > 0
                onClicked: {
                    if (!root._hasBackend) return
                    backend.addWatchAddress(addrField.text.trim(), root._types[typeField.currentIndex], labelField.text.trim())
                    var items = backend.watchItems
                    if (items.length > 0) {
                        var last = items[items.length - 1]
                        backend.updateWatchItem(last.id, {
                            format: root._formats[formatField.currentIndex],
                            scale: parseFloat(scaleField.text) || 1.0,
                            offset: parseFloat(offsetField.text) || 0.0,
                            unit: unitField.text
                        })
                    }
                    root.close()
                }
            }
        }
    }
}
