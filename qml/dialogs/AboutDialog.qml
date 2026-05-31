import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import STM32AiDeployer

Popup {
    id: root
    modal: true
    anchors.centerIn: Overlay.overlay
    width: 460
    padding: 0
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    Overlay.modal: Rectangle { color: Theme.alpha("#000000", 0.55) }

    background: Rectangle {
        color: Theme.surface
        radius: Theme.radiusLg
        border.color: Theme.border
    }

    contentItem: ColumnLayout {
        spacing: Theme.spacingMd

        Item { Layout.fillWidth: true; Layout.preferredHeight: Theme.spacingLg }

        // logo
        Rectangle {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: 64; Layout.preferredHeight: 64
            radius: Theme.radiusLg
            gradient: Gradient {
                GradientStop { position: 0.0; color: Theme.primary }
                GradientStop { position: 1.0; color: Theme.cyan }
            }
            Text { anchors.centerIn: parent; text: "AI"; color: "#FFFFFF"
                   font.family: Theme.fontFamily; font.pixelSize: Theme.fontLg; font.weight: Font.Bold }
        }

        Text {
            Layout.alignment: Qt.AlignHCenter
            text: "STM32 AI Deployer"
            color: Theme.text
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontLg
            font.weight: Font.Bold
        }

        Text {
            Layout.alignment: Qt.AlignHCenter
            text: "v1.0.0"
            color: Theme.textMuted
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSm
        }

        Rectangle { Layout.fillWidth: true; Layout.leftMargin: Theme.spacingLg
                    Layout.rightMargin: Theme.spacingLg; Layout.preferredHeight: 1; color: Theme.border }

        Text {
            Layout.alignment: Qt.AlignHCenter
            horizontalAlignment: Text.AlignHCenter
            text: "STM32 serisi kartlara AI modeli yükleme ve\ngerçek zamanlı metrik izleme uygulaması."
            color: Theme.textMuted
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSm
        }

        Text {
            Layout.alignment: Qt.AlignHCenter
            horizontalAlignment: Text.AlignHCenter
            text: "Marmara Üniversitesi — Bilgisayar Mühendisliği\nBitirme Projesi 2025-2026\n\nMuhammet Ali Şeker · Furkan Talha Kasım · Kadir Mert Abatay"
            color: Theme.textFaint
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontXs
            lineHeight: 1.3
        }

        AppButton {
            Layout.alignment: Qt.AlignHCenter
            Layout.bottomMargin: Theme.spacingLg
            text: "Kapat"
            onClicked: root.close()
        }
    }
}
