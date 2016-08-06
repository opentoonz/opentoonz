# preferred homebrew's directories.
find_path(
    SUPERLU_INCLUDE_DIR
    NAMES
        slu_Cnames.h
    HINTS
        ${THIRDPARTY_LIBS_HINTS}
    PATH_SUFFIXES
        superlu43/4.3_1/include/superlu
        superlu/SuperLU_4.1/include
)

find_library(
    SUPERLU_LIBRARY
    NAMES
        libsuperlu.so
        libsuperlu.a
        libsuperlu_4.1.a
    HINTS
        ${THIRDPARTY_LIBS_HINTS}
    PATH_SUFFIXES
        superlu43/4.3_1/lib
        superlu
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
