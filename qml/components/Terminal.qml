import QtQuick
import QtQuick.Controls
import STM32AiDeployer

Rectangle {
    id: root

    // model: list of { text, type } where type ∈ info|ok|warn|err|cmd
    property var lines: []

    radius: Theme.radiusMd
    color: "#0A0E14"
    border.color: Theme.border
    clip: true

    function lineColor(type) {
        switch (type) {
        case "ok":   return Theme.success
        case "warn": return Theme.warning
        case "err":  return Theme.danger
        case "cmd":  return Theme.primary
        default:     return Theme.textMuted
        }
    }

    ListView {
        id: list
        anchors.fill: parent
        anchors.margins: Theme.spacingSm
        model: root.lines
        clip: true
        spacing: 1
        boundsBehavior: Flickable.StopAtBounds

        ScrollBar.vertical: ScrollBar { width: 8 }

        delegate: Text {
            width: list.width
            text: modelData.text
            color: root.lineColor(modelData.type)
            font.family: Theme.monoFamily
            font.pixelSize: Theme.fontXs
            wrapMode: Text.WrapAnywhere
        }
    }
}
