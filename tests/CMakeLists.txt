cmake_minimum_required(VERSION 3.20)

project(RX-TESTS)

add_executable(test_rx
        test_main.cpp
        observable_test.cpp
        regression_test.cpp
)

target_link_libraries(test_rx PRIVATE gtest rx)

target_compile_options(test_rx
        PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/bigobj>
        $<$<AND:$<CXX_COMPILER_ID:GNU>,$<BOOL:${GNU_BIG_OBJ_FLAG_ENABLE}>>:-Wa,-mbig-obj>
)

set_target_properties(test_rx PROPERTIES FOLDER Rx/Tests)

add_test(NAME test_rx COMMAND test_rx)
