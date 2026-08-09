import QtQuick
import QtQuick.Controls
import STM32AiDeployer

// Compact horizontal event strip — one clickable tick per event, positioned
// by time on the same [windowStart,windowEnd] axis as TracePlotView. Distinct
// from TracePlot's own background event lines (drawn behind the traces):
// this is the interactive "click to jump to an event" affordance (plan
// docs/variable_watcher_plan.md Bolum 9.1).
Rectangle {
    id: root

    property double windowStart: 0.0
    property double windowEnd: 1.0
    property var events: []   // [{t,kind,text,severity}]
    signal eventClicked(double t)

    implicitHeight: 22
    color: Theme.bgElevated
    radius: Theme.radiusSm
    border.color: Theme.border
    clip: true

    function colorFor(kind, severity) {
        if (kind === "targetReset") return "#D88A2A"
        if (severity === "error") return "#F0616D"
        if (severity === "warning") return "#D8A23A"
        return Theme.textFaint
    }

    Repeater {
        model: root.events
        delegate: Rectangle {
            property double _span: Math.max(0.001, root.windowEnd - root.windowStart)
            x: Math.round((modelData.t - root.windowStart) / _span * root.width) - 2
            y: (root.height - height) / 2
            width: 4
            height: 4
            radius: 2
            color: root.colorFor(modelData.kind, modelData.severity)

            MouseArea {
                id: dotArea
                anchors.fill: parent
                anchors.margins: -4
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: root.eventClicked(modelData.t)
                ToolTip.visible: containsMouse
                ToolTip.delay: 200
                ToolTip.text: modelData.text
            }
        }
    }

    Text {
        visible: root.events.length === 0
        anchors.centerIn: parent
        text: "Olay yok"
        color: Theme.textFaint
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontXs
    }
}
