cmake_minimum_required(VERSION 3.18)
project(LightShadingTests LANGUAGES CXX)

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
set(LIGHT_SHADING_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/.." CACHE PATH
    "Production directory containing lightshadingfx.cpp")
get_filename_component(TOONZ_SOURCE_DIR "${LIGHT_SHADING_SOURCE_DIR}/.." ABSOLUTE)

foreach(library tnzcore tnzbase tnzext toonzlib)
    find_library(${library}_LIBRARY NAMES ${library}
                 PATHS "${TOONZ_BUILD_DIR}/${library}"
                 NO_DEFAULT_PATH REQUIRED)
    list(APPEND TOONZ_TEST_LIBRARIES "${${library}_LIBRARY}")
    if(APPLE)
        get_filename_component(library_name "${${library}_LIBRARY}" NAME)
        file(CREATE_LINK "${${library}_LIBRARY}"
             "${CMAKE_CURRENT_BINARY_DIR}/${library_name}" SYMBOLIC)
    endif()
endforeach()

enable_testing()
add_executable(lightshadingfx_test lightshadingfx_test.cpp
    "${LIGHT_SHADING_SOURCE_DIR}/stdfx.cpp" "${LIGHT_SHADING_SOURCE_DIR}/gradients.cpp")
target_include_directories(lightshadingfx_test PRIVATE
    "${LIGHT_SHADING_SOURCE_DIR}" "${TOONZ_SOURCE_DIR}/include")
if(TOONZ_TEST_BIG_ENDIAN)
    target_compile_definitions(lightshadingfx_test PRIVATE TNZ_LITTLE_ENDIAN=0)
else()
    target_compile_definitions(lightshadingfx_test PRIVATE TNZ_LITTLE_ENDIAN=1)
endif()
if(APPLE)
    # These legacy names also select the pixel ABI in OpenToonz's arm64 build.
    target_compile_definitions(lightshadingfx_test PRIVATE
        MACOSX __MACOS__ i386 x64 GL_SILENCE_DEPRECATION)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_compile_definitions(lightshadingfx_test PRIVATE LINUX)
elseif(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
    target_compile_definitions(lightshadingfx_test PRIVATE FREEBSD)
endif()
target_link_libraries(lightshadingfx_test PRIVATE
    ${TOONZ_TEST_LIBRARIES} Qt5::Core Qt5::Gui Qt5::OpenGL)

option(LIGHT_SHADING_SANITIZE "Check undefined arithmetic in the compiled FX and test" ON)
if(LIGHT_SHADING_SANITIZE AND CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(lightshadingfx_test PRIVATE
        -fsanitize=undefined,float-cast-overflow -fno-sanitize-recover=all)
    target_link_options(lightshadingfx_test PRIVATE
        -fsanitize=undefined,float-cast-overflow -fno-sanitize-recover=all)
endif()

set(LIGHT_SHADING_CASES radius-overflow radius-boundary memory hdr no-op active-float
    integer integer-matrix)
option(LIGHT_SHADING_TEST_PADDING "Test the new padding helper; disable for the original PR" ON)
if(LIGHT_SHADING_TEST_PADDING)
    target_compile_definitions(lightshadingfx_test PRIVATE LIGHT_SHADING_TEST_PADDING)
    list(APPEND LIGHT_SHADING_CASES padding)
endif()
foreach(case ${LIGHT_SHADING_CASES})
    add_test(NAME lightshading-${case} COMMAND lightshadingfx_test ${case})
endforeach()
