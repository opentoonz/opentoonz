# looks for libtiff(4.0.3 modified)
#if(WIN32 OR APPLE)
set(_search_INC_DIR tiff-4.0.3/libtiff)
set(_search_LIB_DIR tiff-4.0.3/libtiff/.libs)
#endif()

find_path(TIFF_INCLUDE_DIR NAMES tiffio.h HINTS ${SDKROOT} PATH_SUFFIXES ${_search_INC_DIR})
find_library(TIFF_LIBRARY NAMES libtiff.so HINTS ${SDKROOT} PATH_SUFFIXES ${_search_LIB_DIR})

unset(_search_INC_DIR)
unset(_search_LIB_DIR)

message("***** libtiff Header path:" ${TIFF_INCLUDE_DIR})
message("***** libtiff Library path:" ${TIFF_LIBRARY})

set(TIFF_NAMES ${TIFF_NAMES} TIFF)

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(TIFF DEFAULT_MSG TIFF_LIBRARY TIFF_INCLUDE_DIR)

if(TIFF_FOUND)
    set(TIFF_LIBRARIES ${TIFF_LIBRARY})
endif()

mark_as_advanced(TIFF_LIBRARY TIFF_INCLUDE_DIR)
