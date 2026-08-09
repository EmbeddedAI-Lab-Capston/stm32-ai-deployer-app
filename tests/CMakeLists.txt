# Builds unit tests for the pure (QObject-event-loop/socket/process free)
# classes only. tests/CMakeLists.txt compiling a class is itself the answer
# to "is this class testable" — see docs/variable_watcher_plan.md Bolum 13.
find_package(Qt6 REQUIRED COMPONENTS Core Test)

set(TEST_SOURCES
    main.cpp

    TestGdbRspCodec.h
    TestGdbRspCodec.cpp
    TestNmSymbolParser.h
    TestNmSymbolParser.cpp
    TestValueCodec.h
    TestValueCodec.cpp
    TestWatchModel.h
    TestWatchModel.cpp
    TestElfTargetMatcher.h
    TestElfTargetMatcher.cpp
    TestWatchPlanBuilder.h
    TestWatchPlanBuilder.cpp
    TestTraceBuffer.h
    TestTraceBuffer.cpp
    TestTraceEventLog.h
    TestTraceEventLog.cpp
    TestTraceRecorderPlayer.h
    TestTraceRecorderPlayer.cpp
    TestWatchProfile.h
    TestWatchProfile.cpp
    TestTimeSeriesRuleEngine.h
    TestTimeSeriesRuleEngine.cpp
    TestWatchPresetMatcher.h
    TestWatchPresetMatcher.cpp

    ${CMAKE_SOURCE_DIR}/src/modules/debug/GdbRspCodec.h
    ${CMAKE_SOURCE_DIR}/src/modules/debug/GdbRspCodec.cpp

    ${CMAKE_SOURCE_DIR}/src/modules/watcher/SymbolModel.h
    ${CMAKE_SOURCE_DIR}/src/modules/watcher/NmSymbolParser.h
    ${CMAKE_SOURCE_DIR}/src/modules/watcher/NmSymbolParser.cpp
    ${CMAKE_SOURCE_DIR}/src/modules/watcher/WatchModel.h
    ${CMAKE_SOURCE_DIR}/src/modules/watcher/ValueCodec.h
    ${CMAKE_SOURCE_DIR}/src/modules/watcher/ValueCodec.cpp
    ${CMAKE_SOURCE_DIR}/src/modules/watcher/ElfTargetMatcher.h
    ${CMAKE_SOURCE_DIR}/src/modules/watcher/ElfTargetMatcher.cpp
    ${CMAKE_SOURCE_DIR}/src/modules/watcher/WatchPlanBuilder.h
    ${CMAKE_SOURCE_DIR}/src/modules/watcher/WatchPlanBuilder.cpp
    ${CMAKE_SOURCE_DIR}/src/modules/watcher/TraceBuffer.h
    ${CMAKE_SOURCE_DIR}/src/modules/watcher/TraceBuffer.cpp
    ${CMAKE_SOURCE_DIR}/src/modules/watcher/TraceEventLog.h
    ${CMAKE_SOURCE_DIR}/src/modules/watcher/TraceEventLog.cpp
    ${CMAKE_SOURCE_DIR}/src/modules/watcher/TraceRecorder.h
    ${CMAKE_SOURCE_DIR}/src/modules/watcher/TraceRecorder.cpp
    ${CMAKE_SOURCE_DIR}/src/modules/watcher/TracePlayer.h
    ${CMAKE_SOURCE_DIR}/src/modules/watcher/TracePlayer.cpp
    ${CMAKE_SOURCE_DIR}/src/modules/watcher/WatchProfile.h
    ${CMAKE_SOURCE_DIR}/src/modules/watcher/WatchProfile.cpp
    ${CMAKE_SOURCE_DIR}/src/modules/watcher/TimeSeriesRuleModel.h
    ${CMAKE_SOURCE_DIR}/src/modules/watcher/TimeSeriesRuleEngine.h
    ${CMAKE_SOURCE_DIR}/src/modules/watcher/TimeSeriesRuleEngine.cpp
    ${CMAKE_SOURCE_DIR}/src/modules/watcher/WatchPresetMatcher.h
    ${CMAKE_SOURCE_DIR}/src/modules/watcher/WatchPresetMatcher.cpp
)

qt_add_executable(STM32AiDeployerTests ${TEST_SOURCES})

target_include_directories(STM32AiDeployerTests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(STM32AiDeployerTests PRIVATE
    Qt6::Core
    Qt6::Test
)

add_test(NAME STM32AiDeployerTests COMMAND STM32AiDeployerTests)
