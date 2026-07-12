# Directory for easier includes
# Anywhere you see include(...) you can check <root>/cmake for that file
set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

#
# Extract version from Makefile
#

file(READ Makefile _MAKEFILE_CONTENTS)

string(REGEX MATCH "KFVER_MAJOR = ([0-9]+)\n" KFVER_MAJOR_MATCH "${_MAKEFILE_CONTENTS}")
if(NOT KFVER_MAJOR_MATCH)
    message(FATAL_ERROR "Cannot extract major (ABI) version from Makefile")
endif()
set(KFVER_MAJOR "${CMAKE_MATCH_1}")

string(REGEX MATCH "KFVER_MINOR = ([0-9]+)\n" KFVER_MINOR_MATCH "${_MAKEFILE_CONTENTS}")
if(NOT KFVER_MINOR_MATCH)
    message(FATAL_ERROR "Cannot extract minor version from Makefile")
endif()
set(KFVER_MINOR "${CMAKE_MATCH_1}")

string(REGEX MATCH "KFVER_PATCH = ([0-9]+)\n" KFVER_PATCH_MATCH "${_MAKEFILE_CONTENTS}")
if(NOT KFVER_PATCH_MATCH)
    message(FATAL_ERROR "Cannot extract patch version from Makefile")
endif()
set(KFVER_PATCH "${CMAKE_MATCH_1}")

set(MAKEFILE_EXTRACTED_VERSION "${KFVER_MAJOR}.${KFVER_MINOR}.${KFVER_PATCH}")

#
# Declare CMake project
#

cmake_minimum_required(VERSION 3.10)
project(kissfft VERSION "${MAKEFILE_EXTRACTED_VERSION}")

#
# CMake configuration options
#

# Principal datatype: double, float (default), int16_t, int32_t, simd

set(KISSFFT_DATATYPE "float" CACHE STRING "Principal datatype of kissfft: double, float (default), int16_t, int32_t, simd")

# Additional options

option(KISSFFT_OPENMP "Build kissfft with OpenMP support" OFF)
option(KISSFFT_PKGCONFIG "Build pkg-config files" ON)
option(KISSFFT_STATIC "Build kissfft as static (ON) or shared library (OFF)" OFF)
option(KISSFFT_TEST "Build kissfft tests" ON)
option(KISSFFT_TOOLS "Build kissfft command-line tools" ON)
option(KISSFFT_USE_ALLOCA "Use alloca instead of malloc" OFF)

#
# Validate datatype
#

if (NOT KISSFFT_DATATYPE MATCHES "^double$" AND
    NOT KISSFFT_DATATYPE MATCHES "^float$" AND
    NOT KISSFFT_DATATYPE MATCHES "^int16_t$" AND
    NOT KISSFFT_DATATYPE MATCHES "^int32_t$" AND
    NOT KISSFFT_DATATYPE MATCHES "^simd$")
    message(FATAL_ERROR "Incorrect value of KISSFFT_DATATYPE! It can be one of: double, float, int16_t, int32_t, simd")
endif()

#
# Print principal datatype
#

message(STATUS "Building KissFFT with datatype: ${KISSFFT_DATATYPE}")
set(KISSFFT_OUTPUT_NAME "kissfft-${KISSFFT_DATATYPE}")

#
# Validate KISSFFT_STATIC
#

if (BUILD_SHARED_LIBS AND KISSFFT_STATIC)
    message(FATAL_ERROR "Conflicting CMake configuration: -DBUILD_SHARED_LIBS=ON and -DKISSFFT_STATIC=ON")
endif()

#
# Enable BUILD_SHARED_LIBS for shared library build before
# kissfft library is declared
#

if (NOT KISSFFT_STATIC)
    set(BUILD_SHARED_LIBS ON)
    message(STATUS "Building shared library")
else()
    message(STATUS "Building static library")
endif()

#
# Detect C compiler and pass appropriate flags
#

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    add_compile_options(-ffast-math -fomit-frame-pointer
        -W -Wall -Wcast-align -Wcast-qual -Wshadow -Wwrite-strings
        "$<$<COMPILE_LANGUAGE:C>:-Wstrict-prototypes;-Wmissing-prototypes;-Wnested-externs;-Wbad-function-cast>")
endif()

#
# Add GNUInstallDirs for GNU infrastructure before target)include_directories
#

if(CMAKE_SYSTEM_NAME MATCHES "^(Linux|kFreeBSD|GNU)$" AND NOT CMAKE_CROSSCOMPILING)
    include(GNUInstallDirs)
endif()

#
# Declare PKGINCLUDEDIR for kissfft include path
#

set(PKGINCLUDEDIR "${CMAKE_INSTALL_INCLUDEDIR}/kissfft")
message(STATUS "PKGINCLUDEDIR is ${PKGINCLUDEDIR}")

#
# Declare kissfft library ( libkissfft.a / libkissfft-${KISSFFT_DATATYPE}.so.${MAKEFILE_EXTRACTED_VERSION} )
#

