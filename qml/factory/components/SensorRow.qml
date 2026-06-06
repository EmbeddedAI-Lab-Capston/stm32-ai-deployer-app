import QtQuick
import QtQuick.Layouts
import STM32AiDeployer

// One live sensor line: name, normal band bar with the current value marker,
// the value/unit readout and a status dot.
Rectangle {
    id: root

    property var sensor: ({})   // { name, unit, value, valueText, normalMin, normalMax, status, fraction }

    readonly property string status: sensor.status !== undefined ? sensor.status : "ok"
    readonly property color statusColor: Theme.statusColor(status)

    Layout.fillWidth: true
    implicitHeight: 52
    radius: Theme.radiusSm
    color: status === "ok" ? "transparent" : Theme.alpha(statusColor, 0.07)

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.spacingSm
        anchors.rightMargin: Theme.spacingSm
        spacing: Theme.spacingMd

        Rectangle {
            Layout.preferredWidth: 8; Layout.preferredHeight: 8; radius: 4
            color: root.statusColor
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 4

            RowLayout {
                Layout.fillWidth: true
                Text {
                    text: root.sensor.name !== undefined ? root.sensor.name : "—"
                    color: Theme.text
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSm
                    font.weight: Font.DemiBold
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                }
                Text {
                    text: "norm " + (root.sensor.normalMin !== undefined ? root.sensor.normalMin : 0)
                          + "–" + (root.sensor.normalMax !== undefined ? root.sensor.normalMax : 0)
                    color: Theme.textFaint
                    font.family: Theme.monoFamily
                    font.pixelSize: 10
                }
            }

            // band bar
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 6
                radius: 3
                color: Theme.surfaceRaised

                // normal band region (visual reference)
                Rectangle {
                    x: parent.width * 0.0
                    width: parent.width * 0.77
                    height: parent.height
                    radius: parent.radius
                    color: Theme.alpha(Theme.success, 0.18)
                }

                // current value fill
                Rectangle {
                    height: parent.height
                    radius: parent.radius
                    width: parent.width * Math.max(0.02, Math.min((root.sensor.fraction !== undefined ? root.sensor.fraction : 0) / 1.3, 1))
                    color: root.statusColor
                    Behavior on width { NumberAnimation { duration: Theme.animNormal; easing.type: Easing.OutCubic } }
                }
            }
        }

        Text {
            text: (root.sensor.valueText !== undefined ? root.sensor.valueText : "—")
                  + " " + (root.sensor.unit !== undefined ? root.sensor.unit : "")
            color: root.status === "ok" ? Theme.text : root.statusColor
            font.family: Theme.monoFamily
            font.pixelSize: Theme.fontSm
            font.weight: Font.DemiBold
            horizontalAlignment: Text.AlignRight
            Layout.preferredWidth: 110
        }
    }
}
