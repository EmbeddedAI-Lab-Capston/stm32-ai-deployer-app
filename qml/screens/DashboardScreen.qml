import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import STM32AiDeployer

Item {
    id: root
    property bool compact: width < 1100
    signal openPipelineRequested()
    signal openLogsRequested()

    function recentRows() {
        if (typeof backend === "undefined" || !backend) return []
        var records = backend.recentAnalysisRecords(5)
        var out = []
        for (var i = 0; i < records.length; ++i)
            out.push(records[i].cells || records[i])
        return out
    }

    function pipelineBars() {
        var progress = (typeof backend !== "undefined" && backend) ? backend.pipelineProgress : 0
        var flashProgress = (typeof backend !== "undefined" && backend) ? backend.flashProgress : 0
        return [
            { label: "Pipeline", value: progress / 100, text: progress + "%", color: Theme.primary },
            { label: "Flash", value: flashProgress / 100, text: flashProgress + "%", color: Theme.success }
        ]
    }

    function liveBars() {
        if (typeof appState === "undefined" || !appState) return []
        return [
            { label: "Inference", value: appState.lastInfMs > 0 ? Math.min(appState.lastInfMs / 100, 1) : 0, text: appState.lastInfMs > 0 ? (appState.lastInfMs.toFixed(2) + " ms") : "-", accent: Theme.primary },
            { label: "RAM", value: appState.lastRamKb > 0 ? Math.min(appState.lastRamKb / 1024, 1) : 0, text: appState.lastRamKb > 0 ? (appState.lastRamKb.toFixed(1) + " KB") : "-", accent: Theme.success },
            { label: "Free RAM", value: appState.lastFreeRamKb > 0 ? Math.min(appState.lastFreeRamKb / 1024, 1) : 0, text: appState.lastFreeRamKb > 0 ? (appState.lastFreeRamKb.toFixed(1) + " KB") : "-", accent: Theme.cyan }
        ]
    }

    readonly property var _cards: {
        if (typeof appState !== "undefined" && appState && appState.boardName.length > 0) {
            return [
                { title: "Aktif Kart", value: appState.boardName,
                  subtitle: appState.boardSpec.length > 0 ? appState.boardSpec : "—",
                  icon: "B", accent: Theme.primary },
                { title: "Son Model", value: appState.lastModel.length > 0 ? appState.lastModel : "—",
                  subtitle: appState.connected ? "UART bağlı" : "Bağlantı yok",
                  icon: "M", accent: Theme.cyan },
                { title: "Inference", value: appState.lastInfMs > 0 ? (appState.lastInfMs.toFixed(2) + " ms") : "—",
                  subtitle: "Son UART ölçümü", icon: "T", accent: Theme.success },
                { title: "Doğruluk", value: appState.lastAcc > 0 ? ("%" + appState.lastAcc) : "—",
                  subtitle: "Son inference sonucu", icon: "%", accent: Theme.warning }
            ]
        }
        return MockData.summaryCards
    }

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth
        clip: true
        ScrollBar.vertical.policy: ScrollBar.AsNeeded

        ColumnLayout {
            width: root.width
            spacing: Theme.spacingLg

            // top padding
            Item { Layout.fillWidth: true; Layout.preferredHeight: Theme.spacingMd }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.leftMargin: Theme.spacingLg
                Layout.rightMargin: Theme.spacingLg
                spacing: Theme.spacingLg

                // ── Header ────────────────────────────────────────────────
                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.spacingMd

                    SectionHeader {
                        title: "Genel Bakış"
                        subtitle: "Gömülü AI dağıtımı, canlı UART izleme ve analiz"
                        Layout.fillWidth: true
                    }

                    StatusPill {
                        text: (typeof appState !== "undefined" && appState && appState.connected) ? "UART bagli" : "Bagli degil"
                        status: (typeof appState !== "undefined" && appState && appState.connected) ? "connected" : "offline"
                    }
                    StatusPill {
                        visible: !root.compact
                        text: (typeof backend !== "undefined" && backend) ? backend.pipelineStage : "Pipeline hazir"
                        status: (typeof backend !== "undefined" && backend && backend.pipelineBusy) ? "warning" : "ready"
                    }
                    AppButton { text: "Pipeline"; iconText: "+"; onClicked: root.openPipelineRequested() }
                }

                // ── Summary cards ─────────────────────────────────────────
                GridLayout {
                    Layout.fillWidth: true
                    columns: root.width > 1280 ? 4 : (root.width > 720 ? 2 : 1)
                    columnSpacing: Theme.spacingMd
                    rowSpacing: Theme.spacingMd

                    Repeater {
                        model: root._cards
                        delegate: InfoCard {
                            title: modelData.title
                            value: modelData.value
                            subtitle: modelData.subtitle
                            iconText: modelData.icon
                            accent: modelData.accent
                        }
                    }
                }

                // ── Pipeline + live ───────────────────────────────────────
                GridLayout {
                    Layout.fillWidth: true
                    columns: root.width > 1180 ? 2 : 1
                    columnSpacing: Theme.spacingMd
                    rowSpacing: Theme.spacingMd

                    Card {
                        title: "Pipeline Durumu"
                        subtitle: "Son derleme akışı"
                        Layout.fillWidth: true
                        Layout.preferredHeight: 300

                        ColumnLayout {
                            anchors.fill: parent
                            spacing: Theme.spacingMd

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: Theme.spacingSm
                                StatusPill { text: "X-CUBE-AI hazır"; status: "ready" }
                                StatusPill { text: "Flash bekliyor"; status: "warning" }
                                Item { Layout.fillWidth: true }
                            }

                            Repeater {
                                model: root.pipelineBars()
                                delegate: MetricBar {
                                    label: modelData.label
                                    value: modelData.value
                                    valueText: modelData.text
                                    accent: modelData.color
                                }
                            }

                            Item { Layout.fillHeight: true }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: Theme.spacingSm
                                AppButton { text: "Model Derle"; iconText: ">"; onClicked: root.openPipelineRequested() }
                                AppButton { text: "Loglari Ac"; iconText: "="; variant: "secondary"; onClicked: root.openLogsRequested() }
                            }
                        }
                    }

                    Card {
                        title: "Canlı Sistem"
                        subtitle: "Gerçek zamanlı metrikler"
                        Layout.fillWidth: true
                        Layout.preferredHeight: 300

                        ColumnLayout {
                            anchors.fill: parent
                            spacing: Theme.spacingMd

                            Repeater {
                                model: root.liveBars()
                                delegate: MetricBar {
                                    label: modelData.label
                                    value: modelData.value
                                    valueText: modelData.text
                                    accent: modelData.accent
                                }
                            }

                            Rectangle {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                radius: Theme.radiusMd
                                color: Theme.surfaceRaised
                                border.color: Theme.border

                                Text {
                                    anchors.centerIn: parent
                                    width: parent.width - Theme.spacingLg
                                    text: {
                                        if (typeof appState === "undefined" || !appState) return "Canli veri bekleniyor"
                                        var label = appState.lastLabel.length > 0 ? appState.lastLabel : appState.lastModel
                                        var temp = appState.lastTempC > 0 ? ("   temp: " + appState.lastTempC.toFixed(1) + " C") : ""
                                        return "label: " + (label.length > 0 ? label : "-") + "   confidence: %" + appState.lastAcc + temp
                                    }
                                    color: Theme.textMuted
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontSm
                                    horizontalAlignment: Text.AlignHCenter
                                    elide: Text.ElideRight
                                }
                            }
                        }
                    }
                }

                // ── Recent records ────────────────────────────────────────
                Card {
                    title: "Son Analiz Kayıtları"
                    subtitle: "Gerçek sensör, benchmark ve simülasyon sonuçları"
                    Layout.fillWidth: true
                    Layout.preferredHeight: 320

                    DataTable {
                        anchors.fill: parent
                        columns: [
                            { title: "Saat", width: 60 },
                            { title: "Tür", width: 130 },
                            { title: "Kart", width: 0 },
                            { title: "Model", width: 0 },
                            { title: "Sonuç", width: 0 },
                            { title: "Durum", width: 110 }
                        ]
                        rows: root.recentRows()
                    }
                }

                Item { Layout.fillWidth: true; Layout.preferredHeight: Theme.spacingLg }
            }
        }
    }
}
