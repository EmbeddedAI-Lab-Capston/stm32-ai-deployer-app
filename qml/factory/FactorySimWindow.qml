import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import STM32AiDeployer

// Standalone-feeling window for the Factory Simulation mode. Has its own title
// bar, navigation and screens, and drives the C++ FactorySimulator engine.
ApplicationWindow {
    id: win
    width: 1480
    height: 920
    minimumWidth: 1040
    minimumHeight: 680
    visible: false
    title: "Fabrika Simülasyonu — STM32 AI Deployer"
    color: Theme.bg
    flags: Qt.Window | Qt.FramelessWindowHint

    readonly property int edge: 6
    property int currentIndex: 0
    property int selectedZoneId: 1
    property int selectedNodeId: 1
    property int rev: 0

    // Start the engine when shown, stop it when hidden.
    onVisibleChanged: {
        if (typeof factorySim === "undefined" || !factorySim) return
        if (visible) factorySim.start()
        else factorySim.stop()
    }

    Connections {
        target: (typeof factorySim !== "undefined") ? factorySim : null
        function onTick() { win.rev++ }
    }

    function goNode(id) { win.selectedNodeId = id; win.currentIndex = 3 }
    function goZone(id) { win.selectedZoneId = id; win.currentIndex = 2 }

    Rectangle {
        anchors.fill: parent
        color: Theme.bg
        border.color: Theme.borderStrong
        border.width: win.visibility === Window.Maximized ? 0 : 1

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: win.visibility === Window.Maximized ? 0 : 1
            spacing: 0

            // ── Custom title bar ───────────────────────────────────────────
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: Theme.titleBarHeight
                color: Theme.bgElevated

                Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.border }

                MouseArea {
                    anchors.fill: parent
                    onPressed: win.startSystemMove()
                    onDoubleClicked: win.visibility === Window.Maximized ? win.showNormal() : win.showMaximized()
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Theme.spacingMd
                    anchors.rightMargin: Theme.spacingSm
                    spacing: Theme.spacingSm

                    Rectangle {
                        Layout.preferredWidth: 26; Layout.preferredHeight: 26
                        radius: Theme.radiusSm
                        gradient: Gradient {
                            GradientStop { position: 0.0; color: Theme.purple }
                            GradientStop { position: 1.0; color: Theme.primary }
                        }
                        Text { anchors.centerIn: parent; text: "🏭"; font.pixelSize: 13 }
                    }
                    Text {
                        text: "Fabrika Simülasyonu"
                        color: Theme.text
                        font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm; font.weight: Font.DemiBold
                    }
                    Rectangle {
                        Layout.preferredHeight: 20
                        Layout.preferredWidth: demoLbl.implicitWidth + Theme.spacingSm
                        radius: Theme.radiusSm
                        color: Theme.alpha(Theme.purple, 0.18)
                        Text { id: demoLbl; anchors.centerIn: parent; text: "DEMO"
                               color: Theme.purple; font.family: Theme.fontFamily
                               font.pixelSize: 10; font.weight: Font.Bold }
                    }

                    Item { Layout.fillWidth: true }

                    // engine controls
                    AppButton {
                        text: (typeof factorySim !== "undefined" && factorySim && factorySim.running) ? "Duraklat" : "Başlat"
                        iconText: (typeof factorySim !== "undefined" && factorySim && factorySim.running) ? "❚❚" : "▶"
                        variant: "secondary"
                        onClicked: {
                            if (typeof factorySim === "undefined" || !factorySim) return
                            factorySim.running ? factorySim.stop() : factorySim.start()
                        }
                    }
                    RowLayout {
                        spacing: Theme.spacingXs
                        Text { text: "Hız"; color: Theme.textMuted
                               font.family: Theme.fontFamily; font.pixelSize: Theme.fontXs }
                        Repeater {
                            model: [1, 2, 4]
                            delegate: Rectangle {
                                Layout.preferredWidth: 30; Layout.preferredHeight: 24
                                radius: Theme.radiusSm
                                property bool active: (typeof factorySim !== "undefined" && factorySim)
                                                      ? Math.abs(factorySim.speed - modelData) < 0.01 : false
                                color: active ? Theme.primarySoft : (spMouse.containsMouse ? Theme.surfaceHover : Theme.surfaceRaised)
                                border.color: active ? Theme.primary : Theme.border
                                Text { anchors.centerIn: parent; text: modelData + "×"
                                       color: active ? Theme.primary : Theme.textMuted
                                       font.family: Theme.fontFamily; font.pixelSize: Theme.fontXs
                                       font.weight: active ? Font.DemiBold : Font.Normal }
                                MouseArea { id: spMouse; anchors.fill: parent; hoverEnabled: true
                                    onClicked: if (typeof factorySim !== "undefined" && factorySim) factorySim.speed = modelData }
                            }
                        }
                    }

                    Item { Layout.preferredWidth: Theme.spacingSm }

                    // window controls
                    Repeater {
                        model: [ { id: "min", glyph: "–", hover: Theme.surfaceHover },
                                 { id: "max", glyph: "□", hover: Theme.surfaceHover },
                                 { id: "close", glyph: "✕", hover: Theme.danger } ]
                        delegate: Rectangle {
                            Layout.preferredWidth: 38; Layout.preferredHeight: 30
                            radius: Theme.radiusSm
                            color: ctrlMouse.containsMouse ? modelData.hover : "transparent"
                            Behavior on color { ColorAnimation { duration: Theme.animFast } }
                            Text { anchors.centerIn: parent; text: modelData.glyph
                                   color: (modelData.id === "close" && ctrlMouse.containsMouse) ? "#FFFFFF" : Theme.textMuted
                                   font.pixelSize: Theme.fontSm }
                            MouseArea {
                                id: ctrlMouse; anchors.fill: parent; hoverEnabled: true
                                onClicked: {
                                    if (modelData.id === "min") win.showMinimized()
                                    else if (modelData.id === "max") win.visibility === Window.Maximized ? win.showNormal() : win.showMaximized()
                                    else win.close()
                                }
                            }
                        }
                    }
                }
            }

            // ── Navigation bar ─────────────────────────────────────────────
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: Theme.tabBarHeight
                color: Theme.bg
                Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.border }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Theme.spacingMd
                    anchors.rightMargin: Theme.spacingMd
                    spacing: Theme.spacingXs

                    Repeater {
                        model: [
                            { label: "Genel Bakış", icon: "▦" },
                            { label: "Fabrika Krokisi", icon: "🗺" },
                            { label: "Bölge Detayı", icon: "▤" },
                            { label: "Node & Alarm", icon: "◎" }
                        ]
                        delegate: Item {
                            Layout.preferredHeight: parent.height
                            Layout.preferredWidth: navLabel.implicitWidth + Theme.spacingXl
                            property bool active: index === win.currentIndex

                            Rectangle {
                                anchors.centerIn: parent
                                width: parent.width; height: 34
                                radius: Theme.radiusMd
                                color: (!active && navMouse.containsMouse) ? Theme.surfaceRaised : "transparent"
                                Behavior on color { ColorAnimation { duration: Theme.animFast } }
                            }
                            Text {
                                id: navLabel
                                anchors.centerIn: parent
                                text: modelData.label
                                color: active ? Theme.primary : Theme.textMuted
                                font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm
                                font.weight: active ? Font.DemiBold : Font.Normal
                            }
                            Rectangle {
                                visible: active
                                anchors.bottom: parent.bottom
                                anchors.horizontalCenter: parent.horizontalCenter
                                width: navLabel.implicitWidth + Theme.spacingSm
                                height: 2; radius: 1; color: Theme.primary
                            }
                            MouseArea {
                                id: navMouse; anchors.fill: parent; hoverEnabled: true
                                onClicked: win.currentIndex = index
                            }
                        }
                    }

                    Item { Layout.fillWidth: true }

                    // live status chips
                    StatusPill {
                        text: (typeof factorySim !== "undefined" && factorySim)
                              ? (factorySim.activeAlarmCount + " aktif alarm") : "—"
                        status: (typeof factorySim !== "undefined" && factorySim && factorySim.criticalCount > 0) ? "error"
                                : ((typeof factorySim !== "undefined" && factorySim && factorySim.warningCount > 0) ? "warning" : "ok")
                    }
                    StatusPill {
                        text: (typeof factorySim !== "undefined" && factorySim)
                              ? (factorySim.nodeCount + " node canlı") : "—"
                        status: "info"
                    }
                }
            }

            // ── Screens ────────────────────────────────────────────────────
            StackLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                currentIndex: win.currentIndex

                FactoryDashboard {
                    rev: win.rev
                    onOpenZone: (id) => win.goZone(id)
                }
                FactoryMap {
                    rev: win.rev
                    onOpenZone: (id) => win.goZone(id)
                    onOpenNode: (id) => win.goNode(id)
                }
                ZoneDetail {
                    rev: win.rev
                    zoneId: win.selectedZoneId
                    onOpenNode: (id) => win.goNode(id)
                }
                NodeDetail {
                    rev: win.rev
                    nodeId: win.selectedNodeId
                }
            }
        }
    }

    // ── Resize handles ─────────────────────────────────────────────────────
    MouseArea {
        anchors.left: parent.left; anchors.top: parent.top; anchors.bottom: parent.bottom
        width: win.edge; cursorShape: Qt.SizeHorCursor
        onPressed: win.startSystemResize(Qt.LeftEdge)
    }
    MouseArea {
        anchors.right: parent.right; anchors.top: parent.top; anchors.bottom: parent.bottom
        width: win.edge; cursorShape: Qt.SizeHorCursor
        onPressed: win.startSystemResize(Qt.RightEdge)
    }
    MouseArea {
        anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
        height: win.edge; cursorShape: Qt.SizeVerCursor
        onPressed: win.startSystemResize(Qt.BottomEdge)
    }
    MouseArea {
        anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
        height: win.edge; cursorShape: Qt.SizeVerCursor
        onPressed: win.startSystemResize(Qt.TopEdge)
    }
    MouseArea {
        anchors.right: parent.right; anchors.bottom: parent.bottom
        width: win.edge * 2; height: win.edge * 2; cursorShape: Qt.SizeFDiagCursor
        onPressed: win.startSystemResize(Qt.RightEdge | Qt.BottomEdge)
    }
}