add_library(kissfft
  kiss_fft.c
  kfc.c
  kiss_fftnd.c
  kiss_fftndr.c
  kiss_fftr.c)

target_include_directories(kissfft PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${PKGINCLUDEDIR}>)

#
# Set compile definitions based on datatype and additional support flags
#

set(KISSFFT_COMPILE_DEFINITIONS)

#
# double / float
#

if(KISSFFT_DATATYPE MATCHES "^float$" OR KISSFFT_DATATYPE MATCHES "^double$")
    list(APPEND KISSFFT_COMPILE_DEFINITIONS kiss_fft_scalar=${KISSFFT_DATATYPE})
else()

    #
    # int16_t
    #

    if(KISSFFT_DATATYPE MATCHES "^int16_t$")
        list(APPEND KISSFFT_COMPILE_DEFINITIONS FIXED_POINT=16)
    else()

        #
        # int32_t
        #

        if(KISSFFT_DATATYPE MATCHES "^int32_t$")
            list(APPEND KISSFFT_COMPILE_DEFINITIONS FIXED_POINT=32)
        else()

            #
            # simd
            #

            if(KISSFFT_DATATYPE MATCHES "^simd$")
                list(APPEND KISSFFT_COMPILE_DEFINITIONS USE_SIMD)
                if (NOT MSVC)
                    target_compile_options(kissfft PRIVATE -msse)
                else()
                    target_compile_options(kissfft PRIVATE "/arch:SSE")
                endif()
            endif()
        endif()
    endif()
endif()

#
# OpenMP support
#

if(KISSFFT_OPENMP)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
        if (NOT MSVC)
            target_compile_options(kissfft PRIVATE -fopenmp)
            if(${CMAKE_VERSION} VERSION_LESS "3.13.0")
                target_link_libraries(kissfft PRIVATE "-fopenmp")
            else()
                target_link_options(kissfft PRIVATE -fopenmp)
            endif()
        else()
            target_compile_options(kissfft PRIVATE "/openmp")
            if(${CMAKE_VERSION} VERSION_LESS "3.13.0")
                target_link_libraries(kissfft PRIVATE "/openmp")
            else()
                target_link_options(kissfft PRIVATE "/openmp")
            endif()
        endif()
        set(KISSFFT_EXPORT_SUFFIX "-openmp")
        set(KISSFFT_OUTPUT_NAME "kissfft-${KISSFFT_DATATYPE}-openmp")
    else()
        message(FATAL_ERROR "Don't know how to enable OpenMP for this compiler")
    endif()
endif()

#
# Shared / static library
#

if(NOT KISSFFT_STATIC)
    list(APPEND KISSFFT_COMPILE_DEFINITIONS KISS_FFT_SHARED)
    set_target_properties(kissfft PROPERTIES
        C_VISIBILITY_PRESET hidden)
    set(KISSFFT_EXPORT_SUFFIX "${KISSFFT_EXPORT_SUFFIX}-shared")
else()
    set(KISSFFT_EXPORT_SUFFIX "${KISSFFT_EXPORT_SUFFIX}-static")
endif()

#
# Alloca support
#

if(KISSFFT_USE_ALLOCA)
    list(APPEND KISSFFT_COMPILE_DEFINITIONS KISS_FFT_USE_ALLOCA)
endif()

# Set library name, version, soversion and aliases

target_compile_definitions(kissfft PUBLIC ${KISSFFT_COMPILE_DEFINITIONS})
set_target_properties(kissfft PROPERTIES
    OUTPUT_NAME "${KISSFFT_OUTPUT_NAME}"
    DEFINE_SYMBOL KISS_FFT_BUILD
    EXPORT_NAME "${KISSFFT_OUTPUT_NAME}"
    VERSION ${PROJECT_VERSION}
    SOVERSION ${KFVER_MAJOR})
add_library(kissfft::kissfft ALIAS kissfft)
add_library(kissfft::kissfft-${KISSFFT_DATATYPE} ALIAS kissfft)

#
# Build with libm (-lm) on Linux and kFreeBSD
#

if(CMAKE_SYSTEM_NAME MATCHES "^(Linux|kFreeBSD|GNU)$" AND NOT CMAKE_CROSSCOMPILING)
    target_link_libraries(kissfft PRIVATE m)
endif()

#
# Define a helper function to define executable file
#

