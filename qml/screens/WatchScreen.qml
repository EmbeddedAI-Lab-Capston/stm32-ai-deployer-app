import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import STM32AiDeployer

// Değişken İzleyici (Variable Watcher) screen. Layout per plan
// docs/variable_watcher_plan.md Bolum 7.7/9.1: toolbar top, a resizable
// split of the live plot (Faz 6) over the item table in the middle,
// link/rate status strip at the bottom.
Item {
    id: root

    readonly property bool _hasBackend: (typeof backend !== "undefined" && backend)

    ColumnLayout {
        id: col
        anchors.fill: parent
        anchors.margins: Theme.spacingLg
        spacing: Theme.spacingMd

        SectionHeader {
            id: hdr
            title: "Değişken İzleyici"
            subtitle: "GDB Remote Serial Protocol üzerinden canlı bellek okuma — hedef durdurulmaz, reset atılmaz"
            Layout.fillWidth: true
            Layout.fillHeight: false
        }

        WatchToolbar {
            id: tb
            Layout.fillWidth: true
            Layout.fillHeight: false
            onAddSymbolRequested: symbolPicker.open()
            onAddAddressRequested: addressDialog.open()
        }

        SplitView {
            id: splitView
            Layout.fillWidth: true
            Layout.fillHeight: true
            orientation: Qt.Vertical

            TracePlotView {
                id: plotView
                SplitView.preferredHeight: 320
                SplitView.minimumHeight: 140
            }

            WatchItemTable {
                id: tbl
                SplitView.fillHeight: true
                SplitView.minimumHeight: 120
            }
        }

        WatchLinkStatus {
            id: ls
            Layout.fillWidth: true
            Layout.fillHeight: false
        }
    }

    SymbolPickerDialog { id: symbolPicker }
    WatchItemDialog { id: addressDialog }
}
