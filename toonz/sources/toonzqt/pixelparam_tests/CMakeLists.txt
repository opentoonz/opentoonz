cmake_minimum_required(VERSION 3.18)
project(PixelParamTests LANGUAGES CXX)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Matching dependency build type" FORCE)
endif()
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
find_package(Qt5 5.14 REQUIRED COMPONENTS Core Gui Widgets)
find_package(Boost CONFIG REQUIRED)
if(NOT OPENTOONZ_LIBRARY_DIR)
    message(FATAL_ERROR "Set OPENTOONZ_LIBRARY_DIR to matching OpenToonz shared libraries")
endif()
get_filename_component(SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
qt5_wrap_cpp(PARAMFIELD_MOC "${SOURCE_DIR}/include/toonzqt/paramfield.h")
qt5_wrap_cpp(FXSETTINGS_MOC "${SOURCE_DIR}/include/toonzqt/fxsettings.h")
include(TestBigEndian)
test_big_endian(BIG_ENDIAN)
enable_testing()
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/portablestuff")

add_executable(pixelparam_test pixelparam_test.cpp
    "${SOURCE_DIR}/toonzqt/paramfield.cpp" ${PARAMFIELD_MOC})
add_executable(pixelparam_swatch_test pixelparam_swatch_test.cpp
    "${SOURCE_DIR}/toonzqt/paramfield.cpp" "${SOURCE_DIR}/toonzqt/fxsettings.cpp"
    ${PARAMFIELD_MOC} ${FXSETTINGS_MOC})
foreach(TARGET pixelparam_test pixelparam_swatch_test)
    target_include_directories(${TARGET} PRIVATE
        "${SOURCE_DIR}/include" "${SOURCE_DIR}/include/toonzqt" "${SOURCE_DIR}/toonzqt")
    if(BIG_ENDIAN)
        target_compile_definitions(${TARGET} PRIVATE TNZ_LITTLE_ENDIAN=0)
    else()
        target_compile_definitions(${TARGET} PRIVATE TNZ_LITTLE_ENDIAN=1)
    endif()
    if(APPLE)
        target_compile_definitions(${TARGET} PRIVATE MACOSX __MACOS__ i386 x64)
    elseif(UNIX)
        target_compile_definitions(${TARGET} PRIVATE LINUX)
    endif()
    foreach(LIBRARY toonzqt toonzlib tnzbase tnzcore)
        find_library(${LIBRARY}_PATH NAMES ${LIBRARY}
            PATHS "${OPENTOONZ_LIBRARY_DIR}" NO_DEFAULT_PATH REQUIRED)
        target_link_libraries(${TARGET} PRIVATE "${${LIBRARY}_PATH}")
    endforeach()
    target_link_libraries(${TARGET} PRIVATE Qt5::Core Qt5::Gui Qt5::Widgets Boost::headers)
    add_test(NAME ${TARGET} COMMAND ${TARGET})
    set_tests_properties(${TARGET} PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;UBSAN_OPTIONS=halt_on_error=1")
    if(APPLE)
        set_property(TEST ${TARGET} APPEND PROPERTY
            ENVIRONMENT "DYLD_LIBRARY_PATH=${OPENTOONZ_LIBRARY_DIR}")
    endif()
endforeach()
target_compile_definitions(pixelparam_swatch_test PRIVATE
    PR7243_TEST_PROFILES="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/profiles"
    PR7243_TEST_OUTPUT="${CMAKE_CURRENT_BINARY_DIR}")
