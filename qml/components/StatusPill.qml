import QtQuick
import QtQuick.Layouts
import STM32AiDeployer

Rectangle {
    id: root

    property string text: "Ready"
    property string status: "ready"

    implicitHeight: 26
    implicitWidth: pillContent.implicitWidth + Theme.spacingMd
    radius: height / 2
    color: Theme.alpha(Theme.statusColor(status), 0.14)
    border.color: Theme.alpha(Theme.statusColor(status), 0.40)

    RowLayout {
        id: pillContent
        anchors.centerIn: parent
        spacing: Theme.spacingXs

        Rectangle {
            Layout.preferredWidth: 7
            Layout.preferredHeight: 7
            radius: 4
            color: Theme.statusColor(root.status)
        }

        Text {
            text: root.text
            color: Theme.text
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontXs
            font.weight: Font.DemiBold
        }
    }
}
