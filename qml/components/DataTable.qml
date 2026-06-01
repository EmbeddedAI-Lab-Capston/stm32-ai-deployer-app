import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import STM32AiDeployer

Rectangle {
    id: root

    // columns: [{ title, width(optional, 0=stretch) }]
    // rows: list of arrays (each array = cell strings, same length as columns)
    property var columns: []
    property var rows: []

    radius: Theme.radiusMd
    color: Theme.surface
    border.color: Theme.border
    clip: true

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // ── Header ────────────────────────────────────────────────────────
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 40
            color: Theme.surfaceRaised
            radius: 0

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Theme.spacingMd
                anchors.rightMargin: Theme.spacingMd
                spacing: Theme.spacingMd

                Repeater {
                    model: root.columns
                    delegate: Text {
                        text: modelData.title
                        color: Theme.primary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontXs
                        font.weight: Font.Bold
                        font.capitalization: Font.AllUppercase
                        elide: Text.ElideRight
                        readonly property int colWidth: modelData.width !== undefined ? modelData.width
                                                        : (modelData.w !== undefined ? modelData.w : 0)
                        Layout.fillWidth: colWidth === 0
                        Layout.preferredWidth: colWidth > 0 ? colWidth : 0
                    }
                }
            }
        }

        Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Theme.border }

        // ── Body ──────────────────────────────────────────────────────────
        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            model: root.rows
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar { width: 8 }

            delegate: Rectangle {
                width: list.width
                height: 46
                color: (index % 2 === 0) ? "transparent" : Theme.alpha(Theme.surfaceRaised, 0.5)

                property var rowData: modelData

                Rectangle {
                    anchors.bottom: parent.bottom
                    width: parent.width
                    height: 1
                    color: Theme.alpha(Theme.border, 0.6)
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Theme.spacingMd
                    anchors.rightMargin: Theme.spacingMd
                    spacing: Theme.spacingMd

                    Repeater {
                        model: root.columns
                        delegate: Text {
                            property int colIndex: index
                            text: rowData[colIndex] !== undefined ? rowData[colIndex] : ""
                            color: colIndex === 0 ? Theme.text : Theme.textMuted
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSm
                            font.weight: colIndex === 0 ? Font.DemiBold : Font.Normal
                            elide: Text.ElideRight
                            readonly property int colWidth: modelData.width !== undefined ? modelData.width
                                                            : (modelData.w !== undefined ? modelData.w : 0)
                            Layout.fillWidth: colWidth === 0
                            Layout.preferredWidth: colWidth > 0 ? colWidth : 0
                        }
                    }
                }
            }
        }
    }
}
