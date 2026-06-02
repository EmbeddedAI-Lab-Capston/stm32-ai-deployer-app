import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import STM32AiDeployer

Rectangle {
    id: root

    // columns: [{ title, width(optional, 0=stretch) }]
    // rows: list of arrays or { id, kind, cells }
    property var columns: []
    property var rows: []
    property int selectedIndex: -1
    property int selectedId: -1
    property var selectedCells: []
    signal rowSelected(int id, var cells, int index)

    // Minimum width used for columns declared with width 0 (auto/stretch).
    // Giving every column a real width lets the content overflow horizontally
    // so no data gets clipped — a horizontal scrollbar appears instead.
    property int defaultColWidth: 140

    function rowCells(row) {
        if (row && row.cells !== undefined)
            return row.cells
        return row || []
    }

    function rowId(row) {
        return row && row.id !== undefined ? row.id : -1
    }

    function colWidthOf(col) {
        var w = col.width !== undefined ? col.width
                : (col.w !== undefined ? col.w : 0)
        return w > 0 ? w : root.defaultColWidth
    }

    // Natural width required to render all columns without clipping.
    function naturalWidth() {
        var total = Theme.spacingMd * 2 // left + right margins
        for (var i = 0; i < columns.length; ++i) {
            total += colWidthOf(columns[i])
            if (i > 0)
                total += Theme.spacingMd // inter-column spacing
        }
        return total
    }

    radius: Theme.radiusMd
    color: Theme.surface
    border.color: Theme.border
    clip: true

    Flickable {
        id: hFlick
        anchors.fill: parent
        contentWidth: Math.max(width, root.naturalWidth())
        contentHeight: height
        clip: true
        flickableDirection: Flickable.HorizontalFlick
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.horizontal: ScrollBar {
            policy: ScrollBar.AsNeeded
            height: 8
        }

        ColumnLayout {
            width: hFlick.contentWidth
            height: hFlick.height
            spacing: 0

            // ── Header ────────────────────────────────────────────────────
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
                            Layout.fillWidth: false
                            Layout.preferredWidth: root.colWidthOf(modelData)
                        }
                    }
                    // Fills any leftover space when the table is narrower than
                    // the viewport (keeps columns left-aligned).
                    Item { Layout.fillWidth: true }
                }
            }

            Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Theme.border }

            // ── Body ──────────────────────────────────────────────────────
            ListView {
                id: list
                Layout.fillWidth: true
                Layout.fillHeight: true
                model: root.rows
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar { width: 8 }

                delegate: Rectangle {
                    id: rowDelegate
                    width: list.width
                    height: 46
                    color: root.selectedIndex === index
                           ? Theme.primarySoft
                           : ((index % 2 === 0) ? "transparent" : Theme.alpha(Theme.surfaceRaised, 0.5))

                    property var rowData: root.rowCells(modelData)
                    property int recordId: root.rowId(modelData)

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
                                text: rowDelegate.rowData[colIndex] !== undefined ? rowDelegate.rowData[colIndex] : ""
                                color: colIndex === 0 ? Theme.text : Theme.textMuted
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSm
                                font.weight: colIndex === 0 ? Font.DemiBold : Font.Normal
                                elide: Text.ElideRight
                                Layout.fillWidth: false
                                Layout.preferredWidth: root.colWidthOf(modelData)
                            }
                        }
                        Item { Layout.fillWidth: true }
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            root.selectedIndex = index
                            root.selectedId = rowDelegate.recordId
                            root.selectedCells = rowDelegate.rowData
                            root.rowSelected(rowDelegate.recordId, rowDelegate.rowData, index)
                        }
                    }
                }
            }
        }
    }
}
