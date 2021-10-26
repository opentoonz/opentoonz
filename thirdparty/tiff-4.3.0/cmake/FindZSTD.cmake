# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#[=======================================================================[.rst:
FindZSTD
--------

Find the native ZSTD includes and library.

IMPORTED Targets
^^^^^^^^^^^^^^^^

This module defines :prop_tgt:`IMPORTED` target ``ZSTD::ZSTD``, if
ZSTD has been found.

Result Variables
^^^^^^^^^^^^^^^^

This module defines the following variables:

::

  ZSTD_INCLUDE_DIRS   - where to find zstd.h, etc.
  ZSTD_LIBRARIES      - List of libraries when using zstd.
  ZSTD_FOUND          - True if zstd found.

::

  ZSTD_VERSION_STRING - The version of zstd found (x.y.z)
  ZSTD_VERSION_MAJOR  - The major version of zstd
  ZSTD_VERSION_MINOR  - The minor version of zstd

  Debug and Release variants are found separately.
#]=======================================================================]

# Standard names to search for
set(ZSTD_NAMES zstd zstd_static)
set(ZSTD_NAMES_DEBUG zstdd zstd_staticd)

find_path(ZSTD_INCLUDE_DIR
          NAMES zstd.h
          PATH_SUFFIXES include)

# Allow ZSTD_LIBRARY to be set manually, as the location of the zstd library
if(NOT ZSTD_LIBRARY)
  find_library(ZSTD_LIBRARY_RELEASE
               NAMES ${ZSTD_NAMES}
               PATH_SUFFIXES lib)
  find_library(ZSTD_LIBRARY_DEBUG
               NAMES ${ZSTD_NAMES_DEBUG}
               PATH_SUFFIXES lib)

  include(SelectLibraryConfigurations)
  select_library_configurations(ZSTD)
endif()

unset(ZSTD_NAMES)
unset(ZSTD_NAMES_DEBUG)

mark_as_advanced(ZSTD_INCLUDE_DIR)

if(ZSTD_INCLUDE_DIR AND EXISTS "${ZSTD_INCLUDE_DIR}/zstd.h")
    file(STRINGS "${ZSTD_INCLUDE_DIR}/zstd.h" ZSTD_H REGEX "^#define ZSTD_VERSION_.*$")

    string(REGEX REPLACE "^.*ZSTD_VERSION_MAJOR  *([0-9]+).*$" "\\1" ZSTD_MAJOR_VERSION "${ZSTD_H}")
    string(REGEX REPLACE "^.*ZSTD_VERSION_MINOR  *([0-9]+).*$" "\\1" ZSTD_MINOR_VERSION "${ZSTD_H}")
    string(REGEX REPLACE "^.*ZSTD_VERSION_RELEASE  *([0-9]+).*$" "\\1" ZSTD_PATCH_VERSION "${ZSTD_H}")
    set(ZSTD_VERSION_STRING "${ZSTD_MAJOR_VERSION}.${ZSTD_MINOR_VERSION}.${ZSTD_PATCH_VERSION}")
endif()

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(ZSTD
        REQUIRED_VARS ZSTD_LIBRARY ZSTD_INCLUDE_DIR
        VERSION_VAR ZSTD_VERSION_STRING)

if(ZSTD_FOUND)
    set(ZSTD_INCLUDE_DIRS ${ZSTD_INCLUDE_DIR})

    if(NOT ZSTD_LIBRARIES)
        set(ZSTD_LIBRARIES ${ZSTD_LIBRARY})
    endif()

    if(NOT TARGET ZSTD::ZSTD)
        add_library(ZSTD::ZSTD UNKNOWN IMPORTED)
        set_target_properties(ZSTD::ZSTD PROPERTIES
                INTERFACE_INCLUDE_DIRECTORIES "${ZSTD_INCLUDE_DIRS}")

        if(ZSTD_LIBRARY_RELEASE)
            set_property(TARGET ZSTD::ZSTD APPEND PROPERTY
                    IMPORTED_CONFIGURATIONS RELEASE)
            set_target_properties(ZSTD::ZSTD PROPERTIES
                    IMPORTED_LOCATION_RELEASE "${ZSTD_LIBRARY_RELEASE}")
        endif()

        if(ZSTD_LIBRARY_DEBUG)
            set_property(TARGET ZSTD::ZSTD APPEND PROPERTY
                    IMPORTED_CONFIGURATIONS DEBUG)
            set_target_properties(ZSTD::ZSTD PROPERTIES
                    IMPORTED_LOCATION_DEBUG "${ZSTD_LIBRARY_DEBUG}")
        endif()

        if(NOT ZSTD_LIBRARY_RELEASE AND NOT ZSTD_LIBRARY_DEBUG)
            set_target_properties(ZSTD::ZSTD PROPERTIES
                    IMPORTED_LOCATION_RELEASE "${ZSTD_LIBRARY}")
        endif()
    endif()
endif()
