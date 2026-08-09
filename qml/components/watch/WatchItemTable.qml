import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import STM32AiDeployer

// Watch item table — this phase: table only, no plot (plan
// docs/variable_watcher_plan.md Bolum 7.7). Live value column refreshes at
// 10 Hz (Timer-throttled re-read of backend.watchItems, which itself can be
// updated up to 30 Hz by VariableWatcher::samplesAppended).
Rectangle {
    id: root

    readonly property bool _hasBackend: (typeof backend !== "undefined" && backend)
    property var _rows: []

    function refresh() { _rows = root._hasBackend ? backend.watchItems : [] }

    Component.onCompleted: refresh()
    Connections {
        target: root._hasBackend ? backend : null
        function onWatchItemsChanged() { root.refresh() }
    }
    Timer {
        interval: 100   // 10 Hz — plan Bolum 7.7
        running: true
        repeat: true
        onTriggered: root.refresh()
    }

    readonly property var colWidths: [44, 160, 120, 56, 56, 74, 64, 110, 96, 96, 96, 40]
    readonly property var colTitles: ["Etkin", "Etiket", "Adres", "Tip", "Biçim", "Ölçek", "Birim",
                                       "Canlı Değer", "Min", "Max", "Ort", ""]
    function naturalWidth() {
        var total = Theme.spacingMd * 2
        for (var i = 0; i < colWidths.length; ++i) total += colWidths[i] + (i > 0 ? Theme.spacingSm : 0)
        return total
    }

    radius: Theme.radiusMd
    color: Theme.surface
    border.color: Theme.border
    clip: true

    Flickable {
        id: hFlick
        anchors.fill: parent
        contentWidth: Math.max(width, root.naturalWidth())
        contentHeight: height
        clip: true
        flickableDirection: Flickable.HorizontalFlick
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.horizontal: ScrollBar { policy: ScrollBar.AsNeeded; height: 8 }

        ColumnLayout {
            width: hFlick.contentWidth
            height: hFlick.height
            spacing: 0

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 34
                color: Theme.surfaceRaised
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Theme.spacingMd
                    anchors.rightMargin: Theme.spacingMd
                    spacing: Theme.spacingSm
                    Repeater {
                        model: root.colTitles
                        delegate: Text {
                            text: modelData
                            color: Theme.primary
                            font.family: Theme.fontFamily; font.pixelSize: Theme.fontXs
                            font.weight: Font.Bold
                            font.capitalization: Font.AllUppercase
                            Layout.preferredWidth: root.colWidths[index]
                        }
                    }
                }
            }
            Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Theme.border }

            ListView {
                id: list
                Layout.fillWidth: true
                Layout.fillHeight: true
                model: root._rows
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar { width: 8 }

                delegate: Rectangle {
                    width: list.width
                    height: 40
                    color: (index % 2 === 0) ? "transparent" : Theme.alpha(Theme.surfaceRaised, 0.5)

                    Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.alpha(Theme.border, 0.6) }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: Theme.spacingMd
                        anchors.rightMargin: Theme.spacingMd
                        spacing: Theme.spacingSm

                        CheckBox {
                            Layout.preferredWidth: root.colWidths[0]
                            checked: modelData.enabled === true
                            onToggled: backend.updateWatchItem(modelData.id, { enabled: checked })
                        }
                        Text {
                            text: modelData.label || ""
                            color: Theme.text; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm
                            font.weight: Font.DemiBold
                            elide: Text.ElideRight
                            Layout.preferredWidth: root.colWidths[1]
                        }
                        Text {
                            text: modelData.address || ""
                            color: Theme.textMuted; font.family: Theme.monoFamily; font.pixelSize: Theme.fontXs
                            Layout.preferredWidth: root.colWidths[2]
                        }
                        Text {
                            text: (modelData.type || "").toUpperCase()
                            color: Theme.textMuted; font.family: Theme.fontFamily; font.pixelSize: Theme.fontXs
                            Layout.preferredWidth: root.colWidths[3]
                        }
                        Text {
                            text: modelData.format || ""
                            color: Theme.textMuted; font.family: Theme.fontFamily; font.pixelSize: Theme.fontXs
                            Layout.preferredWidth: root.colWidths[4]
                        }
                        Text {
                            text: modelData.scale !== undefined ? modelData.scale : ""
                            color: Theme.textMuted; font.family: Theme.fontFamily; font.pixelSize: Theme.fontXs
                            Layout.preferredWidth: root.colWidths[5]
                        }
                        Text {
                            text: modelData.unit || ""
                            color: Theme.textMuted; font.family: Theme.fontFamily; font.pixelSize: Theme.fontXs
                            Layout.preferredWidth: root.colWidths[6]
                        }
                        Text {
                            text: modelData.liveValue || "—"
                            color: modelData.hasValue ? Theme.cyan : Theme.textFaint
                            font.family: Theme.monoFamily; font.pixelSize: Theme.fontSm; font.weight: Font.DemiBold
                            elide: Text.ElideRight
                            Layout.preferredWidth: root.colWidths[7]
                        }
                        Text {
                            text: modelData.minValue || "—"
                            color: Theme.textMuted; font.family: Theme.monoFamily; font.pixelSize: Theme.fontXs
                            Layout.preferredWidth: root.colWidths[8]
                        }
                        Text {
                            text: modelData.maxValue || "—"
                            color: Theme.textMuted; font.family: Theme.monoFamily; font.pixelSize: Theme.fontXs
                            Layout.preferredWidth: root.colWidths[9]
                        }
                        Text {
                            text: modelData.meanValue || "—"
                            color: Theme.textMuted; font.family: Theme.monoFamily; font.pixelSize: Theme.fontXs
                            Layout.preferredWidth: root.colWidths[10]
                        }
                        Rectangle {
                            Layout.preferredWidth: root.colWidths[11]
                            Layout.preferredHeight: 24
                            radius: Theme.radiusSm
                            color: delM.containsMouse ? Theme.alpha(Theme.danger, 0.18) : "transparent"
                            Text { anchors.centerIn: parent; text: "✕"; color: Theme.danger; font.pixelSize: Theme.fontSm }
                            MouseArea {
                                id: delM; anchors.fill: parent; hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: backend.removeWatchItem(modelData.id)
                            }
                        }
                    }
                }
            }

            Text {
                visible: root._rows.length === 0
                Layout.fillWidth: true
                Layout.topMargin: Theme.spacingLg
                horizontalAlignment: Text.AlignHCenter
                text: "İzlenen değişken yok. ELF yükleyip 'Sembol Ekle' veya 'Adres Ekle' ile başlayın."
                color: Theme.textFaint; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm
            }
        }
    }
}
