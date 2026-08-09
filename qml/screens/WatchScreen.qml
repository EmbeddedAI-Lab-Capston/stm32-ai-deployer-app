import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import STM32AiDeployer

// Değişken İzleyici (Variable Watcher) screen — this phase (Faz 4): table
// only, no plot yet (that's Faz 6). Layout per plan
// docs/variable_watcher_plan.md Bolum 7.7: toolbar top, table fills the
// middle, link/rate status strip at the bottom.
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

        WatchItemTable {
            id: tbl
            Layout.fillWidth: true
            Layout.fillHeight: true
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
