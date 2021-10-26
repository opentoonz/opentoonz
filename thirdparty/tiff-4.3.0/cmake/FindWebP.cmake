# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#[=======================================================================[.rst:
FindWebP
--------

Find the native WebP includes and library.

IMPORTED Targets
^^^^^^^^^^^^^^^^

This module defines :prop_tgt:`IMPORTED` target ``WebP::WebP``, if
WebP has been found.

Result Variables
^^^^^^^^^^^^^^^^

This module defines the following variables:

::

  WebP_INCLUDE_DIRS   - where to find webp/*.h, etc.
  WebP_LIBRARIES      - List of libraries when using webp.
  WebP_FOUND          - True if webp found.

  Debug and Release variants are found separately.
#]=======================================================================]

# Standard names to search for
set(WebP_NAMES webp)
set(WebP_NAMES_DEBUG webpd)

find_path(WebP_INCLUDE_DIR
          NAMES webp/decode.h
          PATH_SUFFIXES include)

# Allow WebP_LIBRARY to be set manually, as the location of the webp library
if(NOT WebP_LIBRARY)
  find_library(WebP_LIBRARY_RELEASE
               NAMES ${WebP_NAMES}
               PATH_SUFFIXES lib)
  find_library(WebP_LIBRARY_DEBUG
               NAMES ${WebP_NAMES_DEBUG}
               PATH_SUFFIXES lib)

  include(SelectLibraryConfigurations)
  select_library_configurations(WebP)
endif()

unset(WebP_NAMES)
unset(WebP_NAMES_DEBUG)

mark_as_advanced(WebP_INCLUDE_DIR)

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(WebP
        REQUIRED_VARS WebP_LIBRARY WebP_INCLUDE_DIR)

if(WebP_FOUND)
    set(WebP_INCLUDE_DIRS ${WebP_INCLUDE_DIR})

    if(NOT WebP_LIBRARIES)
        set(WebP_LIBRARIES ${WebP_LIBRARY})
    endif()

    if(NOT TARGET WebP::WebP)
        add_library(WebP::WebP UNKNOWN IMPORTED)
        set_target_properties(WebP::WebP PROPERTIES
                INTERFACE_INCLUDE_DIRECTORIES "${WebP_INCLUDE_DIRS}")

        if(WebP_LIBRARY_RELEASE)
            set_property(TARGET WebP::WebP APPEND PROPERTY
                    IMPORTED_CONFIGURATIONS RELEASE)
            set_target_properties(WebP::WebP PROPERTIES
                    IMPORTED_LOCATION_RELEASE "${WebP_LIBRARY_RELEASE}")
        endif()

        if(WebP_LIBRARY_DEBUG)
            set_property(TARGET WebP::WebP APPEND PROPERTY
                    IMPORTED_CONFIGURATIONS DEBUG)
            set_target_properties(WebP::WebP PROPERTIES
                    IMPORTED_LOCATION_DEBUG "${WebP_LIBRARY_DEBUG}")
        endif()

        if(NOT WebP_LIBRARY_RELEASE AND NOT WebP_LIBRARY_DEBUG)
            set_target_properties(WebP::WebP PROPERTIES
                    IMPORTED_LOCATION_RELEASE "${WebP_LIBRARY}")
        endif()
    endif()
endif()
