cmake_minimum_required(VERSION 3.20)
project(RX)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(BUILD_RX_TESTS "Build RX unit tests" ON)

if (NOT CMAKE_SIZEOF_VOID_P)
    if (CMAKE_CL_64)
        set(CMAKE_SIZEOF_VOID_P 8)
    else ()
        set(CMAKE_SIZEOF_VOID_P 4)
    endif ()
endif ()

if (MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /utf-8")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /utf-8")
endif ()

if (NOT T_EXECUTABLE_OUTPUT_PATH)
    set(T_EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR}/bin)
endif ()

if (NOT T_LIBRARY_OUTPUT_PATH)
    set(T_LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR}/bin)
endif ()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${T_EXECUTABLE_OUTPUT_PATH})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${T_EXECUTABLE_OUTPUT_PATH})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO ${T_EXECUTABLE_OUTPUT_PATH})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL ${T_EXECUTABLE_OUTPUT_PATH})

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG ${T_LIBRARY_OUTPUT_PATH})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${T_LIBRARY_OUTPUT_PATH})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELWITHDEBINFO ${T_LIBRARY_OUTPUT_PATH})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_MINSIZEREL ${T_LIBRARY_OUTPUT_PATH})

if (NOT WIN32)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG ${T_LIBRARY_OUTPUT_PATH})
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE ${T_LIBRARY_OUTPUT_PATH})
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO ${T_LIBRARY_OUTPUT_PATH})
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_MINSIZEREL ${T_LIBRARY_OUTPUT_PATH})
endif ()


if (NOT GET_GIT_DEP_FOUND)
    include(cmake/GetGitDep.cmake)
endif ()

if (NOT GIT_DEP_gany_FOUND)
    GetGitDependency(git@github.com:giarld/gany.git gany master)
    add_subdirectory(deps/gany/gany-interface)
endif ()

if (NOT GIT_DEP_gx_FOUND)
    GetGitDependency(git@github.com:giarld/gx.git gx master)
    add_subdirectory(deps/gx)
endif ()

if (BUILD_RX_TESTS AND NOT TARGET gtest)
    GetGitDependency(https://github.com/google/googletest.git googletest main)
    set(INSTALL_GTEST OFF CACHE BOOL "Disable GoogleTest installation" FORCE)
    if (MSVC)
        set(gtest_force_shared_crt ON CACHE BOOL "Use shared MSVC runtime" FORCE)
    endif ()
    add_subdirectory(deps/googletest EXCLUDE_FROM_ALL)
endif ()

add_subdirectory(rx)

if (BUILD_RX_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif ()

if (BUILD_RX_EXAMPLES)
    add_subdirectory(examples)
endif ()
