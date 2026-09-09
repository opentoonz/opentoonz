cmake_minimum_required(VERSION 3.16)
project(TrailTests LANGUAGES CXX)

enable_testing()
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
get_filename_component(TOONZ_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/../sources" ABSOLUTE)
get_filename_component(TOONZ_STUFF "${TOONZ_SOURCES}/../../stuff" ABSOLUTE)

add_library(trail_test_headers INTERFACE)
# Keep existing public-header warnings separate from the regression tests.
target_include_directories(trail_test_headers SYSTEM INTERFACE "${TOONZ_SOURCES}/include")
target_include_directories(trail_test_headers INTERFACE "${TOONZ_SOURCES}")
include(TestBigEndian)
test_big_endian(IS_BIG_ENDIAN)
if(IS_BIG_ENDIAN)
    target_compile_definitions(trail_test_headers INTERFACE TNZ_LITTLE_ENDIAN=0)
else()
    target_compile_definitions(trail_test_headers INTERFACE TNZ_LITTLE_ENDIAN=1)
endif()
if(APPLE)
    target_compile_definitions(trail_test_headers INTERFACE MACOSX i386 GL_SILENCE_DEPRECATION)
elseif(UNIX)
    target_compile_definitions(trail_test_headers INTERFACE LINUX)
endif()

function(add_trail_test name)
    add_executable(${name}_test ${name}_test.cpp ${ARGN})
    target_link_libraries(${name}_test PRIVATE trail_test_headers)
    if(MSVC)
        set_source_files_properties(${name}_test.cpp PROPERTIES COMPILE_OPTIONS "/W4;/WX")
    else()
        set_source_files_properties(${name}_test.cpp PROPERTIES COMPILE_OPTIONS "-Wall;-Werror")
    endif()
    add_test(NAME ${name} COMMAND ${name}_test)
    set_tests_properties(${name} PROPERTIES ENVIRONMENT
        "TOONZROOT=${TOONZ_STUFF};TOONZPROFILES=${TOONZ_STUFF}/profiles")
endfunction()

function(link_trail_core name)
    target_link_libraries(${name}_test PRIVATE "${TNZCORE_LIBRARY}" Qt5::Core Qt5::Gui)
    if(APPLE)
        # The standalone macOS library uses a relative install name.
        add_custom_command(TARGET ${name}_test POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E create_symlink "${TNZCORE_LIBRARY}"
                    "$<TARGET_FILE_DIR:${name}_test>/libtnzcore.dylib")
    endif()
endfunction()

add_trail_test(colorfunctions "${TOONZ_SOURCES}/common/tcolor/tcolorfunctions.cpp")
target_compile_definitions(colorfunctions_test PRIVATE TCOLOR_EXPORTS)
add_trail_test(trailcyclestate)

# Core-backed tests use the exact library built from this checkout.
set(TNZCORE_LIBRARY "" CACHE FILEPATH "Built tnzcore library for raster, PLI and GL tests")
if(TNZCORE_LIBRARY)
    find_package(Qt5 REQUIRED COMPONENTS Core Gui OpenGL Multimedia Network)
    add_trail_test(patterntextures)
    link_trail_core(patterntextures)

    set(PLI_TEST_SOURCES
        "${TOONZ_SOURCES}/image/pli/pli_io.cpp"
        "${TOONZ_SOURCES}/image/pli/tiio_pli.cpp"
        "${TOONZ_SOURCES}/image/pli/tags.cpp")
    add_trail_test(pli ${PLI_TEST_SOURCES})
    link_trail_core(pli)

    find_package(PNG REQUIRED)
    add_trail_test(trailrender "${TOONZ_SOURCES}/image/png/tiio_png.cpp" ${PLI_TEST_SOURCES})
    link_trail_core(trailrender)
    target_link_libraries(trailrender_test PRIVATE Qt5::Widgets Qt5::OpenGL PNG::PNG)
    set_tests_properties(trailrender PROPERTIES LABELS "opengl")
endif()
