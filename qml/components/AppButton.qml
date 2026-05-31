import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import STM32AiDeployer

Button {
    id: control

    property string iconText: ""
    property string variant: "primary"   // primary | secondary | danger | ghost

    implicitHeight: 38
    implicitWidth: Math.max(96, contentRow.implicitWidth + Theme.spacingLg)
    padding: Theme.spacingSm
    hoverEnabled: true

    readonly property color _fg: {
        if (variant === "secondary") return Theme.text
        if (variant === "ghost") return Theme.textMuted
        return "#FFFFFF"
    }
    readonly property color _base: {
        if (variant === "danger")    return Theme.danger
        if (variant === "secondary") return Theme.surfaceRaised
        if (variant === "ghost")     return "transparent"
        return Theme.primary
    }

    contentItem: RowLayout {
        id: contentRow
        spacing: Theme.spacingXs

        Text {
            visible: control.iconText.length > 0
            text: control.iconText
            color: control._fg
            font.pixelSize: Theme.fontMd
            font.family: Theme.fontFamily
            font.weight: Font.Bold
            Layout.alignment: Qt.AlignVCenter
        }

        Text {
            text: control.text
            color: control._fg
            font.pixelSize: Theme.fontSm
            font.family: Theme.fontFamily
            font.weight: Font.DemiBold
            elide: Text.ElideRight
            Layout.alignment: Qt.AlignVCenter
        }
    }

    background: Rectangle {
        radius: Theme.radiusMd
        color: {
            if (control.variant === "ghost")
                return control.hovered ? Theme.surfaceRaised : "transparent"
            if (control.variant === "secondary")
                return control.hovered ? Theme.surfaceHover : Theme.surfaceRaised
            var b = control._base
            if (control.down) return Qt.darker(b, 1.18)
            return control.hovered ? Qt.lighter(b, 1.12) : b
        }
        border.color: control.variant === "secondary" || control.variant === "ghost"
                      ? Theme.border : "transparent"
        opacity: control.enabled ? 1.0 : 0.45

        Behavior on color { ColorAnimation { duration: Theme.animFast } }
    }
}
