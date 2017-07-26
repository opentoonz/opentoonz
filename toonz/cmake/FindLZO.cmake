
if(WITH_SYSTEM_LZO)
    # depend on CMake's defaults
    set(_header_hints)
    set(_header_suffixes
        lzo
    )
    set(_lib_suffixes)
else()
    set(_header_hints
        ${THIRDPARTY_LIBS_HINTS}
    )
    set(_header_suffixes
        lzo/2.09/include/lzo
        lzo/2.03/include/lzo
    )
    set(_lib_hints
        ${THIRDPARTY_LIBS_HINTS}
    )
    set(_lib_suffixes
        lzo/2.09/lib
        lzo/2.03/lib/LZO_lib
    )
endif()

find_path(
    LZO_INCLUDE_DIR
    NAMES
        lzoconf.h
    HINTS
        ${_header_hints}
    PATH_SUFFIXES
        ${_header_suffixes}
)

# List dynamic library 'so' first
# since static has issues on some systems, see: #866
find_library(
    LZO_LIBRARY
    NAMES
        liblzo2.so
        liblzo2.a
        lzo2_64.lib
    HINTS
        ${_lib_hints}
    PATH_SUFFIXES
        ${_lib_suffixes}
)

message("***** LZO Header path:" ${LZO_INCLUDE_DIR})
message("***** LZO Library path:" ${LZO_LIBRARY})

set(LZO_NAMES ${LZO_NAMES} LZO)

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(LZO
    DEFAULT_MSG LZO_LIBRARY LZO_INCLUDE_DIR)

if(LZO_FOUND)
    set(LZO_LIBRARIES ${LZO_LIBRARY})
endif()

mark_as_advanced(
    LZO_LIBRARY
    LZO_INCLUDE_DIR
)

unset(_header_hints)
unset(_header_suffixes)
unset(_lib_hints)
unset(_lib_suffixes)
