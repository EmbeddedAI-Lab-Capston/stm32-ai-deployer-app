import QtQuick
import QtQuick.Layouts
import STM32AiDeployer

ColumnLayout {
    id: root

    property string title: ""
    property string subtitle: ""

    spacing: 2
    Layout.fillWidth: true

    Text {
        text: root.title
        color: Theme.text
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontLg
        font.weight: Font.Bold
        elide: Text.ElideRight
        Layout.fillWidth: true
    }

    Text {
        visible: root.subtitle.length > 0
        text: root.subtitle
        color: Theme.textMuted
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSm
        elide: Text.ElideRight
        Layout.fillWidth: true
    }
}
