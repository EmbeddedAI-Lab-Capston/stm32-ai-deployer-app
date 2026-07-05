import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import STM32AiDeployer

// Register Inspector screen: live SWD register snapshot of the active board.
// Left = peripheral selection, right = expandable decoded register tree.
// Talks only to `backend` (facade rule).
Item {
    id: root

    readonly property bool _hasBackend: (typeof backend !== "undefined" && backend)
    readonly property bool _busy: _hasBackend ? backend.registerBusy : false
    readonly property string _stage: _hasBackend ? backend.registerStage : "idle"
    readonly property string _support: _hasBackend ? backend.registerSupportLevel : "unsupported"
    property int _viewSlot: 0
    property int _snapRev: 0   // bumped when snapshots change, to re-evaluate info bindings

    // Selection is kept here (source of truth) so search-filtering the list
    // never loses ticks. periModel is just the filtered display.
    property var _selSet: ({})
    property var _allPeris: []
    property var _clockMap: ({})

    function refreshPeripheralList() {
        if (!_hasBackend) return
        _allPeris = backend.registerPeripheralList()
        var sel = backend.registerSelectedPeripherals()
        var s = {}
        for (var i = 0; i < sel.length; ++i) s[sel[i]] = true
        _selSet = s
        rebuildPeriModel()
    }
    function rebuildPeriModel() {
        periModel.clear()
        var f = searchField.text.toLowerCase()
        for (var i = 0; i < _allPeris.length; ++i) {
            var n = _allPeris[i]
            if (f.length === 0 || n.toLowerCase().indexOf(f) !== -1)
                periModel.append({ name: n })
        }
    }
    function refreshClockMap() {
        if (!_hasBackend) return
        var t = backend.registerModel
        var m = {}
        for (var i = 0; i < t.length; ++i) m[t[i].name] = t[i].clock
        _clockMap = m
    }
    function selectedNames() {
        var out = []
        for (var k in _selSet) if (_selSet[k]) out.push(k)
        return out
    }
    function setSelection(names) {
        var s = {}
        for (var i = 0; i < names.length; ++i) s[names[i]] = true
        _selSet = s
    }
    function applyPreset(kind) {
        if (kind === "default") { setSelection(backend.registerDefaultPeripherals()); return }
        if (kind === "all")     { setSelection(_allPeris); return }
        if (kind === "none")    { _selSet = ({}); return }
        if (kind === "comm") {
            var re = /USART|UART|SPI|I2C|SAI|LPUART|RCC/
            var out = []
            for (var i = 0; i < _allPeris.length; ++i) if (re.test(_allPeris[i])) out.push(_allPeris[i])
            setSelection(out)
        }
    }
    function selectedCount() {
        var n = 0
        for (var k in _selSet) if (_selSet[k]) n++
        return n
    }

    Component.onCompleted: {
        if (_hasBackend) { backend.prepareRegisters(); refreshPeripheralList() }
    }

    Connections {
        target: root._hasBackend ? backend : null
        function onRegisterCatalogReady(boardName) { root.refreshPeripheralList() }
        function onRegisterModelChanged() { root.refreshClockMap(); root._snapRev++ }
        function onRegisterSnapshotReady(slot) { root._viewSlot = slot; root._snapRev++ }
    }

    ListModel { id: periModel }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacingLg
        spacing: Theme.spacingLg

        // ── Top strip ────────────────────────────────────────────────────
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spacingMd

            SectionHeader {
                title: "Register Inspector"
                subtitle: "Kart çalışırken SWD üzerinden canlı register anlık görüntüsü · reset atılmaz"
                Layout.fillWidth: true
            }
            StatusPill {
                text: root._support === "stable" ? "stable"
                    : root._support === "experimental" ? "experimental" : "desteklenmiyor"
                status: root._support === "stable" ? "ready"
                    : root._support === "experimental" ? "warning" : "error"
            }
            AppButton {
                text: root._busy ? "Alınıyor..." : "Snapshot A"
                iconText: "◉"
                enabled: !root._busy && root._support !== "unsupported"
                onClicked: if (root._hasBackend) backend.takeRegisterSnapshot(0, root.selectedNames())
            }
            AppButton {
                text: "Snapshot B"
                iconText: "◎"
                variant: "secondary"
                enabled: !root._busy && root._support !== "unsupported"
                onClicked: if (root._hasBackend) backend.takeRegisterSnapshot(1, root.selectedNames())
            }
            AppButton {
                text: "Diff"
                variant: "secondary"
                enabled: false   // Faz 5
            }
            AppButton {
                text: "JSON Dışa Aktar"
                variant: "ghost"
                enabled: false   // Faz 5
            }
        }

        // ── Body: selection + tree ───────────────────────────────────────
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: Theme.spacingMd

            // LEFT: peripheral selection
            Card {
                title: "Peripheral Seçimi"
                subtitle: root.selectedCount() + " / " + root._allPeris.length + " seçili · RCC daima okunur"
                Layout.preferredWidth: 320
                Layout.fillHeight: true

                ColumnLayout {
                    anchors.fill: parent
                    spacing: Theme.spacingSm

                    TextField {
                        id: searchField
                        Layout.fillWidth: true
                        placeholderText: "Peripheral ara…"
                        color: Theme.text
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSm
                        leftPadding: Theme.spacingSm; rightPadding: Theme.spacingSm
                        background: Rectangle {
                            radius: Theme.radiusMd; color: Theme.surfaceRaised
                            border.color: searchField.activeFocus ? Theme.primary : Theme.border
                        }
                        onTextChanged: root.rebuildPeriModel()
                    }

                    Flow {
                        Layout.fillWidth: true
                        spacing: 6
                        Repeater {
                            model: [ { k: "default", t: "Varsayılan" }, { k: "all", t: "Tümü" },
                                     { k: "comm", t: "Haberleşme" }, { k: "none", t: "Hiçbiri" } ]
                            delegate: Rectangle {
                                height: 26; width: chipT.implicitWidth + Theme.spacingMd; radius: 13
                                color: chipM.containsMouse ? Theme.surfaceHover : Theme.surfaceRaised
                                border.color: Theme.border
                                Text { id: chipT; anchors.centerIn: parent; text: modelData.t
                                       color: Theme.textMuted; font.family: Theme.fontFamily
                                       font.pixelSize: Theme.fontXs; font.weight: Font.DemiBold }
                                MouseArea { id: chipM; anchors.fill: parent; hoverEnabled: true
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: root.applyPreset(modelData.k) }
                            }
                        }
                    }

                    ListView {
                        id: periList
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        model: periModel
                        clip: true
                        spacing: 1
                        boundsBehavior: Flickable.StopAtBounds
                        ScrollBar.vertical: ScrollBar { width: 9; policy: ScrollBar.AsNeeded }

                        delegate: Rectangle {
                            width: periList.width
                            height: 34
                            radius: Theme.radiusSm
                            color: prowM.containsMouse ? Theme.surfaceHover : "transparent"
                            readonly property bool checked: root._selSet[model.name] === true
                            readonly property string clock: root._clockMap[model.name] !== undefined
                                                            ? root._clockMap[model.name] : ""
                            opacity: clock === "off" ? 0.6 : 1.0

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: Theme.spacingSm
                                anchors.rightMargin: Theme.spacingSm
                                spacing: Theme.spacingSm

                                Rectangle {
                                    Layout.preferredWidth: 16; Layout.preferredHeight: 16
                                    radius: 4
                                    color: parent.parent.checked ? Theme.primary : "transparent"
                                    border.color: parent.parent.checked ? Theme.primary : Theme.borderStrong
                                    border.width: 1.5
                                    Text { anchors.centerIn: parent; visible: parent.parent.parent.checked
                                           text: "✓"; color: "#fff"; font.pixelSize: 11 }
                                }
                                Text { text: model.name; Layout.fillWidth: true
                                       color: parent.parent.checked ? Theme.text : Theme.textMuted
                                       font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm
                                       font.weight: parent.parent.checked ? Font.DemiBold : Font.Normal }
                                // clock badge (after a snapshot)
                                Rectangle {
                                    visible: parent.parent.clock === "on" || parent.parent.clock === "off"
                                    Layout.preferredHeight: 18
                                    Layout.preferredWidth: clkT.implicitWidth + Theme.spacingSm
                                    radius: 9
                                    readonly property bool on: parent.parent.clock === "on"
                                    color: Theme.alpha(on ? Theme.success : Theme.textMuted, 0.12)
                                    Text { id: clkT; anchors.centerIn: parent
                                           text: parent.on ? "on" : "off"
                                           color: parent.on ? Theme.success : Theme.textMuted
                                           font.family: Theme.fontFamily; font.pixelSize: 10; font.weight: Font.DemiBold }
                                }
                            }
                            MouseArea {
                                id: prowM; anchors.fill: parent; hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    var s = root._selSet
                                    s[model.name] = !(s[model.name] === true)
                                    root._selSet = Object.assign({}, s)
                                }
                            }
                        }
                    }
                }
            }

            // RIGHT: register tree
            Card {
                title: "Register Tablosu"
                subtitle: {
                    root._snapRev   // dependency: re-evaluate when snapshots change
                    var info = root._hasBackend ? backend.registerSnapshotInfo(root._viewSlot) : ({})
                    if (!info.valid) return "Henüz anlık görüntü alınmadı"
                    return "Slot " + (root._viewSlot === 0 ? "A" : "B") + " · " + info.takenAt
                           + " · " + info.changedCount + " register reset'ten farklı · "
                           + info.errorCount + " hata"
                }
                Layout.fillWidth: true
                Layout.fillHeight: true

                ColumnLayout {
                    anchors.fill: parent
                    spacing: Theme.spacingSm

                    // A/B view toggle + progress
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.spacingSm
                        Repeater {
                            model: [ { s: 0, t: "A" }, { s: 1, t: "B" } ]
                            delegate: Rectangle {
                                readonly property bool active: root._viewSlot === modelData.s
                                readonly property bool filled: root._snapRev >= 0 && root._hasBackend
                                    && backend.registerSnapshotInfo(modelData.s).valid
                                height: 28; width: 40; radius: Theme.radiusSm
                                color: active ? Theme.primarySoft : Theme.surfaceRaised
                                border.color: active ? Theme.primary : Theme.border
                                opacity: filled ? 1.0 : 0.4
                                Text { anchors.centerIn: parent; text: modelData.t
                                       color: active ? Theme.primary : Theme.textMuted
                                       font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm; font.weight: Font.Bold }
                                MouseArea { anchors.fill: parent; enabled: parent.filled
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: { root._viewSlot = modelData.s
                                                         if (root._hasBackend) backend.setRegisterViewSlot(modelData.s) } }
                            }
                        }
                        Item { Layout.fillWidth: true }
                        BusyIndicator { running: root._busy; visible: root._busy
                                        Layout.preferredWidth: 22; Layout.preferredHeight: 22 }
                        Text {
                            visible: root._busy
                            text: root._stage === "load-svd" ? "SVD yükleniyor…"
                                : root._stage === "read-rcc" ? "RCC okunuyor…"
                                : root._stage === "read-registers" ? "Registerlar okunuyor…"
                                : root._stage === "decode" ? "Çözümleniyor…" : root._stage
                            color: Theme.textMuted; font.family: Theme.fontFamily; font.pixelSize: Theme.fontXs
                        }
                    }

                    RegisterTree {
                        id: tree
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        model: root._hasBackend ? backend.registerModel : []
                    }

                    // empty state
                    Text {
                        visible: tree.model.length === 0 && !root._busy
                        Layout.fillWidth: true
                        horizontalAlignment: Text.AlignHCenter
                        text: root._support === "unsupported"
                              ? "Bu kart için SVD tanımı yok."
                              : "Peripheral seçip 'Snapshot A' ile anlık görüntü al."
                        color: Theme.textFaint; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm
                    }
                }
            }
        }

        // ── Bottom strip: legend ─────────────────────────────────────────
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spacingMd
            Repeater {
                model: [ { c: Theme.cyan, t: "reset'ten farklı" },
                         { c: Theme.purple, t: "yan etki (okunmaz)" },
                         { c: Theme.warning, t: "clock kapalı" },
                         { c: Theme.textMuted, t: "write-only" },
                         { c: Theme.danger, t: "okunamadı" } ]
                delegate: RowLayout {
                    spacing: 6
                    Rectangle { width: 9; height: 9; radius: 2; color: modelData.c }
                    Text { text: modelData.t; color: Theme.textFaint
                           font.family: Theme.fontFamily; font.pixelSize: Theme.fontXs }
                }
            }
            Item { Layout.fillWidth: true }
        }
    }
}
