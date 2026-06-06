import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import STM32AiDeployer

ApplicationWindow {
    id: win
    width: 1500
    height: 940
    minimumWidth: 1040
    minimumHeight: 700
    visible: false
    title: "STM32 AI Deployer"
    color: Theme.bg
    flags: Qt.Window | Qt.FramelessWindowHint
    Component.onCompleted: console.log("Main.qml loaded, visible=" + visible + " size=" + width + "x" + height)

    readonly property int edge: 6   // resize border thickness

    // ── Root frame (border so frameless window has an edge) ────────────────
    Rectangle {
        anchors.fill: parent
        color: Theme.bg
        border.color: Theme.borderStrong
        border.width: win.visibility === Window.Maximized ? 0 : 1

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: win.visibility === Window.Maximized ? 0 : 1
            spacing: 0

            TitleBar {
                Layout.fillWidth: true
                targetWindow: win
                onRequestMinimize: win.showMinimized()
                onRequestToggleMaximize:
                    win.visibility === Window.Maximized ? win.showNormal() : win.showMaximized()
                onRequestClose: win.close()
                onOpenSettings: settingsDialog.open()
                onOpenAbout: aboutDialog.open()
                onOpenFactorySim: {
                    factorySimWindow.show()
                    factorySimWindow.raise()
                    factorySimWindow.requestActivate()
                }
            }

            TopTabBar {
                id: tabBar
                Layout.fillWidth: true
                connected: (typeof appState !== "undefined" && appState) ? appState.connected : MockData.connected
                boardName: (typeof appState !== "undefined" && appState)
                           ? (appState.boardName.length > 0 ? appState.boardName : "Kart") : MockData.boardName
                tabs: [
                    { label: "Dashboard" },
                    { label: "Kartlar" },
                    { label: "Flash" },
                    { label: "Monitör" },
                    { label: "Benchmark" },
                    { label: "Analiz" }
                ]
                onSelected: (index) => stack.currentIndex = index
            }

            StackLayout {
                id: stack
                Layout.fillWidth: true
                Layout.fillHeight: true
                currentIndex: tabBar.currentIndex

                DashboardScreen {
                    onOpenPipelineRequested: {
                        tabBar.currentIndex = 2
                        stack.currentIndex = 2
                    }
                    onOpenLogsRequested: {
                        tabBar.currentIndex = 3
                        stack.currentIndex = 3
                    }
                }
                BoardScreen {}
                FlashScreen {}
                MonitorScreen {}
                BenchmarkScreen {}
                AnalysisScreen {}
            }
        }
    }

    // ── Dialogs ────────────────────────────────────────────────────────────
    SettingsDialog { id: settingsDialog }
    AboutDialog { id: aboutDialog }

    // ── Factory Simulation (separate standalone-feeling window) ─────────────
    FactorySimWindow { id: factorySimWindow }

    // ── Resize handles (frameless) ─────────────────────────────────────────
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
    MouseArea {
        anchors.left: parent.left; anchors.bottom: parent.bottom
        width: win.edge * 2; height: win.edge * 2; cursorShape: Qt.SizeBDiagCursor
        onPressed: win.startSystemResize(Qt.LeftEdge | Qt.BottomEdge)
    }
}
