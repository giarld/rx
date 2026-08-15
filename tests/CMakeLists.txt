cmake_minimum_required(VERSION 3.20)

project(RX-TESTS)

add_executable(test_rx
        test_main.cpp
        test_infrastructure_test.cpp
        observable_creation_test.cpp
        observable_transformation_test.cpp
        observable_filtering_test.cpp
        observable_error_test.cpp
        observable_range_regression_test.cpp
        observable_combination_test.cpp
        observable_callback_test.cpp
        core_lifecycle_test.cpp
        blocking_test.cpp
        observable_lifecycle_test.cpp
        observable_aggregation_test.cpp
        scheduler_test.cpp
        observable_time_test.cpp
)

target_link_libraries(test_rx PRIVATE gtest rx)

target_compile_options(test_rx
        PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/bigobj>
        $<$<AND:$<CXX_COMPILER_ID:GNU>,$<BOOL:${GNU_BIG_OBJ_FLAG_ENABLE}>>:-Wa,-mbig-obj>
)

option(RX_ENABLE_COVERAGE "Instrument RX and tests for Clang/GCC coverage" OFF)
option(RX_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)

if (RX_ENABLE_COVERAGE)
    if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_compile_options(rx PRIVATE --coverage)
        target_compile_options(test_rx PRIVATE --coverage)
        target_link_options(test_rx PRIVATE --coverage)
    elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "GNU")
        target_compile_options(rx PRIVATE -fprofile-instr-generate -fcoverage-mapping)
        target_compile_options(test_rx PRIVATE -fprofile-instr-generate -fcoverage-mapping)
        target_link_options(test_rx PRIVATE -fprofile-instr-generate)
    else ()
        message(FATAL_ERROR "RX_ENABLE_COVERAGE requires GCC or Clang with the GNU command-line frontend")
    endif ()
endif ()

if (RX_ENABLE_SANITIZERS)
    if ((CMAKE_CXX_COMPILER_ID STREQUAL "GNU") OR
        (CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "GNU"))
        target_compile_options(rx PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_compile_options(test_rx PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(test_rx PRIVATE -fsanitize=address,undefined)
    else ()
        message(FATAL_ERROR "RX_ENABLE_SANITIZERS requires GCC or Clang with the GNU command-line frontend")
    endif ()
endif ()

set_target_properties(test_rx PROPERTIES FOLDER Rx/Tests)

include(GoogleTest)
gtest_discover_tests(test_rx
        DISCOVERY_MODE PRE_TEST
        DISCOVERY_TIMEOUT 30
        PROPERTIES
        TIMEOUT 10
        FAIL_REGULAR_EXPRESSION "Object Leak:"
)
