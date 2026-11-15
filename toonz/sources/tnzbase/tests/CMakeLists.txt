cmake_minimum_required(VERSION 3.16)
project(RenderGridTests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Match the OpenToonz build type" FORCE)
endif()
set(OPENTOONZ_BUILD_DIR "" CACHE PATH "Build directory for this exact source checkout")
if(NOT IS_DIRECTORY "${OPENTOONZ_BUILD_DIR}")
    message(FATAL_ERROR "Set OPENTOONZ_BUILD_DIR to the matching OpenToonz build directory")
endif()

find_package(Qt5 5.14 REQUIRED COMPONENTS Core Gui Widgets)
find_library(TNZBASE_LIBRARY NAMES tnzbase
    PATHS "${OPENTOONZ_BUILD_DIR}/tnzbase" NO_DEFAULT_PATH)
find_library(TNZCORE_LIBRARY NAMES tnzcore
    PATHS "${OPENTOONZ_BUILD_DIR}/tnzcore" NO_DEFAULT_PATH)
if(NOT TNZBASE_LIBRARY OR NOT TNZCORE_LIBRARY)
    message(FATAL_ERROR "Build tnzbase and tnzcore in OPENTOONZ_BUILD_DIR first")
endif()

add_executable(rendergrid_test rendergrid_test.cpp)
# Existing library headers have legacy warnings outside this test's scope.
target_include_directories(rendergrid_test SYSTEM PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../../include")
target_link_libraries(rendergrid_test PRIVATE
    "${TNZBASE_LIBRARY}" "${TNZCORE_LIBRARY}" Qt5::Core Qt5::Gui Qt5::Widgets)
if(APPLE)
    target_compile_definitions(rendergrid_test PRIVATE MACOSX __MACOS__ i386 x64 TNZ_LITTLE_ENDIAN=1)
elseif(UNIX)
    target_compile_definitions(rendergrid_test PRIVATE LINUX x64 TNZ_LITTLE_ENDIAN=1)
endif()
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(rendergrid_test PRIVATE -Wall -Wextra -Werror)
endif()

enable_testing()
add_test(NAME rendergrid COMMAND rendergrid_test)
set_tests_properties(rendergrid PROPERTIES TIMEOUT 60
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;DYLD_LIBRARY_PATH=${OPENTOONZ_BUILD_DIR}/tnzbase:${OPENTOONZ_BUILD_DIR}/tnzcore")
