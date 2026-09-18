import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import STM32AiDeployer

// Faz 8 rule violation feed. backend.watchViolations is refreshed by
// Backend's own 4 Hz internal timer (evaluateWatchRules()) and emits
// watchViolationsChanged() — this binds directly, no polling Timer needed
// here (plan docs/variable_watcher_plan.md Bolum 11.1).
Rectangle {
    id: root

    readonly property bool _hasBackend: (typeof backend !== "undefined" && backend)
    readonly property var _violations: _hasBackend ? backend.watchViolations : []

    // Collapsed = header line only. A new violation deliberately does NOT
    // expand the panel (that would shove the item table around mid-session);
    // the count in the header turns red instead.
    property bool collapsed: false
    readonly property int headerHeight: header.implicitHeight + 2 * Theme.spacingSm

    color: Theme.bgElevated
    radius: Theme.radiusSm
    border.color: (root.collapsed && root._violations.length > 0) ? Theme.danger : Theme.border
    implicitHeight: root.collapsed ? root.headerHeight : 120

    Component.onCompleted: {
        if (root._hasBackend)
            root.collapsed = backend.watchPanelCollapsed("rules")
    }

    function toggle() {
        root.collapsed = !root.collapsed
        if (root._hasBackend)
            backend.setWatchPanelCollapsed("rules", root.collapsed)
    }

    function colorFor(sev) {
        if (sev === "error") return Theme.danger
        if (sev === "warning") return Theme.warning
        return Theme.textMuted
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacingSm
        spacing: 4

        RowLayout {
            id: header
            objectName: "watch.ruleFeedHeader"
            Layout.fillWidth: true
            spacing: Theme.spacingSm

            TapHandler { onTapped: root.toggle() }
            HoverHandler { cursorShape: Qt.PointingHandCursor }

            CollapseChevron {
                collapsed: root.collapsed
                Layout.alignment: Qt.AlignVCenter
            }
            Text {
                text: "Kural İhlalleri"
                color: Theme.text
                font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm; font.weight: Font.DemiBold
            }
            Item { Layout.fillWidth: true }
            Text {
                objectName: "watch.ruleFeedCount"
                text: root._violations.length + " aktif"
                color: root._violations.length === 0 ? Theme.textFaint
                       : (root.collapsed ? Theme.danger : Theme.warning)
                font.family: Theme.fontFamily; font.pixelSize: Theme.fontXs
                font.weight: (root.collapsed && root._violations.length > 0) ? Font.DemiBold : Font.Normal
            }
        }

        ListView {
            id: list
            visible: !root.collapsed
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: root._violations
            ScrollBar.vertical: ScrollBar { width: 6 }

            delegate: RowLayout {
                width: list.width
                spacing: Theme.spacingSm
                Rectangle {
                    width: 8; height: 8; radius: 4
                    color: root.colorFor(modelData.severity)
                    Layout.alignment: Qt.AlignVCenter
                }
                Text {
                    Layout.fillWidth: true
                    text: modelData.message
                    color: Theme.textMuted
                    font.family: Theme.fontFamily; font.pixelSize: Theme.fontXs
                    elide: Text.ElideRight
                    ToolTip.visible: maHover.hovered
                    ToolTip.delay: 300
                    ToolTip.text: modelData.message
                    HoverHandler { id: maHover }
                }
            }
        }

        Text {
            visible: !root.collapsed && root._violations.length === 0
            Layout.alignment: Qt.AlignHCenter
            text: "İhlal yok"
            color: Theme.textFaint
            font.family: Theme.fontFamily; font.pixelSize: Theme.fontXs
        }
    }
}
