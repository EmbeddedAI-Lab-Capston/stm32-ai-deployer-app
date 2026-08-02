cmake_minimum_required(VERSION 3.21)

project(STM32AiDeployer VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    SerialPort
    Charts
    Sql
    Quick
    QuickControls2
    QuickWidgets
    Concurrent
    Network
    Test
)

if(COMMAND qt_policy)
    qt_policy(SET QTP0004 NEW)
endif()

set(PROJECT_SOURCES
    src/main.cpp

    src/bridge/Backend.h
    src/bridge/Backend.cpp

    src/core/AppSettings.h
    src/core/AppSettings.cpp

    src/core/AppState.h
    src/core/AppState.cpp

    src/core/ToolDetector.h
    src/core/ToolDetector.cpp

    src/core/TemplateEngine.h
    src/core/TemplateEngine.cpp

    src/modules/board/BoardPresets.h

    src/modules/board/BoardManager.h
    src/modules/board/BoardManager.cpp

    src/modules/flash/CliRunner.h
    src/modules/flash/CliRunner.cpp

    src/modules/flash/FlashManager.h
    src/modules/flash/FlashManager.cpp

    src/modules/flash/XCubeAIRunner.h
    src/modules/flash/XCubeAIRunner.cpp

    src/modules/flash/PipelineConfig.h

    src/modules/flash/PipelineRunner.h
    src/modules/flash/PipelineRunner.cpp

    src/modules/serial/PacketParser.h
    src/modules/serial/PacketParser.cpp

    src/modules/serial/SerialWorker.h
    src/modules/serial/SerialWorker.cpp

    src/modules/serial/SerialManager.h
    src/modules/serial/SerialManager.cpp

    src/modules/serial/CircularBuffer.h

    src/modules/serial/SerialSimulator.h
    src/modules/serial/SerialSimulator.cpp

    src/modules/analysis/AnalysisManager.h
    src/modules/analysis/AnalysisManager.cpp

    src/modules/registers/SvdModel.h
    src/modules/registers/SvdParser.h
    src/modules/registers/SvdParser.cpp
    src/modules/registers/SvdCatalog.h
    src/modules/registers/SvdCatalog.cpp
    src/modules/registers/RegisterReadModel.h
    src/modules/registers/HexDumpParser.h
    src/modules/registers/HexDumpParser.cpp
    src/modules/registers/ReadPlanBuilder.h
    src/modules/registers/ReadPlanBuilder.cpp
    src/modules/registers/IRegisterReader.h
    src/modules/registers/CliRegisterReader.h
    src/modules/registers/CliRegisterReader.cpp
    src/modules/registers/RegisterSnapshot.h
    src/modules/registers/RegisterDecoder.h
    src/modules/registers/RegisterDecoder.cpp
    src/modules/registers/RegisterDiff.h
    src/modules/registers/SnapshotDiffer.h
    src/modules/registers/SnapshotDiffer.cpp
    src/modules/registers/RegisterRuleModel.h
    src/modules/registers/RuleEngine.h
    src/modules/registers/RuleEngine.cpp
    src/modules/registers/RegisterAdvisor.h
    src/modules/registers/RegisterAdvisor.cpp
    src/modules/registers/RegisterInspector.h
    src/modules/registers/RegisterInspector.cpp

    src/modules/debug/DebugLinkTypes.h
    src/modules/debug/GdbRspCodec.h
    src/modules/debug/GdbRspCodec.cpp
    src/modules/debug/GdbServerProcess.h
    src/modules/debug/GdbServerProcess.cpp
    src/modules/debug/DebugLinkWorker.h
    src/modules/debug/DebugLinkWorker.cpp
    src/modules/debug/DebugLink.h
    src/modules/debug/DebugLink.cpp

    src/modules/simulation/FactorySimulator.h
    src/modules/simulation/FactorySimulator.cpp

    src/ui/BoardTab.h
    src/ui/BoardTab.cpp

    src/ui/FlashTab.h
    src/ui/FlashTab.cpp

    src/ui/MonitorTab.h
    src/ui/MonitorTab.cpp

    src/ui/AnalysisTab.h
    src/ui/AnalysisTab.cpp

    src/ui/BenchmarkTab.h
    src/ui/BenchmarkTab.cpp

    src/ui/Sidebar.h
    src/ui/Sidebar.cpp

    src/ui/SettingsDialog.h
    src/ui/SettingsDialog.cpp

    src/ui/PipelineWizard.h
    src/ui/PipelineWizard.cpp

    src/ui/SplashScreen.h

    resources/app.qrc
    resources/app.rc
)

