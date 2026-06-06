import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import STM32AiDeployer

// Scrolling list of the most recent factory alarms/events.
Rectangle {
    id: root

    property var model: []   // list of { time, nodeName, zoneName, severity, message }

    radius: Theme.radiusMd
    color: Theme.surface
    border.color: Theme.border
    clip: true

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // empty state
        Item {
            visible: root.model.length === 0
            Layout.fillWidth: true
            Layout.fillHeight: true
            Text {
                anchors.centerIn: parent
                text: "Aktif alarm yok — tüm hatlar normal."
                color: Theme.textFaint
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSm
            }
        }

        ListView {
            visible: root.model.length > 0
            Layout.fillWidth: true
            Layout.fillHeight: true
            model: root.model
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar { width: 8 }

            delegate: Item {
                width: ListView.view.width
                implicitHeight: 58

                readonly property color sev: Theme.statusColor(
                    modelData.severity === "critical" ? "error" : modelData.severity)

                Rectangle {
                    anchors.fill: parent
                    anchors.margins: 4
                    radius: Theme.radiusSm
                    color: Theme.alpha(sev, 0.06)

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: Theme.spacingSm
                        anchors.rightMargin: Theme.spacingSm
                        spacing: Theme.spacingSm

                        Rectangle {
                            Layout.preferredWidth: 3
                            Layout.fillHeight: true
                            Layout.topMargin: 8
                            Layout.bottomMargin: 8
                            radius: 2
                            color: sev
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 1
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: Theme.spacingXs
                                Text {
                                    text: modelData.nodeName
                                    color: Theme.text
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontXs
                                    font.weight: Font.DemiBold
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }
                                Text {
                                    text: modelData.time
                                    color: Theme.textFaint
                                    font.family: Theme.monoFamily
                                    font.pixelSize: 10
                                }
                            }
                            Text {
                                text: modelData.message
                                color: Theme.textMuted
                                font.family: Theme.fontFamily
                                font.pixelSize: 11
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }
                        }
                    }
                }
            }
        }
    }
}
