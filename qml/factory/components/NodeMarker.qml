import QtQuick
import QtQuick.Layouts
import STM32AiDeployer

// A single STM32 node chip used on the factory map. Pulses while in a
// warning/critical state and reports clicks for drill-down.
Rectangle {
    id: root

    property var node: ({})
    signal clicked(int nodeId)

    readonly property string status: node.status !== undefined ? node.status : "ok"
    readonly property color statusColor: Theme.statusColor(status)
    readonly property bool alert: status !== "ok"

    implicitWidth: 132
    implicitHeight: 56
    radius: Theme.radiusMd
    color: hover.hovered ? Theme.surfaceHover : Theme.surfaceRaised
    border.color: alert ? Theme.alpha(statusColor, 0.85) : Theme.border
    border.width: alert ? 1.5 : 1

    Behavior on color { ColorAnimation { duration: Theme.animFast } }

    // Soft glow ring while alarming.
    Rectangle {
        anchors.centerIn: statusDot
        width: statusDot.width
        height: statusDot.height
        radius: width / 2
        color: "transparent"
        border.color: root.statusColor
        border.width: 2
        visible: root.alert
        opacity: 0
        SequentialAnimation on opacity {
            running: root.alert
            loops: Animation.Infinite
            NumberAnimation { from: 0.7; to: 0; duration: 1100; easing.type: Easing.OutCubic }
        }
        scale: root.alert ? 2.4 : 1
        Behavior on scale { NumberAnimation { duration: 1100 } }
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.spacingSm
        anchors.rightMargin: Theme.spacingSm
        spacing: Theme.spacingXs

        Rectangle {
            id: statusDot
            Layout.preferredWidth: 10
            Layout.preferredHeight: 10
            radius: 5
            color: root.statusColor
        }

        ColumnLayout {
            spacing: 0
            Layout.fillWidth: true

            Text {
                text: root.node.name !== undefined ? root.node.name : "—"
                color: Theme.text
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontXs
                font.weight: Font.DemiBold
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
            Text {
                text: (root.node.board !== undefined ? root.node.board : "")
                      + (root.node.alarmCount > 0 ? ("  •  " + root.node.alarmCount + " uyarı") : "")
                color: root.alert ? root.statusColor : Theme.textFaint
                font.family: Theme.fontFamily
                font.pixelSize: 10
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
        }
    }

    HoverHandler { id: hover }
    TapHandler { onTapped: root.clicked(root.node.id) }
}
