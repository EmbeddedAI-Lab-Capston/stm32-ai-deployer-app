import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import STM32AiDeployer

Item {
    id: root

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacingLg
        spacing: Theme.spacingLg

        RowLayout {
            Layout.fillWidth: true
            SectionHeader {
                title: "Kart Benchmark Testleri"
                subtitle: "Modeli kart üzerinde ölç ve raporla"
                Layout.fillWidth: true
            }
            AppButton { text: "Benchmark Başlat"; iconText: "▶" }
        }

        GridLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            columns: root.width > 1100 ? 2 : 1
            columnSpacing: Theme.spacingMd
            rowSpacing: Theme.spacingMd

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: Theme.spacingMd

                Card {
                    title: "Benchmark Ayarları"
                    Layout.fillWidth: true
                    Layout.preferredHeight: 240

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: Theme.spacingMd
                        FormField { label: "Model"; value: "anomaly_cnn.tflite"; readOnly: true }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Theme.spacingMd
                            FormField { label: "Tekrar Sayısı"; value: "20" }
                            FormField { label: "Seed"; value: "42" }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Theme.spacingMd
                            FormField { label: "Min"; value: "0.0" }
                            FormField { label: "Max"; value: "1.0" }
                        }
                        Item { Layout.fillHeight: true }
                    }
                }

                Card {
                    title: "Metrikler"
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: Theme.spacingXs

                        Repeater {
                            model: MockData.benchMetrics
                            delegate: RowLayout {
                                Layout.fillWidth: true
                                Text {
                                    text: modelData[0]
                                    color: Theme.textMuted
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontSm
                                    Layout.preferredWidth: 120
                                }
                                Text {
                                    text: modelData[1]
                                    color: Theme.text
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontSm
                                    font.weight: Font.DemiBold
                                    Layout.fillWidth: true
                                }
                            }
                        }
                        Item { Layout.fillHeight: true }
                    }
                }
            }

            Card {
                title: "Çıktı"
                Layout.fillWidth: true
                Layout.fillHeight: true

                Terminal {
                    anchors.fill: parent
                    lines: MockData.benchTerminal
                }
            }
        }
    }
}