function(add_kissfft_executable NAME)
    add_executable(${NAME} ${ARGN})
    target_link_libraries(${NAME} PRIVATE kissfft::kissfft)

    #
    # Build with libm (-lm) on Linux and kFreeBSD
    #

    if(CMAKE_SYSTEM_NAME MATCHES "^(Linux|kFreeBSD|GNU)$" AND NOT CMAKE_CROSSCOMPILING)
        target_link_libraries(${NAME} PRIVATE m)
    endif()

    if (NOT KISSFFT_OPENMP)
        set_target_properties(${NAME} PROPERTIES
            OUTPUT_NAME "${NAME}-${KISSFFT_DATATYPE}")
    else()
        if (NOT MSVC)
            target_compile_options(${NAME} PRIVATE -fopenmp)
            if(${CMAKE_VERSION} VERSION_LESS "3.13.0")
                target_link_libraries(${NAME} PRIVATE "-fopenmp")
            else()
                target_link_options(${NAME} PRIVATE -fopenmp)
            endif()
        else()
            target_compile_options(${NAME} PRIVATE "/openmp")
            if(${CMAKE_VERSION} VERSION_LESS "3.13.0")
                target_link_libraries(${NAME} PRIVATE "/openmp")
            else()
                target_link_options(${NAME} PRIVATE "/openmp")
            endif()
        endif()
        set_target_properties(${NAME} PROPERTIES
            OUTPUT_NAME "${NAME}-${KISSFFT_DATATYPE}-openmp")
    endif()
endfunction()

#
# Perform installation of kissfft library and development files
#

install(TARGETS kissfft EXPORT kissfft
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")

install(FILES kiss_fft.h
              kissfft.hh
              kiss_fftnd.h
              kiss_fftndr.h
              kiss_fftr.h
        DESTINATION "${PKGINCLUDEDIR}")

set(KISSFFT_INSTALL_CMAKE "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}"
    CACHE FILEPATH "Install destination of kissfft cmake modules")
mark_as_advanced(KISSFFT_INSTALL_CMAKE)

install(EXPORT kissfft DESTINATION "${KISSFFT_INSTALL_CMAKE}"
        NAMESPACE "kissfft::"
        FILE "${PROJECT_NAME}-${KISSFFT_DATATYPE}${KISSFFT_EXPORT_SUFFIX}-targets.cmake")
include(CMakePackageConfigHelpers)
configure_package_config_file(kissfft-config.cmake.in kissfft-config.cmake
    INSTALL_DESTINATION "${KISSFFT_INSTALL_CMAKE}")
write_basic_package_version_file(kissfft-config-version.cmake COMPATIBILITY AnyNewerVersion)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/kissfft-config.cmake" "${CMAKE_CURRENT_BINARY_DIR}/kissfft-config-version.cmake"
        DESTINATION "${KISSFFT_INSTALL_CMAKE}")

set(PKG_KISSFFT_DEFS)
foreach(_def ${KISSFFT_COMPILE_DEFINITIONS})
    set(PKG_KISSFFT_DEFS "${PKG_KISSFFT_DEFS} -D${_def}")
endforeach()
if (KISSFFT_PKGCONFIG)
    include(JoinPaths)
    set(PKGCONFIG_KISSFFT_PKGINCLUDEDIR "\${includedir}/kissfft")
    set(PKGCONFIG_KISSFFT_PREFIX "${CMAKE_INSTALL_PREFIX}")
    set(PKGCONFIG_KISSFFT_VERSION "${kissfft_VERSION}")
    join_paths(PKGCONFIG_KISSFFT_LIBDIR "\${prefix}" "${CMAKE_INSTALL_LIBDIR}")
    join_paths(PKGCONFIG_KISSFFT_INCLUDEDIR "\${prefix}" "${CMAKE_INSTALL_INCLUDEDIR}")
    if(KISSFFT_DATATYPE MATCHES "^simd$")
       list(APPEND KISSFFT_COMPILE_DEFINITIONS USE_SIMD)
       if (NOT MSVC)
           set(PKG_KISSFFT_DEFS "${PKG_KISSFFT_DEFS} -msse")
       else()
           set(PKG_KISSFFT_DEFS "${PKG_KISSFFT_DEFS} /ARCH:SSE")
       endif()
    endif()
    if (NOT KISSFFT_OPENMP)
        configure_file(kissfft.pc.in "kissfft-${KISSFFT_DATATYPE}.pc" @ONLY)
        install(FILES "${CMAKE_CURRENT_BINARY_DIR}/kissfft-${KISSFFT_DATATYPE}.pc"
                DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig")
    else()
        if (NOT MSVC)
            set(PKG_OPENMP "-fopenmp")
            set(PKG_KISSFFT_DEFS "${PKG_KISSFFT_DEFS} -fopenmp")
        else()
            set(PKG_KISSFFT_DEFS "${PKG_KISSFFT_DEFS} /openmp")
            set(PKG_OPENMP "/openmp")
        endif()
        configure_file(kissfft.pc.in "kissfft-${KISSFFT_DATATYPE}-openmp.pc" @ONLY)
        install(FILES "${CMAKE_CURRENT_BINARY_DIR}/kissfft-${KISSFFT_DATATYPE}-openmp.pc"
                DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig")
    endif()
endif()

#
# Build and install tools if requested by user
#

if(KISSFFT_TOOLS)
    add_subdirectory(tools)
endif()

#
# Build and run tests if requested by user
#

if(KISSFFT_TEST)
    enable_testing()
    add_subdirectory(test)
endif()
