import QtQuick
import QtQuick.Controls
import STM32AiDeployer

Rectangle {
    id: root

    // model: list of { text, type } where type ∈ info|ok|warn|err|cmd
    property var lines: []

    radius: Theme.radiusMd
    color: "#0A0E14"
    border.color: Theme.border
    clip: true

    function lineColor(type) {
        switch (type) {
        case "ok":   return Theme.success
        case "warn": return Theme.warning
        case "err":  return Theme.danger
        case "cmd":  return Theme.primary
        default:     return Theme.textMuted
        }
    }

    // Copies all terminal lines to system clipboard.
    function copyAll() {
        var parts = []
        for (var i = 0; i < root.lines.length; ++i)
            parts.push(root.lines[i].text)
        _clipboardHelper.text = parts.join("\n")
        _clipboardHelper.selectAll()
        _clipboardHelper.copy()
        _copyFeedback.visible = true
        _feedbackTimer.restart()
    }

    // Copies a single line to clipboard.
    function copyLine(text) {
        _clipboardHelper.text = text
        _clipboardHelper.selectAll()
        _clipboardHelper.copy()
        _copyFeedback.visible = true
        _feedbackTimer.restart()
    }

    // Hidden TextEdit used only as a clipboard bridge (Qt 6 QML pattern).
    TextEdit {
        id: _clipboardHelper
        visible: false
        text: ""
    }

    Timer {
        id: _feedbackTimer
        interval: 1200
        onTriggered: _copyFeedback.visible = false
    }

    // ── Main list ────────────────────────────────────────────────────────────
    ListView {
        id: list
        anchors.fill: parent
        anchors.margins: Theme.spacingSm
        anchors.rightMargin: Theme.spacingSm + 28  // leave room for copy button
        model: root.lines
        clip: true
        spacing: 1
        boundsBehavior: Flickable.StopAtBounds

        ScrollBar.vertical: ScrollBar { width: 8 }

        delegate: Item {
            width: list.width
            height: lineText.implicitHeight + 1

            Text {
                id: lineText
                width: parent.width
                text: modelData.text
                color: root.lineColor(modelData.type)
                font.family: Theme.monoFamily
                font.pixelSize: Theme.fontXs
                wrapMode: Text.WrapAnywhere
            }

            // Right-click → copy this line
            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.RightButton
                onClicked: (mouse) => {
                    if (mouse.button === Qt.RightButton) {
                        _lineMenu.targetText = modelData.text
                        _lineMenu.popup()
                    }
                }
            }
        }

        onCountChanged: {
            if (atYEnd || count <= 1)
                positionViewAtEnd()
        }
    }

    // ── Copy-all button (top-right corner) ──────────────────────────────────
    Rectangle {
        id: _copyBtn
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 4
        width: 24
        height: 24
        radius: 4
        color: _copyBtnArea.containsMouse ? Theme.alpha(Theme.primary, 0.25) : Theme.alpha(Theme.primary, 0.10)
        border.color: Theme.alpha(Theme.primary, 0.35)

        Text {
            anchors.centerIn: parent
            text: "⎘"
            color: Theme.primary
            font.pixelSize: 13
        }

        ToolTip.visible: _copyBtnArea.containsMouse
        ToolTip.text: "Tümünü kopyala"
        ToolTip.delay: 400

        MouseArea {
            id: _copyBtnArea
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: root.copyAll()
        }
    }

    // ── "Kopyalandı" feedback badge ──────────────────────────────────────────
    Rectangle {
        id: _copyFeedback
        visible: false
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 6
        width: feedbackLabel.implicitWidth + 12
        height: 20
        radius: 4
        color: Theme.alpha(Theme.success, 0.85)

        Text {
            id: feedbackLabel
            anchors.centerIn: parent
            text: "Kopyalandı"
            color: "#ffffff"
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontXs
            font.weight: Font.DemiBold
        }
    }

    // ── Right-click context menu ─────────────────────────────────────────────
    Menu {
        id: _lineMenu
        property string targetText: ""

        MenuItem {
            text: "Satırı Kopyala"
            onTriggered: root.copyLine(_lineMenu.targetText)
        }
        MenuItem {
            text: "Tümünü Kopyala"
            onTriggered: root.copyAll()
        }
        MenuSeparator {}
        MenuItem {
            text: "Sona Git"
            onTriggered: list.positionViewAtEnd()
        }
    }
}
