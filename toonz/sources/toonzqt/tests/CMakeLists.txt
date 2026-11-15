cmake_minimum_required(VERSION 3.18)
project(ParamFieldTests LANGUAGES CXX)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type matching OpenToonz dependencies" FORCE)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
find_package(Qt5 5.14 REQUIRED COMPONENTS Core Gui Widgets)

if(NOT OPENTOONZ_BUILD_DIR)
    message(FATAL_ERROR "Set OPENTOONZ_BUILD_DIR to a built OpenToonz build directory")
endif()

get_filename_component(SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
qt5_wrap_cpp(PARAMFIELD_MOC
    "${SOURCE_DIR}/include/toonzqt/doublefield.h"
    "${SOURCE_DIR}/include/toonzqt/paramfield.h"
)

enable_testing()
add_executable(paramfield_test
    paramfield_test.cpp
    "${SOURCE_DIR}/toonzqt/doublefield.cpp"
    "${SOURCE_DIR}/toonzqt/paramfield.cpp"
    "${SOURCE_DIR}/common/tparam/tdoubleparam.cpp"
    ${PARAMFIELD_MOC}
)
target_include_directories(paramfield_test PRIVATE "${SOURCE_DIR}/include")
include(TestBigEndian)
test_big_endian(BIG_ENDIAN)
if(BIG_ENDIAN)
    target_compile_definitions(paramfield_test PRIVATE TNZ_LITTLE_ENDIAN=0)
else()
    target_compile_definitions(paramfield_test PRIVATE TNZ_LITTLE_ENDIAN=1)
endif()
if(APPLE)
    target_compile_definitions(paramfield_test PRIVATE MACOSX __MACOS__ i386 x64)
elseif(UNIX)
    target_compile_definitions(paramfield_test PRIVATE LINUX)
endif()
foreach(LIBRARY toonzqt toonzlib tnzbase tnzcore)
    find_library(${LIBRARY}_PATH NAMES ${LIBRARY}
        PATHS "${OPENTOONZ_BUILD_DIR}/${LIBRARY}" NO_DEFAULT_PATH REQUIRED)
    target_link_libraries(paramfield_test PRIVATE "${${LIBRARY}_PATH}")
endforeach()
target_link_libraries(paramfield_test PRIVATE Qt5::Core Qt5::Gui Qt5::Widgets)
add_test(NAME paramfield COMMAND paramfield_test)
set_tests_properties(paramfield PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
if(APPLE)
    set(LIBRARY_PATH "")
    foreach(LIBRARY toonzqt toonzlib tnzbase tnzcore tnzext image sound colorfx)
        list(APPEND LIBRARY_PATH "${OPENTOONZ_BUILD_DIR}/${LIBRARY}")
    endforeach()
    list(JOIN LIBRARY_PATH ":" LIBRARY_PATH)
    set_property(TEST paramfield APPEND PROPERTY
        ENVIRONMENT "DYLD_LIBRARY_PATH=${LIBRARY_PATH}")
endif()

qt5_wrap_cpp(FXSETTINGS_MOC "${SOURCE_DIR}/include/toonzqt/fxsettings.h")
find_package(Boost CONFIG REQUIRED)
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/portablestuff")
add_executable(fxsettings_swatch_test
    fxsettings_swatch_test.cpp
    "${SOURCE_DIR}/toonzqt/doublefield.cpp"
    "${SOURCE_DIR}/toonzqt/paramfield.cpp"
    "${SOURCE_DIR}/toonzqt/fxsettings.cpp"
    "${SOURCE_DIR}/common/tparam/tdoubleparam.cpp"
    ${PARAMFIELD_MOC}
    ${FXSETTINGS_MOC}
)
target_include_directories(fxsettings_swatch_test PRIVATE
    "${SOURCE_DIR}/include" "${SOURCE_DIR}/include/toonzqt" "${SOURCE_DIR}/toonzqt")
target_compile_definitions(fxsettings_swatch_test PRIVATE
    $<TARGET_PROPERTY:paramfield_test,COMPILE_DEFINITIONS>
    PR7241_TEST_PROFILES="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/profiles"
    PR7241_TEST_OUTPUT="${CMAKE_CURRENT_BINARY_DIR}")
target_link_libraries(fxsettings_swatch_test PRIVATE
    $<TARGET_PROPERTY:paramfield_test,LINK_LIBRARIES> Boost::headers)
add_test(NAME fxsettings_swatch COMMAND fxsettings_swatch_test)
set_tests_properties(fxsettings_swatch PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;DYLD_LIBRARY_PATH=${LIBRARY_PATH}")
