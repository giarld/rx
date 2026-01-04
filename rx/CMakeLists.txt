cmake_minimum_required(VERSION 3.20)

project(Rx)

set(TARGET_NAME rx)

file(GLOB_RECURSE RX_CORE_SRC ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp ${CMAKE_CURRENT_SOURCE_DIR}/src/*.c)
file(GLOB_RECURSE RX_CORE_HDR ${CMAKE_CURRENT_SOURCE_DIR}/include/*.h ${CMAKE_CURRENT_SOURCE_DIR}/src/*.h)

add_library(${TARGET_NAME}
        ${RX_CORE_SRC}
        ${RX_CORE_HDR}
)
target_include_directories(${TARGET_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(${TARGET_NAME} PUBLIC gany gx)

set_target_properties(${TARGET_NAME} PROPERTIES FOLDER Rx)

if (BUILD_SHARED_LIBS)
    target_compile_definitions(${TARGET_NAME} PRIVATE BUILD_SHARED_LIBS=1)
endif ()

if (CMAKE_COMPILER_IS_GNUCXX)
    include(CheckCXXCompilerFlag)
    check_cxx_compiler_flag("-Wa,-mbig-obj" GNU_BIG_OBJ_FLAG_ENABLE)
    message(STATUS GNU_BIG_OBJ_FLAG_ENABLE=${GNU_BIG_OBJ_FLAG_ENABLE})
endif ()

target_compile_options(${TARGET_NAME}
        PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/bigobj>
        $<$<AND:$<CXX_COMPILER_ID:GNU>,$<BOOL:${GNU_BIG_OBJ_FLAG_ENABLE}>>:-Wa,-mbig-obj>)
