pragma Singleton

import QtQuick

QtObject {
    id: mock

    // ── Connection / active board ─────────────────────────────────────────
    readonly property bool   connected: true
    readonly property string boardName: "NUCLEO-H723ZG"
    readonly property string boardChip: "STM32H723ZG"
    readonly property string boardSpec: "Cortex-M7 · 550 MHz"
    readonly property string activePort: "COM5"
    readonly property int    activeBaud: 115200

    // ── Dashboard summary cards ───────────────────────────────────────────
    readonly property var summaryCards: [
        { title: "Aktif Kart",   value: "NUCLEO-H723ZG", subtitle: "STM32H723ZG · 550 MHz", icon: "B", accent: "#4F8BFF" },
        { title: "Son Model",    value: "anomaly_cnn",   subtitle: "INT8 · BME280 · 0.94 ms", icon: "M", accent: "#3FD0C9" },
        { title: "RAM Kullanımı",value: "6.44 KiB",      subtitle: "185 KiB boş heap",         icon: "R", accent: "#3FB950" },
        { title: "Doğruluk",     value: "%96",           subtitle: "Son UART inference",       icon: "%", accent: "#D8A23A" }
    ]

    readonly property var pipelineSteps: [
        { label: "Model analizi",     value: 1.0,  text: "Tamamlandı", color: "#3FB950" },
        { label: "C kod üretimi",     value: 1.0,  text: "Tamamlandı", color: "#3FB950" },
        { label: "Template hazırlığı",value: 0.82, text: "F4/H7 hazır", color: "#4F8BFF" },
        { label: "Flash",             value: 0.35, text: "Bekliyor",   color: "#D8A23A" }
    ]

    readonly property var liveMetrics: [
        { label: "Inference", value: 0.18, text: "0.94 ms",  accent: "#4F8BFF" },
        { label: "RAM",       value: 0.32, text: "6.44 KiB", accent: "#3FB950" },
        { label: "Sıcaklık",  value: 0.48, text: "38 °C",    accent: "#D8A23A" },
        { label: "UART",      value: 0.74, text: "115200",   accent: "#3FD0C9" }
    ]

    readonly property var recentRecords: [
        ["16:41", "REAL-LIVE-42", "NUCLEO-H723ZG", "anomaly_cnn", "normal %90",    "Hazır"],
        ["16:32", "BENCH-120",    "NUCLEO-H723ZG", "anomaly_cnn", "0.934 ms",      "Hazır"],
        ["16:18", "SIM-LIVE-12",  "STM32F407",     "har_mlp",     "walking %96",   "Hazır"],
        ["15:59", "PIPELINE",     "STM32N6",       "kws_lstm",    "experimental",  "Deneysel"]
    ]

    // ── Board screen ──────────────────────────────────────────────────────
    readonly property var boardInfo: [
        ["Model",      "STM32H7"],
        ["Flash",      "1024 KB"],
        ["RAM",        "564 KB"],
        ["Hız",        "550 MHz"],
        ["COM",        "COM5"],
        ["Board",      "NUCLEO-H723ZG"],
        ["Device ID",  "0x483"],
        ["Revision",   "Rev V"],
        ["Device",     "STM32H723/733"],
        ["NVM",        "1 MByte"],
        ["CPU",        "Cortex-M7"],
        ["ST-LINK",    "V3 · FW V3J13"],
        ["Voltage",    "3.27 V"]
    ]

    readonly property var boardPresets: [
        { name: "STM32F4", spec: "1024 KB · 192 KB · 168 MHz", target: "MLP INT8" },
        { name: "STM32H7", spec: "2048 KB · 1024 KB · 480 MHz", target: "1D CNN INT8" },
        { name: "STM32N6", spec: "4096 KB · 4096 KB · 800 MHz", target: "LSTM / KWS" }
    ]

    // ── Flash screen ──────────────────────────────────────────────────────
    readonly property var flashTerminal: [
        { text: "$ STM32_Programmer_CLI -c port=SWD", type: "cmd" },
        { text: "ST-LINK SN  : 0036002A3137510...", type: "info" },
        { text: "Board       : NUCLEO-H723ZG", type: "info" },
        { text: "Flash size  : 1 MBytes", type: "info" },
        { text: "Downloading anomaly_cnn.elf ...", type: "info" },
        { text: "Memory Programmed in 2.314s", type: "ok" },
        { text: "Verification OK", type: "ok" },
        { text: "RUNNING Program ... done.", type: "ok" }
    ]

    // ── Monitor screen ────────────────────────────────────────────────────
    readonly property var monitorTerminal: [
        { text: "§{\"t\":\"boot\",\"card\":\"STM32H7\",\"model\":\"anomaly_cnn\"}", type: "cmd" },
        { text: "[boot] STM32H7 · EdgeAI_v1.0 · 115200", type: "info" },
        { text: "§{\"t\":\"inf\",\"inf_us\":940,\"acc_pct\":96,\"label\":\"normal\"}", type: "info" },
        { text: "[inf]  940 us · acc 96% · normal", type: "ok" },
        { text: "§{\"t\":\"sys\",\"uptime_s\":42,\"temp_c\":38}", type: "info" },
        { text: "[sys]  uptime 42s · 38°C · running", type: "info" },
        { text: "§{\"t\":\"inf\",\"inf_us\":951,\"acc_pct\":94,\"label\":\"anomaly\"}", type: "info" },
        { text: "[inf]  951 us · acc 94% · anomaly", type: "warn" }
    ]

    readonly property var monitorMetrics: [
        { title: "Inference",  value: "0.94 ms",     icon: "speed",  accent: "#4F8BFF" },
        { title: "RAM",        value: "6.44 KiB",    icon: "memory", accent: "#3FB950" },
        { title: "Son Tahmin", value: "normal · %96", icon: "target", accent: "#3FD0C9" }
    ]

    // ── Benchmark screen ──────────────────────────────────────────────────
    readonly property var benchMetrics: [
        ["Model",         "anomaly_cnn"],
        ["Inference",     "0.934 ms"],
        ["RAM / Acts",    "6.44 KiB"],
        ["Flash / Wts",   "48.2 KiB"],
        ["MACC",          "1.21 M"],
        ["Durum",         "Tamamlandı"]
    ]

    readonly property var benchTerminal: [
        { text: "$ stedgeai validate -m anomaly_cnn.tflite", type: "cmd" },
        { text: "Importing model ...", type: "info" },
        { text: "Generating C code ...", type: "info" },
        { text: "Running 20 inferences on target ...", type: "info" },
        { text: "avg 0.934 ms · min 0.921 · max 0.948", type: "ok" },
        { text: "Validation report saved.", type: "ok" }
    ]

    // ── Analysis screen ───────────────────────────────────────────────────
    readonly property var analysisRows: [
        ["BENCH-120",   "NUCLEO-H723ZG", "anomaly_cnn", "INT8", "0.934 ms", "6.44 KiB", "%96"],
        ["BENCH-118",   "NUCLEO-H723ZG", "anomaly_cnn", "F32",  "2.118 ms", "18.9 KiB", "%97"],
        ["SIM-LIVE-12", "STM32F407",     "har_mlp",     "INT8", "1.402 ms", "3.07 KiB", "%96"],
        ["REAL-LIVE-9", "STM32F407",     "har_mlp",     "INT8", "1.388 ms", "3.07 KiB", "%95"],
        ["PIPELINE-3",  "STM32N6",       "kws_lstm",    "INT8", "0.612 ms", "12.1 KiB", "%93"]
    ]

    readonly property var analysisBars: [
        { label: "anomaly_cnn", value: 0.45, text: "0.93 ms" },
        { label: "har_mlp",     value: 0.68, text: "1.40 ms" },
        { label: "kws_lstm",    value: 0.30, text: "0.61 ms" },
        { label: "anomaly_f32", value: 1.0,  text: "2.12 ms" }
    ]

    readonly property var analysisSummary: [
        { title: "Toplam Kayıt", value: "127",     accent: "#4F8BFF" },
        { title: "En Hızlı",     value: "0.61 ms", accent: "#3FB950" },
        { title: "Ort. Doğruluk",value: "%95.4",   accent: "#3FD0C9" },
        { title: "Model Sayısı", value: "8",       accent: "#D8A23A" }
    ]

    // ── Tools / settings (Cube yolları) ───────────────────────────────────
    readonly property var toolPaths: [
        { name: "STM32_Programmer_CLI", path: "C:/ST/STM32CubeProgrammer/bin/STM32_Programmer_CLI.exe", found: true },
        { name: "stedgeai (X-CUBE-AI)", path: "C:/Users/.../X-CUBE-AI/10.2.0/Utilities/windows/stedgeai.exe", found: true },
        { name: "arm-none-eabi-gcc",    path: "C:/ST/STM32CubeIDE/.../tools/bin/arm-none-eabi-gcc.exe", found: true },
        { name: "make",                 path: "", found: false }
    ]
}
