import QtQuick
import QtQuick.Layouts
import STM32AiDeployer

RowLayout {
    id: root

    property string label: ""
    property real value: 0.0
    property string valueText: ""
    property color accent: Theme.primary

    spacing: Theme.spacingSm
    Layout.fillWidth: true

    Text {
        text: root.label
        color: Theme.textMuted
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSm
        elide: Text.ElideRight
        Layout.preferredWidth: 110
    }

    Rectangle {
        Layout.fillWidth: true
        Layout.preferredHeight: 8
        radius: 4
        color: Theme.surfaceRaised

        Rectangle {
            width: parent.width * Math.max(0, Math.min(root.value, 1))
            height: parent.height
            radius: parent.radius
            color: root.accent

            Behavior on width {
                NumberAnimation { duration: Theme.animNormal; easing.type: Easing.OutCubic }
            }
        }
    }

    Text {
        text: root.valueText
        color: Theme.text
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSm
        font.weight: Font.DemiBold
        horizontalAlignment: Text.AlignRight
        Layout.preferredWidth: 80
    }
}
