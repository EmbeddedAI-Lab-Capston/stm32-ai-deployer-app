import QtQuick
import STM32AiDeployer

// Persistent, impossible-to-miss banner shown whenever the screen is
// showing a recorded trace instead of a live target (plan
// docs/variable_watcher_plan.md Bolum 10.3 — "KAYITTAN OYNATMA" must never
// be mistaken for live data).
Rectangle {
    id: root

    readonly property bool _hasBackend: (typeof backend !== "undefined" && backend)
    readonly property bool active: _hasBackend && backend.watchPlayback

    visible: active
    implicitHeight: active ? 30 : 0
    color: Theme.warning
    radius: Theme.radiusSm

    readonly property var _info: active ? backend.watchPlaybackInfo() : ({})

    Text {
        anchors.centerIn: parent
        text: "⏺ KAYITTAN OYNATMA — canlı hedef yok" +
              (root._info.model ? ("   (" + root._info.model + " / " + root._info.board + ")") : "")
        color: "#1A1200"
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSm
        font.weight: Font.Bold
    }
}
