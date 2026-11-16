cmake_minimum_required(VERSION 3.18)
project(SpectrumAliasTests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Match the OpenToonz libraries" FORCE)
endif()
set(OPENTOONZ_LIBRARY_DIR "" CACHE PATH "Matching OpenToonz shared libraries")
if(NOT IS_DIRECTORY "${OPENTOONZ_LIBRARY_DIR}")
    message(FATAL_ERROR "Set OPENTOONZ_LIBRARY_DIR to matching OpenToonz libraries")
endif()
get_filename_component(SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
set(OPENTOONZ_RASTERFX_SOURCE "${SOURCE_DIR}/tnzbase/trasterfx.cpp"
    CACHE FILEPATH "TRasterFx implementation to exercise")
set(OPENTOONZ_MACROFX_SOURCE "${SOURCE_DIR}/common/tfx/tmacrofx.cpp"
    CACHE FILEPATH "TMacroFx implementation to exercise")

find_package(Qt5 5.14 REQUIRED COMPONENTS Core Gui)
add_executable(spectrumalias_test spectrumalias_test.cpp
    "${OPENTOONZ_RASTERFX_SOURCE}" "${OPENTOONZ_MACROFX_SOURCE}"
    "${SOURCE_DIR}/common/tparam/tspectrumparam.cpp"
    "${SOURCE_DIR}/common/tparam/tpixelparam.cpp"
    "${SOURCE_DIR}/common/tcolor/tpixelutils.cpp")
target_include_directories(spectrumalias_test SYSTEM PRIVATE "${SOURCE_DIR}/include")
foreach(LIBRARY tnzbase tnzcore)
    find_library(${LIBRARY}_PATH NAMES ${LIBRARY}
        PATHS "${OPENTOONZ_LIBRARY_DIR}" NO_DEFAULT_PATH REQUIRED)
    target_link_libraries(spectrumalias_test PRIVATE "${${LIBRARY}_PATH}")
endforeach()
target_link_libraries(spectrumalias_test PRIVATE Qt5::Core Qt5::Gui)
if(APPLE)
    target_compile_definitions(spectrumalias_test PRIVATE
        MACOSX __MACOS__ i386 x64 TNZ_LITTLE_ENDIAN=1)
elseif(UNIX)
    target_compile_definitions(spectrumalias_test PRIVATE LINUX x64 TNZ_LITTLE_ENDIAN=1)
endif()
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    set_source_files_properties(spectrumalias_test.cpp PROPERTIES
        COMPILE_OPTIONS "-Wall;-Wextra;-Werror")
endif()
enable_testing()
foreach(CASE IN ITEMS channels positions hdr huge animation legacy locale connected
    macro nested_macro macro_huge legacy_macro legacy_nested_macro)
    add_test(NAME spectrumalias_${CASE} COMMAND spectrumalias_test ${CASE})
    set_tests_properties(spectrumalias_${CASE} PROPERTIES TIMEOUT 60
        ENVIRONMENT "DYLD_LIBRARY_PATH=${OPENTOONZ_LIBRARY_DIR};UBSAN_OPTIONS=halt_on_error=1")
endforeach()
