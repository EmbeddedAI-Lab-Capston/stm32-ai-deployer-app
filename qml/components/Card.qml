import QtQuick
import QtQuick.Layouts
import QtQuick.Effects
import STM32AiDeployer

Item {
    id: root

    property string title: ""
    property string subtitle: ""
    property int padding: Theme.spacingMd
    default property alias content: body.data

    implicitHeight: 160

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
        shadowColor: "#000000"
    }

    Rectangle {
        id: card
        anchors.fill: parent
        radius: Theme.radiusLg
        color: Theme.surface
        border.color: Theme.border

        ColumnLayout {
            id: header
            visible: root.title.length > 0
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.leftMargin: root.padding
            anchors.rightMargin: root.padding
            anchors.topMargin: root.padding
            spacing: 2

            Text {
                text: root.title
                color: Theme.text
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontMd
                font.weight: Font.DemiBold
                elide: Text.ElideRight
                Layout.fillWidth: true
            }

            Text {
                visible: root.subtitle.length > 0
                text: root.subtitle
                color: Theme.textMuted
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontXs
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
        }

        Item {
            id: body
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.top: header.visible ? header.bottom : parent.top
            anchors.margins: root.padding
            anchors.topMargin: header.visible ? Theme.spacingSm : root.padding
        }
    }
}
