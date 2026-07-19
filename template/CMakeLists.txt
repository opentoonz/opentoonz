cmake_minimum_required(VERSION 3.16)
project(yz-ui-template VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# ---- Options ----
option(BUILD_TESTS "Build unit tests" ON)

# ---- Qt6 ----
find_package(Qt6 REQUIRED COMPONENTS
    Widgets Core Gui Svg Network Sql LinguistTools)

# ---- FetchContent Dependencies ----
include(cmake/dependencies.cmake)

# ---- Subdirectories ----
add_subdirectory(src)

if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
