import QtQuick
import STM32AiDeployer

// Chevron marking a collapsible section: points right when collapsed, down
// when expanded. Purely visual — the section's header row owns the click,
// so the whole line is a hit target rather than this small glyph.
Item {
    id: root

    property bool collapsed: false

    implicitWidth: 12
    implicitHeight: 12

    Text {
        anchors.centerIn: parent
        text: "›"
        color: Theme.textMuted
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSm
        font.weight: Font.DemiBold
        rotation: root.collapsed ? 0 : 90
        Behavior on rotation { NumberAnimation { duration: 120 } }
    }
}
