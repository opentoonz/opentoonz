# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#[=======================================================================[.rst:
FindDeflate
--------

Find the native Deflate includes and library.

IMPORTED Targets
^^^^^^^^^^^^^^^^

This module defines :prop_tgt:`IMPORTED` target ``Deflate::Deflate``, if
Deflate has been found.

Result Variables
^^^^^^^^^^^^^^^^

This module defines the following variables:

::

  Deflate_INCLUDE_DIRS   - where to find deflate.h, etc.
  Deflate_LIBRARIES      - List of libraries when using deflate.
  Deflate_FOUND          - True if deflate found.

::

  Deflate_VERSION_STRING - The version of deflate found (x.y.z)
  Deflate_VERSION_MAJOR  - The major version of deflate
  Deflate_VERSION_MINOR  - The minor version of deflate

  Debug and Release variants are found separately.
#]=======================================================================]

# Standard names to search for
set(Deflate_NAMES deflate deflatestatic)
set(Deflate_NAMES_DEBUG deflated deflatestaticd)

find_path(Deflate_INCLUDE_DIR
          NAMES libdeflate.h
          PATH_SUFFIXES include)

set(Deflate_OLD_FIND_LIBRARY_PREFIXES "${CMAKE_FIND_LIBRARY_PREFIXES}")
# Library has a "lib" prefix even on Windows.
set(CMAKE_FIND_LIBRARY_PREFIXES "lib" "")

# Allow Deflate_LIBRARY to be set manually, as the location of the deflate library
if(NOT Deflate_LIBRARY)
  find_library(Deflate_LIBRARY_RELEASE
               NAMES ${Deflate_NAMES}
               PATH_SUFFIXES lib)
  find_library(Deflate_LIBRARY_DEBUG
               NAMES ${Deflate_NAMES_DEBUG}
               PATH_SUFFIXES lib)

  include(SelectLibraryConfigurations)
  select_library_configurations(Deflate)
endif()

set(CMAKE_FIND_LIBRARY_PREFIXES "${Deflate_OLD_FIND_LIBRARY_PREFIXES}")

unset(Deflate_NAMES)
unset(Deflate_NAMES_DEBUG)
unset(Deflate_OLD_FIND_LIBRARY_PREFIXES)

mark_as_advanced(Deflate_INCLUDE_DIR)

if(Deflate_INCLUDE_DIR AND EXISTS "${Deflate_INCLUDE_DIR}/deflate.h")
    file(STRINGS "${Deflate_INCLUDE_DIR}/libdeflate.h" Deflate_H REGEX "^#define LIBDEFLATE_VERSION_STRING\s+\"[^\"]*\"$")

    string(REGEX REPLACE "^.*Deflate_VERSION \"([0-9]+).*$" "\\1" Deflate_MAJOR_VERSION "${Deflate_H}")
    string(REGEX REPLACE "^.*Deflate_VERSION \"[0-9]+\\.([0-9]+).*$" "\\1" Deflate_MINOR_VERSION  "${Deflate_H}")
    set(Deflate_VERSION_STRING "${Deflate_MAJOR_VERSION}.${Deflate_MINOR_VERSION}")

    set(Deflate_MAJOR_VERSION "${Deflate_VERSION_MAJOR}")
    set(Deflate_MINOR_VERSION "${Deflate_VERSION_MINOR}")
endif()

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(Deflate
        REQUIRED_VARS Deflate_LIBRARY Deflate_INCLUDE_DIR
        VERSION_VAR Deflate_VERSION_STRING)

if(Deflate_FOUND)
    set(Deflate_INCLUDE_DIRS ${Deflate_INCLUDE_DIR})

    if(NOT Deflate_LIBRARIES)
        set(Deflate_LIBRARIES ${Deflate_LIBRARY})
    endif()

    if(NOT TARGET Deflate::Deflate)
        add_library(Deflate::Deflate UNKNOWN IMPORTED)
        set_target_properties(Deflate::Deflate PROPERTIES
                INTERFACE_INCLUDE_DIRECTORIES "${Deflate_INCLUDE_DIRS}")

        if(Deflate_LIBRARY_RELEASE)
            set_property(TARGET Deflate::Deflate APPEND PROPERTY
                    IMPORTED_CONFIGURATIONS RELEASE)
            set_target_properties(Deflate::Deflate PROPERTIES
                    IMPORTED_LOCATION_RELEASE "${Deflate_LIBRARY_RELEASE}")
        endif()

        if(Deflate_LIBRARY_DEBUG)
            set_property(TARGET Deflate::Deflate APPEND PROPERTY
                    IMPORTED_CONFIGURATIONS DEBUG)
            set_target_properties(Deflate::Deflate PROPERTIES
                    IMPORTED_LOCATION_DEBUG "${Deflate_LIBRARY_DEBUG}")
        endif()

        if(NOT Deflate_LIBRARY_RELEASE AND NOT Deflate_LIBRARY_DEBUG)
            set_target_properties(Deflate::Deflate PROPERTIES
                    IMPORTED_LOCATION_RELEASE "${Deflate_LIBRARY}")
        endif()
    endif()
endif()
