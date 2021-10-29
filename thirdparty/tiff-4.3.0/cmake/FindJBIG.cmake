# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#[=======================================================================[.rst:
FindJBIG
--------

Find the native JBIG includes and library.

IMPORTED Targets
^^^^^^^^^^^^^^^^

This module defines :prop_tgt:`IMPORTED` target ``JBIG::JBIG``, if
JBIG has been found.

Result Variables
^^^^^^^^^^^^^^^^

This module defines the following variables:

::

  JBIG_INCLUDE_DIRS   - where to find jbig.h, etc.
  JBIG_LIBRARIES      - List of libraries when using jbig.
  JBIG_FOUND          - True if jbig found.

::

  JBIG_VERSION_STRING - The version of jbig found (x.y.z)
  JBIG_VERSION_MAJOR  - The major version of jbig
  JBIG_VERSION_MINOR  - The minor version of jbig

  Debug and Release variants are found separately.
#]=======================================================================]

# Standard names to search for
set(JBIG_NAMES jbig)
set(JBIG_NAMES_DEBUG jbigd)

find_path(JBIG_INCLUDE_DIR
          NAMES jbig.h
          PATH_SUFFIXES include)

set(JBIG_OLD_FIND_LIBRARY_PREFIXES "${CMAKE_FIND_LIBRARY_PREFIXES}")
# Library has a "lib" prefix even on Windows.
set(CMAKE_FIND_LIBRARY_PREFIXES "lib" "")

# Allow JBIG_LIBRARY to be set manually, as the location of the jbig library
if(NOT JBIG_LIBRARY)
  find_library(JBIG_LIBRARY_RELEASE
               NAMES ${JBIG_NAMES}
               PATH_SUFFIXES lib)
  find_library(JBIG_LIBRARY_DEBUG
               NAMES ${JBIG_NAMES_DEBUG}
               PATH_SUFFIXES lib)

  include(SelectLibraryConfigurations)
  select_library_configurations(JBIG)
endif()

set(CMAKE_FIND_LIBRARY_PREFIXES "${JBIG_OLD_FIND_LIBRARY_PREFIXES}")

unset(JBIG_NAMES)
unset(JBIG_NAMES_DEBUG)
unset(JBIG_OLD_FIND_LIBRARY_PREFIXES)

mark_as_advanced(JBIG_INCLUDE_DIR)

if(JBIG_INCLUDE_DIR AND EXISTS "${JBIG_INCLUDE_DIR}/jbig.h")
    file(STRINGS "${JBIG_INCLUDE_DIR}/jbig.h" JBIG_H REGEX "^#define JBG_VERSION  *\"[^\"]*\"$")

    string(REGEX REPLACE "^.*JBG_VERSION  *\"([0-9]+).*$" "\\1" JBIG_MAJOR_VERSION "${JBIG_H}")
    string(REGEX REPLACE "^.*JBG_VERSION  *\"[0-9]+\\.([0-9]+).*$" "\\1" JBIG_MINOR_VERSION  "${JBIG_H}")
    set(JBIG_VERSION_STRING "${JBIG_MAJOR_VERSION}.${JBIG_MINOR_VERSION}")

    set(JBIG_MAJOR_VERSION "${JBIG_VERSION_MAJOR}")
    set(JBIG_MINOR_VERSION "${JBIG_VERSION_MINOR}")
endif()

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(JBIG
        REQUIRED_VARS JBIG_LIBRARY JBIG_INCLUDE_DIR
        VERSION_VAR JBIG_VERSION_STRING)

if(JBIG_FOUND)
    set(JBIG_INCLUDE_DIRS ${JBIG_INCLUDE_DIR})

    if(NOT JBIG_LIBRARIES)
        set(JBIG_LIBRARIES ${JBIG_LIBRARY})
    endif()

    if(NOT TARGET JBIG::JBIG)
        add_library(JBIG::JBIG UNKNOWN IMPORTED)
        set_target_properties(JBIG::JBIG PROPERTIES
                INTERFACE_INCLUDE_DIRECTORIES "${JBIG_INCLUDE_DIRS}")

        if(JBIG_LIBRARY_RELEASE)
            set_property(TARGET JBIG::JBIG APPEND PROPERTY
                    IMPORTED_CONFIGURATIONS RELEASE)
            set_target_properties(JBIG::JBIG PROPERTIES
                    IMPORTED_LOCATION_RELEASE "${JBIG_LIBRARY_RELEASE}")
        endif()

        if(JBIG_LIBRARY_DEBUG)
            set_property(TARGET JBIG::JBIG APPEND PROPERTY
                    IMPORTED_CONFIGURATIONS DEBUG)
            set_target_properties(JBIG::JBIG PROPERTIES
                    IMPORTED_LOCATION_DEBUG "${JBIG_LIBRARY_DEBUG}")
        endif()

        if(NOT JBIG_LIBRARY_RELEASE AND NOT JBIG_LIBRARY_DEBUG)
            set_target_properties(JBIG::JBIG PROPERTIES
                    IMPORTED_LOCATION_RELEASE "${JBIG_LIBRARY}")
        endif()
    endif()
endif()
