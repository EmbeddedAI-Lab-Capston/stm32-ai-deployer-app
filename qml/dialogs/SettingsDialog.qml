import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import STM32AiDeployer

Popup {
    id: root
    modal: true
    anchors.centerIn: Overlay.overlay
    width: 620
    padding: 0
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    Overlay.modal: Rectangle { color: Theme.alpha("#000000", 0.55) }

    background: Rectangle {
        color: Theme.surface
        radius: Theme.radiusLg
        border.color: Theme.border
    }

    contentItem: ColumnLayout {
        spacing: 0

        // ── Header ────────────────────────────────────────────────────────
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 56
            color: Theme.bgElevated
            radius: Theme.radiusLg

            // square off bottom corners
            Rectangle {
                anchors.bottom: parent.bottom; width: parent.width
                height: parent.radius; color: parent.color
            }
            Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.border }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Theme.spacingLg
                anchors.rightMargin: Theme.spacingSm

                ColumnLayout {
                    spacing: 0
                    Layout.fillWidth: true
                    Text {
                        text: "Ayarlar"
                        color: Theme.text
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontMd
                        font.weight: Font.DemiBold
                    }
                    Text {
                        text: "Araç yolları (STM32Cube)"
                        color: Theme.textMuted
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontXs
                    }
                }

                Rectangle {
                    Layout.preferredWidth: 32; Layout.preferredHeight: 32
                    radius: Theme.radiusSm
                    color: closeMouse.containsMouse ? Theme.danger : "transparent"
                    Text { anchors.centerIn: parent; text: "✕"
                           color: closeMouse.containsMouse ? "#FFFFFF" : Theme.textMuted; font.pixelSize: Theme.fontSm }
                    MouseArea { id: closeMouse; anchors.fill: parent; hoverEnabled: true; onClicked: root.close() }
                }
            }
        }

        // ── Tool path rows ────────────────────────────────────────────────
        ColumnLayout {
            Layout.fillWidth: true
            Layout.margins: Theme.spacingLg
            spacing: Theme.spacingMd

            Repeater {
                model: (typeof backend !== "undefined" && backend) ? backend.toolPaths : MockData.toolPaths
                delegate: Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 72
                    radius: Theme.radiusMd
                    color: Theme.surfaceRaised
                    border.color: Theme.border

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: Theme.spacingMd
                        anchors.rightMargin: Theme.spacingMd
                        spacing: Theme.spacingMd

                        // status dot
                        Rectangle {
                            Layout.preferredWidth: 10; Layout.preferredHeight: 10; radius: 5
                            color: modelData.found ? Theme.success : Theme.danger
                        }

                        ColumnLayout {
                            spacing: 3
                            Layout.fillWidth: true
                            Text {
                                text: modelData.name
                                color: Theme.text
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSm
                                font.weight: Font.DemiBold
                            }
                            Text {
                                text: modelData.found ? modelData.path : "Bulunamadı — yolu seçin"
                                color: modelData.found ? Theme.textMuted : Theme.danger
                                font.family: Theme.monoFamily
                                font.pixelSize: Theme.fontXs
                                elide: Text.ElideMiddle
                                Layout.fillWidth: true
                            }
                        }

                        AppButton {
                            text: modelData.found ? "Değiştir" : "Seç"
                            variant: "secondary"
                            onClicked: {
                                if (typeof backend === "undefined" || !backend)
                                    return
                                fileDialog.targetKey = modelData.key
                                fileDialog.open()
                            }
                        }
                    }
                }
            }
        }

        // ── Footer ────────────────────────────────────────────────────────
        Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Theme.border }

        RowLayout {
            Layout.fillWidth: true
            Layout.margins: Theme.spacingMd
            spacing: Theme.spacingSm

            AppButton {
                text: (typeof backend !== "undefined" && backend && backend.scanning) ? "Taranıyor…" : "Tümünü Tara"
                iconText: "↻"
                variant: "secondary"
                enabled: !(typeof backend !== "undefined" && backend && backend.scanning)
                onClicked: if (typeof backend !== "undefined" && backend) backend.scanTools()
            }
            Item { Layout.fillWidth: true }
            AppButton { text: "Kapat"; variant: "ghost"; onClicked: root.close() }
            AppButton { text: "Kaydet"; onClicked: root.close() }
        }
    }

    // Native file picker for tool path selection
    FileDialog {
        id: fileDialog
        property string targetKey: ""
        title: "Araç yürütülebilir dosyasını seçin"
        nameFilters: ["Yürütülebilir (*.exe)", "Tüm dosyalar (*)"]
        onAccepted: {
            if (typeof backend !== "undefined" && backend && targetKey.length > 0) {
                var p = selectedFile.toString().replace("file:///", "")
                backend.setToolPath(targetKey, p)
            }
        }
    }
}
