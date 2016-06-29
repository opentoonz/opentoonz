# looks for libtiff(4.0.3 modified)
find_path(TIFF_INCLUDE_DIR NAMES tiffio.h HINTS ${SDKROOT} /usr/include/i386-linux-gnu/ /usr/include/x86_64-linux-gnu/ PATH_SUFFIXES tiff-4.0.3/libtiff/)
find_library(TIFF_LIBRARY NAMES libtiff.a HINTS ${SDKROOT}  /usr/include/i386-linux-gnu/ /usr/lib/x86_64-linux-gnu/  PATH_SUFFIXES tiff-4.0.3/libtiff/.libs NO_DEFAULT_PATH)

message("***** libtiff Header path:" ${TIFF_INCLUDE_DIR})
message("***** libtiff Library path:" ${TIFF_LIBRARY})

set(TIFF_NAMES ${TIFF_NAMES} TIFF)

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(TIFF DEFAULT_MSG TIFF_LIBRARY TIFF_INCLUDE_DIR)

if(TIFF_FOUND)
    set(TIFF_LIBRARIES ${TIFF_LIBRARY})
endif()

mark_as_advanced(TIFF_LIBRARY TIFF_INCLUDE_DIR)
