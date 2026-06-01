import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import STM32AiDeployer

// A labelled ComboBox that actually works — replaces the fake FormField for dropdowns.
ColumnLayout {
    id: root

    property string label: ""
    property var    options: []
    property int    currentIndex: 0
    property alias  model: combo.model

    signal activated(int index)
    spacing: Theme.spacingXs
    Layout.fillWidth: true

    Text {
        text: root.label
        color: Theme.textMuted
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontXs
        font.weight: Font.DemiBold
    }

    ComboBox {
        id: combo
        Layout.fillWidth: true
        model: root.options
        currentIndex: root.currentIndex
        onActivated: (idx) => {
            root.currentIndex = idx
            root.activated(idx)
        }

        background: Rectangle {
            radius: Theme.radiusMd
            color: Theme.surfaceRaised
            border.color: combo.activeFocus ? Theme.primary : Theme.border
            Behavior on border.color { ColorAnimation { duration: Theme.animFast } }
        }

        contentItem: Text {
            leftPadding: Theme.spacingSm
            text: combo.displayText
            color: Theme.text
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSm
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }

        indicator: Text {
            x: combo.width - width - Theme.spacingSm
            y: (combo.height - height) / 2
            text: "▾"
            color: Theme.textMuted
            font.pixelSize: Theme.fontSm
        }

        popup: Popup {
            y: combo.height + 2
            width: combo.width
            implicitHeight: Math.min(contentItem.implicitHeight, 240)
            padding: 4

            background: Rectangle {
                color: Theme.surface
                radius: Theme.radiusMd
                border.color: Theme.border
            }

            contentItem: ListView {
                implicitHeight: contentHeight
                model: combo.popup.visible ? combo.delegateModel : null
                clip: true
                ScrollBar.vertical: ScrollBar { width: 6 }
            }
        }

        delegate: ItemDelegate {
            width: combo.width
            highlighted: combo.highlightedIndex === index
            contentItem: Text {
                text: modelData !== undefined ? modelData : ""
                color: highlighted ? Theme.primary : Theme.text
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSm
                verticalAlignment: Text.AlignVCenter
                leftPadding: Theme.spacingSm
            }
            background: Rectangle {
                color: highlighted ? Theme.primarySoft : "transparent"
                radius: Theme.radiusSm
            }
        }
    }
}
