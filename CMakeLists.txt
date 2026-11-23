cmake_minimum_required(VERSION 3.28)
project(protobufexplorer VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    set(CMAKE_CXX_COMPILER_LAUNCHER ${CCACHE_PROGRAM})
endif()

# Before Qt: qt_standard_project_setup() turns AUTOMOC on for every target made after it.
include(cmake/Protobuf.cmake)

find_package(Qt6 6.4 REQUIRED COMPONENTS Core Concurrent Widgets Test)
qt_standard_project_setup()

add_library(protoexplorer_warnings INTERFACE)
target_compile_options(protoexplorer_warnings INTERFACE
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall;-Wextra;-Werror>)

enable_testing()

add_subdirectory(src)
add_subdirectory(tests)
