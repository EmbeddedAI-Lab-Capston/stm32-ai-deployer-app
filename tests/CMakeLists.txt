# Builds unit tests for the pure (QObject-event-loop/socket/process free)
# classes only. tests/CMakeLists.txt compiling a class is itself the answer
# to "is this class testable" — see docs/variable_watcher_plan.md Bolum 13.
find_package(Qt6 REQUIRED COMPONENTS Core Test)

set(TEST_SOURCES
    main.cpp

    TestGdbRspCodec.h
    TestGdbRspCodec.cpp

    ${CMAKE_SOURCE_DIR}/src/modules/debug/GdbRspCodec.h
    ${CMAKE_SOURCE_DIR}/src/modules/debug/GdbRspCodec.cpp
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
