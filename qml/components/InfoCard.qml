import QtQuick
import QtQuick.Layouts
import QtQuick.Effects
import STM32AiDeployer

Item {
    id: root

    property string title: ""
    property string value: "--"
    property string subtitle: ""
    property string iconText: ""
    property color accent: Theme.primary

    implicitHeight: 128
    Layout.fillWidth: true

    Rectangle {
        id: shadowSource
        anchors.fill: card
        radius: card.radius
        color: card.color
        visible: false
    }

    MultiEffect {
        anchors.fill: shadowSource
        source: shadowSource
        shadowEnabled: true
        shadowBlur: 0.5
        shadowOpacity: 0.22
        shadowVerticalOffset: 6
    }

    Rectangle {
        id: card
        anchors.fill: parent
        radius: Theme.radiusLg
        color: Theme.surface
        border.color: Theme.border

        // accent strip
        Rectangle {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: 3
            radius: 2
            color: root.accent
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: Theme.spacingMd
            anchors.leftMargin: Theme.spacingMd + 4
            spacing: Theme.spacingSm

            RowLayout {
                Layout.fillWidth: true

                Text {
                    text: root.title
                    color: Theme.textMuted
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSm
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }

                Rectangle {
                    Layout.preferredWidth: 32
                    Layout.preferredHeight: 32
                    radius: Theme.radiusMd
                    color: Theme.alpha(root.accent, 0.16)
                    visible: root.iconText.length > 0

                    Text {
                        anchors.centerIn: parent
                        text: root.iconText
                        color: root.accent
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontMd
                        font.weight: Font.Bold
                    }
                }
            }

            Text {
                text: root.value
                color: Theme.text
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontXl
                font.weight: Font.Bold
                elide: Text.ElideRight
                Layout.fillWidth: true
            }

            Text {
                text: root.subtitle
                color: Theme.textFaint
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontXs
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
        }
    }
}
