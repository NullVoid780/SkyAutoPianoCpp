cmake_minimum_required(VERSION 3.25)

set(SKY_VERSION "2.0.1" CACHE STRING "Application version")
set(SKY_UPDATE_REPOSITORY "NullVoid780/SkyAutoPianoCpp" CACHE STRING "GitHub repository the updater reads releases from")

project(SkyAutoPiano VERSION ${SKY_VERSION} LANGUAGES CXX)

if(NOT WIN32 OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "Sky Auto Piano targets Windows x64 only.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

option(SKY_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(SKY_BUILD_TESTS "Build the Catch2 test runner" ON)

add_compile_options(/permissive- /W4 /utf-8 /Zc:__cplusplus /Zc:preprocessor /EHsc)
if(SKY_WARNINGS_AS_ERRORS)
    add_compile_options(/WX)
endif()
add_compile_definitions(UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN _WIN32_WINNT=0x0A00)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Widgets Svg Network)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(miniz CONFIG REQUIRED)

include(cmake/forbidden_qt_modules.cmake)

add_subdirectory(libs/skyinput)
add_subdirectory(src)

if(SKY_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

sky_check_forbidden_qt_modules()