qt_add_executable(STM32AiDeployer
    WIN32
    ${PROJECT_SOURCES}
)

set_source_files_properties(qml/Theme.qml
    PROPERTIES QT_QML_SINGLETON_TYPE TRUE
)
set_source_files_properties(qml/mock/MockData.qml
    PROPERTIES QT_QML_SINGLETON_TYPE TRUE
)

qt_add_qml_module(STM32AiDeployer
    URI STM32AiDeployer
    VERSION 1.0
    RESOURCE_PREFIX /
    QML_FILES
        qml/Main.qml
        qml/Theme.qml
        qml/mock/MockData.qml
        qml/components/AppButton.qml
        qml/components/Card.qml
        qml/components/DataTable.qml
        qml/components/ComboField.qml
        qml/components/FormField.qml
        qml/components/InfoCard.qml
        qml/components/MetricBar.qml
        qml/components/MetricIcon.qml
        qml/components/SectionHeader.qml
        qml/components/StatusPill.qml
        qml/components/Terminal.qml
        qml/components/TitleBar.qml
        qml/components/TopTabBar.qml
        qml/components/RegisterTree.qml
        qml/components/RegisterDiffView.qml
        qml/dialogs/SettingsDialog.qml
        qml/dialogs/AboutDialog.qml
        qml/dialogs/PipelineWizard.qml
        qml/screens/DashboardScreen.qml
        qml/screens/BoardScreen.qml
        qml/screens/FlashScreen.qml
        qml/screens/MonitorScreen.qml
        qml/screens/BenchmarkScreen.qml
        qml/screens/AnalysisScreen.qml
        qml/screens/RegisterScreen.qml
        qml/factory/FactorySimWindow.qml
        qml/factory/FactoryDashboard.qml
        qml/factory/FactoryMap.qml
        qml/factory/ZoneDetail.qml
        qml/factory/NodeDetail.qml
        qml/factory/components/NodeMarker.qml
        qml/factory/components/ZoneBlock.qml
        qml/factory/components/SensorRow.qml
        qml/factory/components/AlarmFeed.qml
)

target_link_libraries(STM32AiDeployer PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::SerialPort
    Qt6::Charts
    Qt6::Sql
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::QuickWidgets
    Qt6::Concurrent
    Qt6::Network
)

target_include_directories(STM32AiDeployer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

set_target_properties(STM32AiDeployer PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
)

# Copy the templates/ directory next to the executable after every build.
# This makes PipelineRunner find them via applicationDirPath() + "/templates".
add_custom_command(TARGET STM32AiDeployer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/templates"
        "$<TARGET_FILE_DIR:STM32AiDeployer>/templates"
    COMMENT "Copying templates to build directory"
)

# Copy the svd/ directory (CMSIS-SVD register definitions + boards.json) next to
# the executable. SvdCatalog finds them via applicationDirPath() + "/svd"
# (same pattern as templates/). Not embedded in qrc: files are 2-23 MB each.
add_custom_command(TARGET STM32AiDeployer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/svd"
        "$<TARGET_FILE_DIR:STM32AiDeployer>/svd"
    COMMENT "Copying SVD register definitions to build directory"
)

# Unit tests for the pure classes (no Qt event loop / socket / process
# dependency) — separate target, does not slow down or affect the app build.
option(STM32AID_BUILD_TESTS "Build unit tests" ON)
if(STM32AID_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Deploy Qt runtime DLLs + QML plugins next to the executable so the app runs
# standalone (windeployqt is required for the QtQuick/Controls/Dialogs plugins).
find_program(WINDEPLOYQT_EXE windeployqt HINTS "${Qt6_DIR}/../../../bin")
if(WINDEPLOYQT_EXE)
    add_custom_command(TARGET STM32AiDeployer POST_BUILD
        COMMAND "${WINDEPLOYQT_EXE}"
            --qmldir "${CMAKE_SOURCE_DIR}/qml"
            --no-translations
            --no-system-d3d-compiler
            "$<TARGET_FILE:STM32AiDeployer>"
        COMMENT "Deploying Qt runtime and QML plugins"
        VERBATIM
    )
endif()
