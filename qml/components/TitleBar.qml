import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import STM32AiDeployer

Rectangle {
    id: root

    property var targetWindow: null
    signal requestMinimize()
    signal requestToggleMaximize()
    signal requestClose()
    signal openSettings()
    signal openAbout()
    signal openFactorySim()

    implicitHeight: Theme.titleBarHeight
    color: Theme.bgElevated

    Rectangle {
        anchors.bottom: parent.bottom
        width: parent.width
        height: 1
        color: Theme.border
    }

    // ── Drag region (covers whole bar; menu/controls sit above it) ─────────
    MouseArea {
        anchors.fill: parent
        onPressed: if (root.targetWindow) root.targetWindow.startSystemMove()
        onDoubleClicked: root.requestToggleMaximize()
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.spacingMd
        anchors.rightMargin: Theme.spacingSm
        spacing: Theme.spacingSm

        // logo mark
        Rectangle {
            Layout.preferredWidth: 26
            Layout.preferredHeight: 26
            radius: Theme.radiusSm
            gradient: Gradient {
                GradientStop { position: 0.0; color: Theme.primary }
                GradientStop { position: 1.0; color: Theme.cyan }
            }
            Text {
                anchors.centerIn: parent
                text: "AI"
                color: "#FFFFFF"
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontXs
                font.weight: Font.Bold
            }
        }

        Text {
            text: "STM32 AI Deployer"
            color: Theme.text
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSm
            font.weight: Font.DemiBold
        }

        // ── Menu bar ───────────────────────────────────────────────────────
        Item { Layout.preferredWidth: Theme.spacingMd }

        Repeater {
            model: [
                { label: "Dosya",   items: ["Çıkış"] },
                { label: "Araçlar", items: ["Ayarlar"] },
                { label: "Yardım",  items: ["Hakkında"] }
            ]
            delegate: Item {
                id: menuRoot
                Layout.preferredHeight: 30
                Layout.preferredWidth: menuLabel.implicitWidth + Theme.spacingMd
                property int menuIndex: index

                Rectangle {
                    anchors.fill: parent
                    radius: Theme.radiusSm
                    color: (menuMouse.containsMouse || menu.opened) ? Theme.surfaceHover : "transparent"
                    Behavior on color { ColorAnimation { duration: Theme.animFast } }
                }

                Text {
                    id: menuLabel
                    anchors.centerIn: parent
                    text: modelData.label
                    color: Theme.text
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSm
                }

                MouseArea {
                    id: menuMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: menu.opened ? menu.close() : menu.open()
                }

                Popup {
                    id: menu
                    y: parent.height + 2
                    x: 0
                    padding: 6
                    implicitWidth: 180

                    background: Rectangle {
                        color: Theme.surface
                        radius: Theme.radiusMd
                        border.color: Theme.border
                    }

                    contentItem: ColumnLayout {
                        spacing: 2
                        Repeater {
                            model: modelData.items
                            delegate: Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 34
                                radius: Theme.radiusSm
                                color: itemMouse.containsMouse ? Theme.surfaceHover : "transparent"

                                Text {
                                    anchors.verticalCenter: parent.verticalCenter
                                    anchors.left: parent.left
                                    anchors.leftMargin: Theme.spacingSm
                                    text: modelData
                                    color: Theme.text
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontSm
                                }

                                MouseArea {
                                    id: itemMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    onClicked: {
                                        menu.close()
                                        if (modelData === "Ayarlar")  root.openSettings()
                                        else if (modelData === "Hakkında") root.openAbout()
                                        else if (modelData === "Çıkış") root.requestClose()
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        Item { Layout.fillWidth: true }

        // ── Factory Simulation launcher ────────────────────────────────────
        Rectangle {
            Layout.preferredHeight: 30
            Layout.preferredWidth: simRow.implicitWidth + Theme.spacingMd
            radius: Theme.radiusSm
            color: simMouse.containsMouse ? Theme.alpha(Theme.purple, 0.22) : Theme.alpha(Theme.purple, 0.12)
            border.color: Theme.alpha(Theme.purple, 0.45)
            Behavior on color { ColorAnimation { duration: Theme.animFast } }

            RowLayout {
                id: simRow
                anchors.centerIn: parent
                spacing: Theme.spacingXs
                Text { text: "🏭"; font.pixelSize: Theme.fontSm }
                Text {
                    text: "Fabrika Simülasyonu"
                    color: Theme.purple
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontXs
                    font.weight: Font.DemiBold
                }
            }
            MouseArea {
                id: simMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: root.openFactorySim()
            }
        }

        Item { Layout.preferredWidth: Theme.spacingSm }

        // ── Window controls ───────────────────────────────────────────────
        Repeater {
            model: [
                { id: "min",   glyph: "–", hover: Theme.surfaceHover },
                { id: "max",   glyph: "□", hover: Theme.surfaceHover },
                { id: "close", glyph: "✕", hover: Theme.danger }
            ]
            delegate: Rectangle {
                Layout.preferredWidth: 38
                Layout.preferredHeight: 30
                radius: Theme.radiusSm
                color: ctrlMouse.containsMouse ? modelData.hover : "transparent"

                Behavior on color { ColorAnimation { duration: Theme.animFast } }

                Text {
                    anchors.centerIn: parent
                    text: modelData.glyph
                    color: (modelData.id === "close" && ctrlMouse.containsMouse)
                           ? "#FFFFFF" : Theme.textMuted
                    font.pixelSize: Theme.fontSm
                }

                MouseArea {
                    id: ctrlMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: {
                        if (modelData.id === "min")   root.requestMinimize()
                        else if (modelData.id === "max") root.requestToggleMaximize()
                        else root.requestClose()
                    }
                }
            }
        }
    }
}
