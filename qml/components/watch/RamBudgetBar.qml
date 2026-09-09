import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import STM32AiDeployer

// RAM bütçesi görselleştirmesi (Faz 10.2, docs/memory_telemetry_plan.md
// Bolum 4). backend.ramBudget bir Q_PROPERTY (NOTIFY ile canli) — bu
// bileşen yalnızca onu bir yığılmış çubuğa çeviriyor, hiçbir hesap
// yapmıyor (hesap src/modules/watcher/RamBudget.cpp'de, saf ve test edilmiş).
Rectangle {
    id: root

    readonly property bool _hasBackend: (typeof backend !== "undefined" && backend)
    readonly property var  _b: root._hasBackend ? backend.ramBudget : ({})
    readonly property bool _ok: root._b.ok === true
    readonly property bool _collision: root._ok && Number(root._b.freeBytes) < 0

    function _kb(bytes) {
        return (Number(bytes) / 1024).toFixed(1) + " KB"
    }

    implicitHeight: root._ok ? 64 : 34
    color: Theme.bgElevated
    radius: Theme.radiusMd
    border.color: root._collision ? Theme.danger : Theme.border

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacingSm
        spacing: Theme.spacingXs

        // ── Not available: no fabricated numbers, just the reason ──────────
        Text {
            visible: !root._ok
            Layout.fillWidth: true
            text: "RAM bütçesi: " + (root._b.warning || "izleyici bağlı değil")
            color: Theme.textMuted
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontXs
            elide: Text.ElideRight
        }

        // ── Summary line ─────────────────────────────────────────────────
        RowLayout {
            visible: root._ok
            Layout.fillWidth: true
            spacing: Theme.spacingSm

            Text {
                text: "RAM Bütçesi"
                color: Theme.text
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontXs
                font.weight: Font.DemiBold
            }
            Text {
                text: root._ok
                      ? (root._kb(root._b.staticUsed + root._b.heapUsed + root._b.stackUsed) + " / " + root._kb(root._b.ramTotal)
                         + "  (%" + root._b.usedPct.toFixed(1) + ")")
                      : ""
                color: Theme.textMuted
                font.family: Theme.monoFamily
                font.pixelSize: Theme.fontXs
            }
            Item { Layout.fillWidth: true }
            StatusPill {
                visible: root._collision
                text: "RAM ÇAKIŞMASI"
                status: "error"
                ToolTip.visible: collisionHover.hovered
                ToolTip.delay: 200
                ToolTip.text: root._b.warning || ""
                HoverHandler { id: collisionHover }
            }
        }

        // ── Stacked bar: static | heap | free | stack ───────────────────────
        Rectangle {
            visible: root._ok
            Layout.fillWidth: true
            Layout.preferredHeight: 18
            radius: Theme.radiusSm
            color: Theme.surface
            clip: true

            RowLayout {
                anchors.fill: parent
                spacing: 0

                Rectangle {
                    Layout.fillHeight: true
                    Layout.preferredWidth: parent.width * (root._ok ? root._b.staticUsed / root._b.ramTotal : 0)
                    color: Theme.primary
                    ToolTip.visible: staticHover.hovered
                    ToolTip.delay: 200
                    ToolTip.text: "Statik veri (.data+.bss): " + root._kb(root._b.staticUsed)
                    HoverHandler { id: staticHover }
                }
                Rectangle {
                    Layout.fillHeight: true
                    Layout.preferredWidth: parent.width * (root._ok ? root._b.heapUsed / root._b.ramTotal : 0)
                    color: Theme.purple
                    ToolTip.visible: heapHover.hovered
                    ToolTip.delay: 200
                    ToolTip.text: "Heap: " + root._kb(root._b.heapUsed)
                    HoverHandler { id: heapHover }
                }
                Rectangle {
                    Layout.fillHeight: true
                    Layout.fillWidth: true
                    color: root._collision ? Theme.danger : Theme.surfaceRaised
                    ToolTip.visible: freeHover.hovered
                    ToolTip.delay: 200
                    ToolTip.text: root._collision
                                  ? root._b.warning
                                  : ("Boş: " + root._kb(root._b.freeBytes))
                    HoverHandler { id: freeHover }
                }
                Rectangle {
                    Layout.fillHeight: true
                    Layout.preferredWidth: parent.width * (root._ok ? root._b.stackUsed / root._b.ramTotal : 0)
                    color: Theme.cyan
                    ToolTip.visible: stackHover.hovered
                    ToolTip.delay: 200
                    ToolTip.text: "Stack (en derin nokta): " + root._kb(root._b.stackUsed)
                    HoverHandler { id: stackHover }
                }
            }
        }
    }
}
