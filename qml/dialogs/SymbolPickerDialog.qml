import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import STM32AiDeployer

// Search + add symbols from the loaded ELF into the watch list (plan
// docs/variable_watcher_plan.md Bolum 7.1). Absolute ("A" type) symbols are
// shown but disabled — they are VALUES, not addresses, and can never be
// added (Symbol::addressIsValue).
Popup {
    id: root
    modal: true
    anchors.centerIn: Overlay.overlay
    width: 480
    height: 520
    padding: Theme.spacingLg
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    readonly property bool _hasBackend: (typeof backend !== "undefined" && backend)
    property var _results: []

    function refresh() {
        _results = _hasBackend ? backend.watchSymbols(searchField.text, 200) : []
    }

    onOpened: { searchField.text = ""; refresh() }

    Overlay.modal: Rectangle { color: Theme.alpha("#000000", 0.55) }
    background: Rectangle { color: Theme.surface; radius: Theme.radiusLg; border.color: Theme.border }

    contentItem: ColumnLayout {
        spacing: Theme.spacingMd

        Text {
            text: "Sembol Ekle"
            color: Theme.text
            font.family: Theme.fontFamily; font.pixelSize: Theme.fontMd; font.weight: Font.DemiBold
        }

        TextField {
            id: searchField
            Layout.fillWidth: true
            placeholderText: "Sembol ara…"
            color: Theme.text
            font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm
            background: Rectangle { radius: Theme.radiusSm; color: Theme.surfaceRaised; border.color: Theme.border }
            leftPadding: Theme.spacingSm; rightPadding: Theme.spacingSm
            onTextChanged: root.refresh()
        }

        Text {
            visible: root._results.length === 0
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            text: root._hasBackend && backend.watchElfPath().length === 0
                  ? "Once bir ELF yükleyin." : "Sonuç yok."
            color: Theme.textFaint; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm
        }

        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: root._results.length > 0
            model: root._results
            clip: true
            spacing: 1
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar { width: 8 }

            delegate: Rectangle {
                width: ListView.view.width
                height: 36
                readonly property bool watchable: modelData.watchable === true
                color: rowM.containsMouse && watchable ? Theme.surfaceHover : "transparent"
                radius: Theme.radiusSm
                opacity: watchable ? 1.0 : 0.45

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Theme.spacingSm
                    anchors.rightMargin: Theme.spacingSm
                    spacing: Theme.spacingSm
                    Text {
                        text: modelData.name
                        color: Theme.text; font.family: Theme.monoFamily; font.pixelSize: Theme.fontSm
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                    Text {
                        text: modelData.watchable ? modelData.address : "DEĞER=" + modelData.address
                        color: Theme.textFaint; font.family: Theme.monoFamily; font.pixelSize: Theme.fontXs
                    }
                }
                MouseArea {
                    id: rowM
                    anchors.fill: parent
                    hoverEnabled: true
                    enabled: watchable
                    cursorShape: watchable ? Qt.PointingHandCursor : Qt.ForbiddenCursor
                    onClicked: {
                        backend.addWatchSymbol(modelData.name)
                        root.close()
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Item { Layout.fillWidth: true }
            AppButton { text: "Kapat"; variant: "ghost"; onClicked: root.close() }
        }
    }
}
