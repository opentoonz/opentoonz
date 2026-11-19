cmake_minimum_required(VERSION 3.18)
project(ToneHighlightTests LANGUAGES CXX)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Match the OpenToonz library build" FORCE)
endif()
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
find_package(Qt5 5.14 REQUIRED COMPONENTS Core Gui OpenGL)
include(TestBigEndian)
test_big_endian(TOONZ_TEST_BIG_ENDIAN)

set(TOONZ_BUILD_DIR "" CACHE PATH "An existing OpenToonz build containing its shared libraries")
set(TONE_HIGHLIGHT_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/.." CACHE PATH
    "Production directory containing tonehighlightfx.cpp")
get_filename_component(TOONZ_SOURCE_DIR "${TONE_HIGHLIGHT_SOURCE_DIR}/.." ABSOLUTE)

foreach(library tnzcore tnzbase tnzext toonzlib)
    find_library(${library}_LIBRARY NAMES ${library}
                 PATHS "${TOONZ_BUILD_DIR}/${library}" NO_DEFAULT_PATH REQUIRED)
    list(APPEND TOONZ_TEST_LIBRARIES "${${library}_LIBRARY}")
    if(APPLE)
        get_filename_component(library_name "${${library}_LIBRARY}" NAME)
        file(CREATE_LINK "${${library}_LIBRARY}"
             "${CMAKE_CURRENT_BINARY_DIR}/${library_name}" SYMBOLIC)
    endif()
endforeach()

enable_testing()
add_executable(tonehighlightfx_test tonehighlightfx_test.cpp
    "${TONE_HIGHLIGHT_SOURCE_DIR}/stdfx.cpp" "${TONE_HIGHLIGHT_SOURCE_DIR}/gradients.cpp")
add_executable(renderer_failure_test renderer_failure_test.cpp
    "${TONE_HIGHLIGHT_SOURCE_DIR}/stdfx.cpp" "${TONE_HIGHLIGHT_SOURCE_DIR}/gradients.cpp")
option(TONE_HIGHLIGHT_SANITIZE "Check undefined arithmetic in the compiled FX and test" ON)
foreach(target tonehighlightfx_test renderer_failure_test)
    target_include_directories(${target} PRIVATE
        "${TONE_HIGHLIGHT_SOURCE_DIR}" "${TOONZ_SOURCE_DIR}/include")
    if(TOONZ_TEST_BIG_ENDIAN)
        target_compile_definitions(${target} PRIVATE TNZ_LITTLE_ENDIAN=0)
    else()
        target_compile_definitions(${target} PRIVATE TNZ_LITTLE_ENDIAN=1)
    endif()
    if(APPLE)
        # These legacy names also select the pixel ABI in OpenToonz's arm64 build.
        target_compile_definitions(${target} PRIVATE
            MACOSX __MACOS__ i386 x64 GL_SILENCE_DEPRECATION)
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        target_compile_definitions(${target} PRIVATE LINUX)
    elseif(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
        target_compile_definitions(${target} PRIVATE FREEBSD)
    endif()
    target_link_libraries(${target} PRIVATE
        ${TOONZ_TEST_LIBRARIES} Qt5::Core Qt5::Gui Qt5::OpenGL)

    if(TONE_HIGHLIGHT_SANITIZE AND CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(${target} PRIVATE
            -fsanitize=undefined,float-cast-overflow -fno-sanitize-recover=all)
        target_link_options(${target} PRIVATE
            -fsanitize=undefined,float-cast-overflow -fno-sanitize-recover=all)
    endif()
endforeach()

foreach(bpp 32 64 128)
    foreach(case success budget nonfinite)
        add_test(NAME tonehighlight-renderer-${case}-${bpp}
                 COMMAND renderer_failure_test ${bpp} ${case})
        set_tests_properties(tonehighlight-renderer-${case}-${bpp} PROPERTIES TIMEOUT 20)
    endforeach()
endforeach()

foreach(case radius-extreme radius-workspace radius-nonfinite radius-scale memory
             hdr-zero hdr-no-matte no-op active-float tiny-softness integer-matrix
             subpixel-matrix source-missing)
    add_test(NAME tonehighlight-${case} COMMAND tonehighlightfx_test ${case})
endforeach()

option(TONE_HIGHLIGHT_TEST_PADDING "Test checked dimensions; disable for the original PR" ON)
if(TONE_HIGHLIGHT_TEST_PADDING)
    target_compile_definitions(tonehighlightfx_test PRIVATE TONE_HIGHLIGHT_TEST_PADDING)
    add_test(NAME tonehighlight-padding COMMAND tonehighlightfx_test padding)
endif()
