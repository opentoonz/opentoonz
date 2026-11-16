cmake_minimum_required(VERSION 3.16)
project(FxSerializationTests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Match the OpenToonz build type" FORCE)
endif()
set(OPENTOONZ_LIBRARY_DIR "" CACHE PATH "Matching OpenToonz build or combined library directory")
if(NOT IS_DIRECTORY "${OPENTOONZ_LIBRARY_DIR}")
    message(FATAL_ERROR "Set OPENTOONZ_LIBRARY_DIR to libraries built from this source revision")
endif()

find_package(Qt5 5.14 REQUIRED COMPONENTS Core Gui)
find_library(TNZBASE_LIBRARY NAMES tnzbase
    PATHS "${OPENTOONZ_LIBRARY_DIR}" "${OPENTOONZ_LIBRARY_DIR}/tnzbase" NO_DEFAULT_PATH)
find_library(TNZCORE_LIBRARY NAMES tnzcore
    PATHS "${OPENTOONZ_LIBRARY_DIR}" "${OPENTOONZ_LIBRARY_DIR}/tnzcore" NO_DEFAULT_PATH)
if(NOT TNZBASE_LIBRARY OR NOT TNZCORE_LIBRARY)
    message(FATAL_ERROR "Build matching tnzbase and tnzcore libraries first")
endif()

enable_testing()
string(CONCAT TEST_LIBRARY_PATH "${OPENTOONZ_LIBRARY_DIR}:"
    "${OPENTOONZ_LIBRARY_DIR}/tnzbase:${OPENTOONZ_LIBRARY_DIR}/tnzcore")
foreach(TEST_NAME IN ITEMS fxserialization streamroundtrip)
    add_executable(${TEST_NAME}_test ${TEST_NAME}_test.cpp)
    # Existing library headers have legacy warnings outside this test's scope.
    target_include_directories(${TEST_NAME}_test SYSTEM PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/../../include")
    target_link_libraries(${TEST_NAME}_test PRIVATE
        "${TNZBASE_LIBRARY}" "${TNZCORE_LIBRARY}" Qt5::Core Qt5::Gui)
    if(APPLE)
        target_compile_definitions(${TEST_NAME}_test PRIVATE
            MACOSX __MACOS__ i386 x64 TNZ_LITTLE_ENDIAN=1)
    elseif(UNIX)
        target_compile_definitions(${TEST_NAME}_test PRIVATE LINUX x64 TNZ_LITTLE_ENDIAN=1)
    endif()
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(${TEST_NAME}_test PRIVATE -Wall -Wextra -Werror)
    endif()
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME}_test)
    set_tests_properties(${TEST_NAME} PROPERTIES TIMEOUT 60
        ENVIRONMENT "DYLD_LIBRARY_PATH=${TEST_LIBRARY_PATH}")
endforeach()
