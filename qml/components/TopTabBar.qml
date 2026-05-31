import QtQuick
import QtQuick.Layouts
import STM32AiDeployer

Rectangle {
    id: root

    property var tabs: []          // [{ label, icon }]
    property int currentIndex: 0
    signal selected(int index)

    // right-side status chips
    property bool connected: false
    property string boardName: ""

    implicitHeight: Theme.tabBarHeight
    color: Theme.bg

    Rectangle {
        anchors.bottom: parent.bottom
        width: parent.width
        height: 1
        color: Theme.border
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.spacingMd
        anchors.rightMargin: Theme.spacingMd
        spacing: Theme.spacingXs

        Repeater {
            model: root.tabs
            delegate: Item {
                Layout.preferredHeight: root.height
                Layout.preferredWidth: tabLabel.implicitWidth + Theme.spacingLg

                property bool active: index === root.currentIndex

                Rectangle {
                    anchors.centerIn: parent
                    width: parent.width
                    height: 34
                    radius: Theme.radiusMd
                    color: (!active && tabMouse.containsMouse) ? Theme.surfaceRaised : "transparent"
                    Behavior on color { ColorAnimation { duration: Theme.animFast } }
                }

                Text {
                    id: tabLabel
                    anchors.centerIn: parent
                    text: modelData.label
                    color: active ? Theme.primary : Theme.textMuted
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSm
                    font.weight: active ? Font.DemiBold : Font.Normal
                }

                // active underline
                Rectangle {
                    visible: active
                    anchors.bottom: parent.bottom
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: tabLabel.implicitWidth + Theme.spacingSm
                    height: 2
                    radius: 1
                    color: Theme.primary
                }

                MouseArea {
                    id: tabMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: {
                        root.currentIndex = index
                        root.selected(index)
                    }
                }
            }
        }

        Item { Layout.fillWidth: true }

        // connection status chip
        Rectangle {
            Layout.preferredHeight: 26
            Layout.preferredWidth: connRow.implicitWidth + Theme.spacingMd
            radius: height / 2
            color: Theme.alpha(root.connected ? Theme.success : Theme.danger, 0.14)
            border.color: Theme.alpha(root.connected ? Theme.success : Theme.danger, 0.40)

            RowLayout {
                id: connRow
                anchors.centerIn: parent
                spacing: Theme.spacingXs
                Rectangle {
                    Layout.preferredWidth: 7; Layout.preferredHeight: 7; radius: 4
                    color: root.connected ? Theme.success : Theme.danger
                }
                Text {
                    text: root.connected ? (root.boardName + " bağlı") : "Bağlantı yok"
                    color: Theme.text
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontXs
                    font.weight: Font.DemiBold
                }
            }
        }
    }
}
