import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import STM32AiDeployer

// Faz 10.4: browse the active board's SVD catalog and add a register to the
// Variable Watcher's live 200 Hz polling list. The watch engine can already
// read any address — this dialog is purely the naming/browsing layer that
// connects it to the SAME SVD catalog the Register Inspector uses.
//
// Gözlemci ilkesi (CLAUDE.md): a register whose readAction has a side
// effect on read (or is write-only) is NEVER added without the user's
// explicit acknowledgement — polling it at 200 Hz would repeatedly clear a
// status flag / change firmware behaviour, unlike a single Register
// Inspector snapshot read. Same pattern as the ELF-mismatch "Yine de devam
// et" button in WatchLinkStatus.qml.
Popup {
    id: root
    objectName: "watch.registerPickerDialog"
    modal: true
    anchors.centerIn: Overlay.overlay
    width: 760
    height: 560
    padding: Theme.spacingLg
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    readonly property bool _hasBackend: (typeof backend !== "undefined" && backend)
    property var _peripherals: []
    property string _selectedPeripheral: ""
    property var _registers: []
    property var _selectedRegister: null   // one entry from _registers, or null
    property bool _confirmingSideEffect: false

    function refreshPeripherals() {
        if (!root._hasBackend) { _peripherals = []; return }
        backend.prepareRegisters()   // no-op / immediate if the SVD is already cached
        const all = backend.watchPeripheralList()
        const needle = periphSearch.text.trim().toLowerCase()
        _peripherals = needle.length === 0 ? all : all.filter(p => p.toLowerCase().includes(needle))
    }

    function selectPeripheral(name) {
        root._selectedPeripheral = name
        root._selectedRegister = null
        root._confirmingSideEffect = false
        root._registers = root._hasBackend ? backend.watchRegistersOf(name) : []
    }

    function selectRegister(reg) {
        root._selectedRegister = reg
        root._confirmingSideEffect = false
    }

    function addSelected(acknowledge) {
        if (!root._selectedRegister) return
        const id = backend.addWatchRegister(root._selectedPeripheral, root._selectedRegister.name, acknowledge)
        if (id.length > 0) {
            root._confirmingSideEffect = false
            root._selectedRegister = null
        }
        // id === "" with hasReadSideEffect and !acknowledge -> Backend refused
        // via statusMessage; the confirm banner (below) is what the user needs.
    }

    onOpened: {
        periphSearch.text = ""
        root._selectedPeripheral = ""
        root._selectedRegister = null
        root._confirmingSideEffect = false
        refreshPeripherals()
    }

    Overlay.modal: Rectangle { color: Theme.alpha("#000000", 0.55) }
    background: Rectangle { color: Theme.surface; radius: Theme.radiusLg; border.color: Theme.border }

    contentItem: ColumnLayout {
        spacing: Theme.spacingMd

        Text {
            text: "Register Ekle"
            color: Theme.text
            font.family: Theme.fontFamily; font.pixelSize: Theme.fontMd; font.weight: Font.DemiBold
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: Theme.spacingMd

            // ── Left: peripheral list ────────────────────────────────────
            ColumnLayout {
                Layout.preferredWidth: 220
                Layout.fillHeight: true
                spacing: Theme.spacingXs

                TextField {
                    id: periphSearch
                    Layout.fillWidth: true
                    placeholderText: "Peripheral ara…"
                    color: Theme.text
                    font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm
                    background: Rectangle { radius: Theme.radiusSm; color: Theme.surfaceRaised; border.color: Theme.border }
                    leftPadding: Theme.spacingSm; rightPadding: Theme.spacingSm
                    onTextChanged: root.refreshPeripherals()
                }

                ListView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: root._peripherals
                    spacing: 1
                    boundsBehavior: Flickable.StopAtBounds
                    ScrollBar.vertical: ScrollBar { width: 8 }
                    // The full peripheral list is only ~100 rows — keep them
                    // all instantiated (not just the visible viewport) so a
                    // typed search can jump straight to a match without a
                    // scroll pass first.
                    cacheBuffer: 8000

                    delegate: Rectangle {
                        objectName: "watch.registerPicker.peripheral." + modelData
                        width: ListView.view.width
                        height: 30
                        readonly property bool selected: modelData === root._selectedPeripheral
                        color: selected ? Theme.primarySoft : (periphM.containsMouse ? Theme.surfaceHover : "transparent")
                        radius: Theme.radiusSm
                        border.color: selected ? Theme.primaryDim : "transparent"

                        Text {
                            anchors.fill: parent
                            anchors.leftMargin: Theme.spacingSm
                            verticalAlignment: Text.AlignVCenter
                            text: modelData
                            color: Theme.text; font.family: Theme.monoFamily; font.pixelSize: Theme.fontSm
                            elide: Text.ElideRight
                        }
                        MouseArea {
                            id: periphM
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.selectPeripheral(modelData)
                        }
                    }
                }
            }

            Rectangle { Layout.preferredWidth: 1; Layout.fillHeight: true; color: Theme.border }

            // ── Right: registers of the selected peripheral ─────────────
            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: Theme.spacingXs

                Text {
                    visible: root._selectedPeripheral.length === 0
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    text: "Soldan bir peripheral seçin."
                    color: Theme.textFaint; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm
                }

                ListView {
                    visible: root._selectedPeripheral.length > 0
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: root._registers
                    spacing: 1
                    boundsBehavior: Flickable.StopAtBounds
                    ScrollBar.vertical: ScrollBar { width: 8 }
                    cacheBuffer: 8000

                    delegate: Rectangle {
                        objectName: "watch.registerPicker.register." + modelData.name
                        width: ListView.view.width
                        height: 46
                        readonly property bool selected: root._selectedRegister && modelData.name === root._selectedRegister.name
                        readonly property bool clockOff: modelData.clockKnown === true && modelData.clockEnabled === false
                        color: selected ? Theme.primarySoft : (regM.containsMouse ? Theme.surfaceHover : "transparent")
                        radius: Theme.radiusSm
                        border.color: selected ? Theme.primaryDim : "transparent"
                        opacity: clockOff ? 0.55 : 1.0

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: Theme.spacingSm
                            anchors.rightMargin: Theme.spacingSm
                            spacing: Theme.spacingSm

                            Text {
                                visible: modelData.hasReadSideEffect === true
                                text: "⚠"
                                color: Theme.warning
                                font.pixelSize: Theme.fontMd
                                ToolTip.visible: warnHover.hovered
                                ToolTip.delay: 200
                                ToolTip.text: "Okununca yan etkisi var (" + (modelData.readAction || "write-only") + ") — onay gerekir"
                                HoverHandler { id: warnHover }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 0
                                Text {
                                    text: modelData.name + "  " + modelData.addr
                                    color: Theme.text; font.family: Theme.monoFamily; font.pixelSize: Theme.fontSm
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }
                                Text {
                                    text: (clockOff ? "[clock off] " : "") + (modelData.description || "")
                                    color: clockOff ? Theme.danger : Theme.textFaint
                                    font.family: Theme.fontFamily; font.pixelSize: Theme.fontXs
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }
                            }
                        }
                        MouseArea {
                            id: regM
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.selectRegister(modelData)
                        }
                    }
                }

                // ── Read-side-effect confirmation banner ────────────────
                Rectangle {
                    visible: root._confirmingSideEffect
                    Layout.fillWidth: true
                    implicitHeight: confirmCol.implicitHeight + Theme.spacingMd
                    radius: Theme.radiusSm
                    color: Theme.alpha(Theme.danger, 0.12)
                    border.color: Theme.alpha(Theme.danger, 0.4)

                    ColumnLayout {
                        id: confirmCol
                        anchors.fill: parent
                        anchors.margins: Theme.spacingSm
                        spacing: Theme.spacingXs

                        Text {
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            text: "Bu register okununca temizlenir/değişir — sürekli (200 Hz) okumak firmware davranışını değiştirebilir. Yine de eklensin mi?"
                            color: Theme.text
                            font.family: Theme.fontFamily; font.pixelSize: Theme.fontXs
                        }
                        RowLayout {
                            Layout.alignment: Qt.AlignRight
                            AppButton { text: "İptal"; variant: "ghost"; implicitHeight: 28; onClicked: root._confirmingSideEffect = false }
                            AppButton { text: "Yine de Ekle"; variant: "danger"; implicitHeight: 28; onClicked: root.addSelected(true) }
                        }
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Item { Layout.fillWidth: true }
            AppButton {
                objectName: "watch.registerPickerAddButton"
                text: "Ekle"
                variant: "primary"
                enabled: root._selectedRegister !== null && !root._confirmingSideEffect
                onClicked: {
                    if (root._selectedRegister.hasReadSideEffect === true)
                        root._confirmingSideEffect = true
                    else
                        root.addSelected(false)
                }
            }
            AppButton { objectName: "watch.registerPickerCloseButton"; text: "Kapat"; variant: "ghost"; onClicked: root.close() }
        }
    }
}
