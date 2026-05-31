import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import STM32AiDeployer

ColumnLayout {
    id: root

    property string label: ""
    property string value: ""
    property string placeholder: ""
    property bool readOnly: false
    property var options: []          // if non-empty → combo style display
    property int currentIndex: 0

    spacing: Theme.spacingXs
    Layout.fillWidth: true

    Text {
        text: root.label
        color: Theme.textMuted
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontXs
        font.weight: Font.DemiBold
    }

    Rectangle {
        Layout.fillWidth: true
        Layout.preferredHeight: 36
        radius: Theme.radiusMd
        color: root.readOnly ? Theme.bgElevated : Theme.surfaceRaised
        border.color: fieldMouse.containsMouse ? Theme.borderStrong : Theme.border

        Behavior on border.color { ColorAnimation { duration: Theme.animFast } }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: Theme.spacingSm
            anchors.rightMargin: Theme.spacingSm
            spacing: Theme.spacingXs

            Text {
                Layout.fillWidth: true
                text: {
                    if (root.options.length > 0)
                        return root.options[Math.min(root.currentIndex, root.options.length - 1)]
                    return root.value.length > 0 ? root.value : root.placeholder
                }
                color: (root.value.length > 0 || root.options.length > 0)
                       ? Theme.text : Theme.textFaint
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSm
                elide: Text.ElideRight
                verticalAlignment: Text.AlignVCenter
            }

            Text {
                visible: root.options.length > 0
                text: "▾"
                color: Theme.textMuted
                font.pixelSize: Theme.fontSm
            }
        }

        MouseArea {
            id: fieldMouse
            anchors.fill: parent
            hoverEnabled: true
            acceptedButtons: Qt.NoButton
        }
    }
}
