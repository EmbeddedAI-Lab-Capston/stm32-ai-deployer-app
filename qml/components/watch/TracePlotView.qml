import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import STM32AiDeployer

// Axis + legend + cursor + zoom wrapper around the TracePlot paint engine
// (plan docs/variable_watcher_plan.md Bolum 9.1/9.4). Polls the Backend
// facade at 25 Hz for a pre-decimated frame — cost is bounded by pixel
// width (<=800 columns), independent of the sample rate (3000 Hz costs the
// same as 10 Hz here).
Item {
    id: root

    readonly property bool _hasBackend: (typeof backend !== "undefined" && backend)

    property real windowSec: 10.0
    readonly property int columns: Math.max(1, Math.min(800, Math.round(plotArea.width)))

    property real _windowStart: 0.0
    property real _windowEnd: 1.0
    property real pinnedCursorTime: -1   // set by clicking an event marker

    property var _rows: []   // legend: backend.watchItems rows

    function refreshFrame() {
        if (!root._hasBackend) return
        _windowEnd = backend.watchSessionNow()
        _windowStart = Math.max(0, _windowEnd - root.windowSec)
        plot.windowStart = _windowStart
        plot.windowEnd = _windowEnd > _windowStart ? _windowEnd : _windowStart + 1

        const frame = backend.watchPlotFrame(root.columns, root.windowSec)
        plot.frame = frame

        let maxLane = 0
        for (let i = 0; i < frame.length; ++i)
            maxLane = Math.max(maxLane, frame[i].laneIndex + 1)
        plot.laneCount = Math.max(1, maxLane)

        plot.events = backend.watchEvents(_windowStart, plot.windowEnd)
        // NOTE: watchItems is a Q_PROPERTY, not an invokable. Calling it as a
        // function threw a TypeError on every frame since Faz 6, so the legend
        // and the hover value box never had any rows to show.
        _rows = backend.watchItems
    }

    Timer {
        interval: 40   // 25 Hz, plan Bolum 9.4
        running: root._hasBackend && backend.watchLinkOpen
        repeat: true
        onTriggered: root.refreshFrame()
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: Theme.spacingXs

        // ── Legend + zoom hint ───────────────────────────────────────────
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spacingMd

            // Flow, not a row: with many traces a single row ran off the right
            // edge and pushed the zoom hint out with it. Hidden entries take
            // no space in a positioner.
            Flow {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignTop
                spacing: Theme.spacingMd

                Repeater {
                    model: root._rows
                    delegate: Row {
                        spacing: 4
                        visible: modelData.enabled && modelData.plotVisible !== false
                        Rectangle {
                            width: 10; height: 10; radius: 2
                            anchors.verticalCenter: parent.verticalCenter
                            color: modelData.color || Theme.textFaint
                        }
                        Text {
                            text: modelData.label + (modelData.hasValue ? ("  " + modelData.liveValue) : "")
                            color: Theme.textMuted
                            font.family: Theme.fontFamily; font.pixelSize: Theme.fontXs
                        }
                    }
                }
            }

            Text {
                Layout.alignment: Qt.AlignTop
                text: root.windowSec.toFixed(0) + " s pencere — tekerlek: yakınlaştır/uzaklaştır"
                color: Theme.textFaint
                font.family: Theme.fontFamily; font.pixelSize: Theme.fontXs
            }
        }

        // ── Plot ──────────────────────────────────────────────────────────
        Rectangle {
            id: plotArea
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: Theme.surface
            radius: Theme.radiusMd
            border.color: Theme.border
            clip: true

            TracePlot {
                id: plot
                anchors.fill: parent
                anchors.margins: 1
                gridColor: Theme.border
                axisColor: Theme.textFaint
                cursorTime: hoverArea.containsMouse ? plot.timeAtX(hoverArea.mouseX) : root.pinnedCursorTime
            }

            MouseArea {
                id: hoverArea
                anchors.fill: parent
                hoverEnabled: true
                acceptedButtons: Qt.NoButton
                onWheel: (wheel) => {
                    const factor = wheel.angleDelta.y > 0 ? (1 / 1.2) : 1.2
                    root.windowSec = Math.max(1, Math.min(300, root.windowSec * factor))
                    wheel.accepted = true
                }
            }

            Rectangle {
                id: cursorBox
                visible: hoverArea.containsMouse && root._hasBackend && root._rows.length > 0
                x: Math.min(hoverArea.mouseX + 8, parent.width - width - 4)
                y: 4
                width: cursorCol.implicitWidth + Theme.spacingSm * 2
                height: cursorCol.implicitHeight + Theme.spacingXs
                radius: Theme.radiusSm
                color: Theme.alpha(Theme.bgElevated, 0.92)
                border.color: Theme.border

                property var _values: (visible) ? backend.watchValuesAt(plot.timeAtX(hoverArea.mouseX)) : ({})

                ColumnLayout {
                    id: cursorCol
                    x: Theme.spacingSm
                    y: Theme.spacingXs / 2
                    spacing: 2
                    Repeater {
                        model: root._rows
                        delegate: Text {
                            visible: modelData.enabled && modelData.plotVisible !== false
                            property var v: cursorBox._values[modelData.id]
                            text: modelData.label + ": " + (v ? v.formatted : "—")
                            color: modelData.color || Theme.text
                            font.family: Theme.monoFamily; font.pixelSize: Theme.fontXs
                        }
                    }
                }
            }
        }

        WatchEventLane {
            Layout.fillWidth: true
            windowStart: root._windowStart
            windowEnd: plot.windowEnd
            events: plot.events
            onEventClicked: (t) => { root.pinnedCursorTime = t }
        }
    }
}
