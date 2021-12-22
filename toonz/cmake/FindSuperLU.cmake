
if(WITH_SYSTEM_SUPERLU)
    # depend on CMake's defaults
    set(_header_hints)
    set(_header_suffixes
        superlu
        SuperLU
    )
    set(_lib_suffixes)
else()
    # preferred homebrew's directories.
    set(_header_hints
        ${THIRDPARTY_LIBS_HINTS}
    )
    set(_header_suffixes
        superlu43/4.3_1/include/superlu
        superlu/SuperLU_4.1/include
    )
    set(_lib_hints
        ${THIRDPARTY_LIBS_HINTS}
    )
    set(_lib_suffixes
        superlu
    )
endif()

find_path(
    SUPERLU_INCLUDE_DIR
    NAMES
        slu_Cnames.h
    HINTS
        ${_header_hints}
    PATH_SUFFIXES
        ${_header_suffixes}
)

find_library(
    SUPERLU_LIBRARY
    NAMES
        libsuperlu.so
        libsuperlu.a
        libsuperlu_4.1.a
    HINTS
        ${_lib_hints}
    PATH_SUFFIXES
        ${_lib_suffixes}
)

message("***** SuperLU Header path:" ${SUPERLU_INCLUDE_DIR})
message("***** SuperLU Library path:" ${SUPERLU_LIBRARY})

set(SUPERLU_NAMES ${SUPERLU_NAMES} SuperLU)

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(SuperLU
    DEFAULT_MSG SUPERLU_LIBRARY SUPERLU_INCLUDE_DIR)

if(SUPERLU_FOUND)
    set(SUPERLU_LIBRARIES ${SUPERLU_LIBRARY})
endif()

mark_as_advanced(
    SUPERLU_LIBRARY
    SUPERLU_INCLUDE_DIR
)

unset(_header_hints)
unset(_header_suffixes)
unset(_lib_hints)
unset(_lib_suffixes)
