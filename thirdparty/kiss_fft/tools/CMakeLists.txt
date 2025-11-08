add_kissfft_executable(fastconvr kiss_fastfir.c)
target_compile_definitions(fastconvr PRIVATE REAL_FASTFIR FAST_FILT_UTIL)

add_kissfft_executable(fastconv kiss_fastfir.c)
target_compile_definitions(fastconv PRIVATE FAST_FILT_UTIL)

add_kissfft_executable(fft fftutil.c)

install(TARGETS fastconv fastconvr fft
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        PUBLIC_HEADER DESTINATION ${PKGINCLUDEDIR})

# psdpng does not build with "simd" datatype
if(NOT KISSFFT_DATATYPE MATCHES "simd")
    include(FindPkgConfig)
    pkg_check_modules(libpng REQUIRED IMPORTED_TARGET libpng)
    add_kissfft_executable(psdpng psdpng.c)
    target_link_libraries(psdpng PRIVATE PkgConfig::libpng)
    install(TARGETS psdpng
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        PUBLIC_HEADER DESTINATION ${PKGINCLUDEDIR})
endif()

#FIXME: dumphdr.c is not available
#add_kissfft_executable(dumphdr dumphdr.c)
