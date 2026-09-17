import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import STM32AiDeployer

// Top action bar for the Variable Watcher screen: link open/close, ELF load,
// add symbol/address, rate selection, start/stop
// (plan docs/variable_watcher_plan.md Bolum 7.1/7.7).
RowLayout {
    id: root
    spacing: Theme.spacingSm

    signal addSymbolRequested()
    signal addAddressRequested()
    signal addRegisterRequested()

    readonly property bool _hasBackend: (typeof backend !== "undefined" && backend)
    readonly property var _rateOptions: [10, 50, 100, 200, 500, 1000, 0]   // 0 = max
    property int _rateIndex: 3   // 200 Hz default

    function rateLabel(hz) { return hz === 0 ? "Maks" : (hz + " Hz") }

    // NOTE: watchElfPath() is a plain invokable with no NOTIFY, so a binding on
    // it is evaluated once and goes stale. Mirror it and refresh on load.
    property string _elfPath: _hasBackend ? backend.watchElfPath() : ""
    Connections {
        target: root._hasBackend ? backend : null
        function onWatchSymbolsLoaded(count) { root._elfPath = backend.watchElfPath() }
    }

    AppButton {
        objectName: "watch.connectButton"
        text: (root._hasBackend && backend.watchLinkOpen) ? "Bağlantıyı Kapat" : "Bağlan"
        variant: (root._hasBackend && backend.watchLinkOpen) ? "secondary" : "primary"
        enabled: root._hasBackend
        onClicked: {
            if (backend.watchLinkOpen) backend.closeWatchLink()
            else backend.openWatchLink()
        }
    }

    Rectangle { Layout.preferredWidth: 1; Layout.fillHeight: true; color: Theme.border }

    Text {
        text: root._elfPath.length > 0 ? root._elfPath : "ELF yüklenmedi"
        color: Theme.textMuted
        font.family: Theme.monoFamily
        font.pixelSize: Theme.fontXs
        elide: Text.ElideMiddle
        Layout.preferredWidth: 260
    }
    AppButton {
        objectName: "watch.loadElfButton"
        text: "ELF Yükle…"
        variant: "secondary"
        enabled: root._hasBackend
        onClicked: {
            var suggested = backend.suggestedElfPath()
            if (suggested.length > 0)
                elfDialog.currentFile = "file:///" + suggested
            elfDialog.open()
        }
    }

    Rectangle { Layout.preferredWidth: 1; Layout.fillHeight: true; color: Theme.border }

    AppButton {
        objectName: "watch.addSymbolButton"
        text: "Sembol Ekle"
        variant: "secondary"
        enabled: root._hasBackend
        onClicked: root.addSymbolRequested()
    }
    AppButton {
        objectName: "watch.addAddressButton"
        text: "Adres Ekle"
        variant: "secondary"
        enabled: root._hasBackend
        onClicked: root.addAddressRequested()
    }
    AppButton {
        objectName: "watch.addRegisterButton"
        text: "Register Ekle"
        variant: "secondary"
        enabled: root._hasBackend
        onClicked: root.addRegisterRequested()
    }

    Item { Layout.fillWidth: true }

    ComboField {
        id: rateField
        label: ""
        options: root._rateOptions.map(root.rateLabel)
        currentIndex: root._rateIndex
        Layout.preferredWidth: 130
        onActivated: (idx) => root._rateIndex = idx
    }

    AppButton {
        objectName: "watch.startButton"
        text: (root._hasBackend && backend.watchRunning) ? "Durdur" : "Başlat"
        variant: (root._hasBackend && backend.watchRunning) ? "danger" : "primary"
        enabled: root._hasBackend && backend.watchLinkOpen
        onClicked: {
            if (backend.watchRunning) backend.stopWatch()
            else backend.startWatch(root._rateOptions[root._rateIndex])
        }
    }
    AppButton {
        objectName: "watch.clearDataButton"
        text: "Veriyi Temizle"
        variant: "ghost"
        enabled: root._hasBackend
        onClicked: backend.clearWatchData()
    }

    FileDialog {
        id: elfDialog
        title: "İzlenecek ELF dosyasını seçin"
        nameFilters: ["ELF dosyası (*.elf)", "Tüm dosyalar (*)"]
        onAccepted: {
            var p = String(selectedFile).replace("file:///", "")
            if (root._hasBackend) backend.loadWatchElf(p)
        }
    }
}
