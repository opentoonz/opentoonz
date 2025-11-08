function(add_kissfft_test_executable NAME)
    add_kissfft_executable(${NAME} ${ARGN})
    target_include_directories(${NAME} PRIVATE ..)

    add_test(NAME ${NAME} COMMAND ${NAME})
    set_tests_properties(${NAME} PROPERTIES TIMEOUT 3600)
endfunction()

set(KISSFFT_TEST_NUMFFTS 10000)

#
# Add tools-independent fastfilt_* (../tools/fft_*) executable without adding a test
#

add_kissfft_executable(fastfilt ../tools/fftutil.c)
target_include_directories(fastfilt PRIVATE ..)

#
# Add test executables and define tests
#

add_kissfft_test_executable(bm_kiss benchkiss.c pstats.c)
# add_test(NAME benchmar COMMAND ${NAME})
# set_tests_properties(${NAME} PROPERTIES TIMEOUT 3600)

include(FindPkgConfig)
if(KISSFFT_FLOAT)
    set(fftw3_pkg fftw3f)
else()
    set(fftw3_pkg fftw3)
endif()
pkg_check_modules(fftw3 REQUIRED IMPORTED_TARGET ${fftw3_pkg})
add_kissfft_test_executable(bm_fftw benchfftw.c pstats.c)
target_link_libraries(bm_fftw PRIVATE PkgConfig::fftw3)

add_kissfft_test_executable(st twotonetest.c)

add_kissfft_test_executable(tkfc twotonetest.c)
target_compile_definitions(tkfc PRIVATE KFC_TEST)

add_kissfft_test_executable(ffr twotonetest.c)
add_kissfft_test_executable(tr test_real.c)

add_kissfft_test_executable(testcpp testcpp.cc)

if(KISSFFT_DATATYPE MATCHES "^simd$")
    add_kissfft_test_executable(tsimd test_simd.c)
    target_compile_definitions(tsimd PRIVATE USE_SIMD)
    if (NOT MSVC)
        target_compile_options(kissfft PRIVATE -msse)
    else()
        target_compile_options(kissfft PRIVATE "/arch:SSE")
    endif()
endif()

find_package(Python REQUIRED)
add_test(NAME testkiss.py COMMAND "${PYTHON_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/testkiss.py")
list(APPEND TESTKISS_PY_ENV "KISSFFT_DATATYPE=${KISSFFT_DATATYPE}")
list(APPEND TESTKISS_PY_ENV "KISSFFT_OPENMP=${KISSFFT_OPENMP}")
set_tests_properties(testkiss.py PROPERTIES
    TIMEOUT 3600
    ENVIRONMENT "${TESTKISS_PY_ENV}"
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
