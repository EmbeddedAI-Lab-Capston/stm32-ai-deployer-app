cmake_minimum_required(VERSION 3.21)

project(STM32AiDeployer VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS
    Core
    Widgets
    SerialPort
    Charts
    Sql
)

set(PROJECT_SOURCES
    src/main.cpp

    src/mainwindow.h
    src/mainwindow.cpp

    src/core/AppSettings.h
    src/core/AppSettings.cpp

    src/modules/board/BoardManager.h
    src/modules/board/BoardManager.cpp

    src/modules/flash/CliRunner.h
    src/modules/flash/CliRunner.cpp

    src/modules/flash/FlashManager.h
    src/modules/flash/FlashManager.cpp

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

    src/ui/BoardTab.h
    src/ui/BoardTab.cpp

    src/ui/FlashTab.h
    src/ui/FlashTab.cpp

    src/ui/MonitorTab.h
    src/ui/MonitorTab.cpp

    src/ui/AnalysisTab.h
    src/ui/AnalysisTab.cpp

    src/ui/SettingsDialog.h
    src/ui/SettingsDialog.cpp

    resources/app.qrc
    resources/app.rc
)

qt_add_executable(STM32AiDeployer
    WIN32
    ${PROJECT_SOURCES}
)

target_link_libraries(STM32AiDeployer PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::SerialPort
    Qt6::Charts
    Qt6::Sql
)

target_include_directories(STM32AiDeployer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

set_target_properties(STM32AiDeployer PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
)
