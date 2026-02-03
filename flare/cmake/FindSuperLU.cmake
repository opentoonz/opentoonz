if(WITH_SYSTEM_SUPERLU)
    # Depend on CMake's default settings
    set(_header_hints)
    set(_header_suffixes
        superlu
        SuperLU
    )
    set(_lib_suffixes)
else()
    # Preferred directories for Homebrew installation
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

# Check for macOS with Homebrew
if(APPLE)
    # Detect Homebrew installation (different paths for Intel and ARM systems)
    if(EXISTS "/opt/homebrew")
        # For Apple Silicon systems (M1/M2)
        set(_header_hints
            /opt/homebrew/include
            /usr/local/include
            ${THIRDPARTY_LIBS_HINTS}
        )
        set(_lib_hints
            /opt/homebrew/lib
            /usr/local/lib
            ${THIRDPARTY_LIBS_HINTS}
        )
        message("***** Using Homebrew for Apple Silicon (M1/M2)")
    elseif(EXISTS "/usr/local")
        # For Intel systems
        set(_header_hints
            /opt/homebrew/include
            /usr/local/include
            ${THIRDPARTY_LIBS_HINTS}
        )
        set(_lib_hints
            /opt/homebrew/lib
            /usr/local/lib
            ${THIRDPARTY_LIBS_HINTS}
        )
        message("***** Using Homebrew for Intel")
    else()
        message(FATAL_ERROR "Homebrew not installed or path not found")
    endif()
endif()

# Find the path to the SuperLU header files
find_path(
    SUPERLU_INCLUDE_DIR
    NAMES
        slu_Cnames.h
    HINTS
        ${_header_hints}
    PATH_SUFFIXES
        ${_header_suffixes}
)

# Find the path to the SuperLU library
find_library(
    SUPERLU_LIBRARY
    NAMES
        libsuperlu.so
        libsuperlu.dylib
        libsuperlu.a
        libsuperlu_4.1.a
    HINTS
        ${_lib_hints}
    PATH_SUFFIXES
        ${_lib_suffixes}
)

# Output the paths for debugging purposes
message("***** SuperLU Header path:" ${SUPERLU_INCLUDE_DIR})
message("***** SuperLU Library path:" ${SUPERLU_LIBRARY})

# Set the SuperLU names
set(SUPERLU_NAMES ${SUPERLU_NAMES} SuperLU)

# Handle standard package search arguments
include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(SuperLU
    DEFAULT_MSG SUPERLU_LIBRARY SUPERLU_INCLUDE_DIR)

# If SuperLU is found, set the libraries
if(SUPERLU_FOUND)
    set(SUPERLU_LIBRARIES ${SUPERLU_LIBRARY})
endif()

# Mark the library and include directories as advanced for the user
mark_as_advanced(
    SUPERLU_LIBRARY
    SUPERLU_INCLUDE_DIR
)

# Unset temporary variables used during the configuration
unset(_header_hints)
unset(_header_suffixes)
unset(_lib_hints)
unset(_lib_suffixes)