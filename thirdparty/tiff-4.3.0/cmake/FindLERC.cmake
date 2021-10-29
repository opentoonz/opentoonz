# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#[=======================================================================[.rst:
FindLerc
--------

Find the native Lerc includes and library.

IMPORTED Targets
^^^^^^^^^^^^^^^^

This module defines :prop_tgt:`IMPORTED` target ``LERC::LERC``, if
CMath has been found.

Result Variables
^^^^^^^^^^^^^^^^

This module defines the following variables:

::

  LERC_INCLUDE_DIRS   - where to find Lerc_c_api.h, etc.
  LERC_LIBRARIES      - List of libraries when using LERC.
  LERC_FOUND          - True if LERC found.

#]=======================================================================]

# Standard names to search for
set(LERC_NAMES LercLib Lerc)

find_path(LERC_INCLUDE_DIR
          NAMES Lerc_c_api.h
          PATH_SUFFIXES include)

# Allow LERC_LIBRARY to be set manually, as the location of the deflate library
if(NOT LERC_LIBRARY)
  find_library(LERC_LIBRARY_RELEASE
               NAMES ${LERC_NAMES}
               PATH_SUFFIXES lib)

  include(SelectLibraryConfigurations)
  select_library_configurations(LERC)
endif()

unset(LERC_NAMES)

mark_as_advanced(LERC_INCLUDE_DIR)

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(LERC
        REQUIRED_VARS LERC_LIBRARY LERC_INCLUDE_DIR
        VERSION_VAR LERC_VERSION_STRING)

if(LERC_FOUND)
    set(LERC_INCLUDE_DIRS ${LERC_INCLUDE_DIR})

    if(NOT LERC_LIBRARIES)
        set(LERC_LIBRARIES ${LERC_LIBRARY})
    endif()

    if(NOT TARGET LERC::LERC)
        add_library(LERC::LERC UNKNOWN IMPORTED)
        set_target_properties(LERC::LERC PROPERTIES
                INTERFACE_INCLUDE_DIRECTORIES "${LERC_INCLUDE_DIRS}")

        if(LERC_LIBRARY_RELEASE)
            set_property(TARGET LERC::LERC APPEND PROPERTY
                    IMPORTED_CONFIGURATIONS RELEASE)
            set_target_properties(LERC::LERC PROPERTIES
                    IMPORTED_LOCATION_RELEASE "${LERC_LIBRARY_RELEASE}")
        endif()

        if(LERC_LIBRARY_DEBUG)
            set_property(TARGET LERC::LERC APPEND PROPERTY
                    IMPORTED_CONFIGURATIONS DEBUG)
            set_target_properties(LERC::LERC PROPERTIES
                    IMPORTED_LOCATION_DEBUG "${LERC_LIBRARY_DEBUG}")
        endif()

        if(NOT LERC_LIBRARY_RELEASE AND NOT LERC_LIBRARY_DEBUG)
            set_target_properties(LERC::LERC PROPERTIES
                    IMPORTED_LOCATION_RELEASE "${LERC_LIBRARY}")
        endif()
    endif()
endif()
