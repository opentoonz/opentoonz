cmake_minimum_required(VERSION 3.18)
project(CelBoundaryTests LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
find_package(Qt5 5.14 REQUIRED COMPONENTS Core Gui OpenGL)
if(NOT OPENTOONZ_LIBRARY_DIR)
    message(FATAL_ERROR "Set OPENTOONZ_LIBRARY_DIR to matching OpenToonz libraries")
endif()
get_filename_component(SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
add_executable(celboundary_test celboundary_test.cpp
    "${SOURCE_DIR}/stdfx/celblur.cpp" "${SOURCE_DIR}/stdfx/stdfx.cpp"
    "${SOURCE_DIR}/stdfx/gradients.cpp")
target_include_directories(celboundary_test PRIVATE
    "${SOURCE_DIR}/include" "${SOURCE_DIR}/stdfx")
if(APPLE)
    target_compile_definitions(celboundary_test PRIVATE
        MACOSX __MACOS__ i386 x64 TNZ_LITTLE_ENDIAN=1 GL_SILENCE_DEPRECATION)
elseif(UNIX)
    target_compile_definitions(celboundary_test PRIVATE LINUX TNZ_LITTLE_ENDIAN=1)
endif()
foreach(library tnzcore tnzbase tnzext toonzlib)
    find_library(${library}_PATH NAMES ${library}
        PATHS "${OPENTOONZ_LIBRARY_DIR}" NO_DEFAULT_PATH REQUIRED)
    target_link_libraries(celboundary_test PRIVATE "${${library}_PATH}")
endforeach()
target_link_libraries(celboundary_test PRIVATE Qt5::Core Qt5::Gui Qt5::OpenGL)
enable_testing()
foreach(case tiles hdr canceled missing)
    add_test(NAME celboundary_${case} COMMAND celboundary_test ${case})
    set_tests_properties(celboundary_${case} PROPERTIES
        ENVIRONMENT "DYLD_LIBRARY_PATH=${OPENTOONZ_LIBRARY_DIR};UBSAN_OPTIONS=halt_on_error=1")
endforeach()
