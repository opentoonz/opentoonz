find_path(LZO_INCLUDE_DIR NAMES lzoconf.h HINTS ${THIRDPARTY_LIBS_HINTS} PATH_SUFFIXES lzo/2.09/include/lzo lzo/2.03/include/lzo)
find_library(LZO_LIBRARY NAMES liblzo2.a lzo2_64.lib liblzo2.so HINTS ${THIRDPARTY_LIBS_HINTS} PATH_SUFFIXES lzo/2.09/lib lzo/2.03/lib/LZO_lib)

message("***** LZO Header path:" ${LZO_INCLUDE_DIR})
message("***** LZO Library path:" ${LZO_LIBRARY})

set(LZO_NAMES ${LZO_NAMES} LZO)

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(LZO DEFAULT_MSG LZO_LIBRARY LZO_INCLUDE_DIR)

if(LZO_FOUND)
    set(LZO_LIBRARIES ${LZO_LIBRARY})
endif()

mark_as_advanced(LZO_LIBRARY LZO_INCLUDE_DIR)
