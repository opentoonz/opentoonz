# preferred homebrew's directories.
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

string(TOLOWER ${SUPERLU_LIBRARY} SUPERLU_LIB_CASE_INSENSITIVE)

if (${SUPERLU_LIB_CASE_INSENSITIVE} MATCHES "thirdparty")
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
else()
    set(SUPERLU_NO_THIRDPARTY "true")
    find_path(
        SUPERLU_INCLUDE_DIR
        NAMES
            slu_Cnames.h
        PATH_SUFFIXES
            superlu
    )
    #Get the version of Superlu. We need >= 5.0.0
    if (SUPERLU_INCLUDE_DIR)
        file(STRINGS "${SUPERLU_INCLUDE_DIR}/slu_util.h" SUPERLU_VERSION REGEX "#define SUPERLU_MAJOR_VERSION.+[0-9]+")
        if(SUPERLU_VERSION)
            string(REGEX MATCH "[0-9]+" SUPERLU_VERSION ${SUPERLU_VERSION})
        else()
            set(SUPERLU_VERSION "0")
        endif()
    endif()
endif()


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
