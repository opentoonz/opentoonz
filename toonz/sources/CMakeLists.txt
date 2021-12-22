cmake_minimum_required(VERSION 2.8.11)

set(CMAKE_BUILD_TYPE_INIT Release)

project(OpenToonz)


#-----------------------------------------------------------------------------
# Platform Specific Defaults

# list of var-names
set(_init_vars)

#-----------------------------------------------------------------------------
# Select build target
set(BUILD_TARGET_WIN       OFF)
set(BUILD_TARGET_APPLE     OFF)
set(BUILD_TARGET_UNIX      OFF)
if(WIN32)
    set(BUILD_TARGET_WIN    ON)
    message(STATUS "Target system is WIN")
elseif(APPLE)
    set(BUILD_TARGET_APPLE  ON)
    message(STATUS "Target system is APPLE")
elseif(UNIX)
    set(BUILD_TARGET_UNIX   ON)
    message(STATUS "Target system is UNIX")
else()
    set(BUILD_TARGET_UNIX   ON)
    message(WARNING "Target system unknown, set UNIX by default")
    message(STATUS "Target system is UNIX (actually unknown)")
endif()

#-----------------------------------------------------------------------------
# Select build environment

set(BUILD_ENV_MSVC     OFF)
set(BUILD_ENV_APPLE    OFF)
set(BUILD_ENV_UNIXLIKE OFF)
if(MSVC)
    set(BUILD_ENV_MSVC ON)
    if(NOT BUILD_TARGET_WIN)
        message(WARNING "Build environment is MSVC, but target system is not WIN")
    endif()
    message(STATUS "Build environment is MSVC")
elseif(CMAKE_HOST_APPLE)
    set(BUILD_ENV_APPLE ON)
    if(NOT BUILD_TARGET_APPLE)
        message(WARNING "Build environment is APPLE, but target system is not APPLE")
    endif()
    message(STATUS "Build environment is APPLE")
elseif(CMAKE_HOST_UNIX OR CMAKE_COMPILER_IS_GNUCC)
    set(BUILD_ENV_UNIXLIKE ON)
    if(NOT BUILD_TARGET_UNIX AND NOT BUILD_TARGET_WIN)
        message(WARNING "Build environment is UNIXLIKE, but target system is not UNIX or WIN")
    endif()
    message(STATUS "Build environment is UNIXLIKE")
else()
    message(ERROR "Unsupported build environment, only MSVC, APPLE and UNIXLIKE are supported")
endif()


# initialize to ON
macro(option_defaults_init)
    foreach(_var ${ARGV})
        set(${_var} ON)
        list(APPEND _init_vars "${_var}")
    endforeach()
    unset(_var)
endmacro()

# remove from namespace
macro(option_defaults_clear)
    foreach(_var ${_init_vars})
        unset(${_var})
    endforeach()
    unset(_var)
    unset(_init_vars)
endmacro()

# values to initialize WITH_****
option_defaults_init(
    _init_SYSTEM_LZO
)

# customize...
if(BUILD_ENV_MSVC)
    set(_init_SYSTEM_LZO                     OFF)
    set(_init_SYSTEM_SUPERLU                 OFF)
elseif(BUILD_ENV_APPLE)
    set(_init_SYSTEM_LZO                     OFF)
    set(_init_SYSTEM_SUPERLU                 OFF)
elseif(BUILD_ENV_UNIXLIKE)
    set(_init_SYSTEM_LZO                     ON)
    set(_init_SYSTEM_SUPERLU                 ON)
endif()


#-----------------------------------------------------------------------------
# Build Options

option(WITH_SYSTEM_LZO "Use the system LZO library instead of 'thirdpary'" ${_init_SYSTEM_LZO})
option(WITH_SYSTEM_SUPERLU "Use the system SuperLU library instead of 'thirdpary'" ${_init_SYSTEM_SUPERLU})
option(WITH_CANON "Build with Canon DSLR support - Requires Canon SDK" OFF)
option(WITH_TRANSLATION "Generate translation projects as well" ON)
option(WITH_WINTAB "(Windows only) Build with customized Qt with WinTab support. https://github.com/shun-iwasawa/qt5/releases/tag/v5.15.2_wintab" OFF)

# avoid using again
option_defaults_clear()

# end option(...)

#-----------------------------------------------------------------------------
# Platform Architecture

if(BUILD_ENV_APPLE)
    if(NOT PLATFORM)
        set(PLATFORM 64)
    endif()
else() # Windows and Linux case
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(PLATFORM 64)
        set(PLATFORM2 _64)
    elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
        set(PLATFORM 32)
        set(PLATFORM2)
    else()
        message("Warning : CMAKE_SIZEOF_VOID_P = ${CMAKE_SIZEOF_VOID_P}")
        set(PLATFORM 64)
        set(PLATFORM2 _64)
    endif()
endif()
message("PLATFORM:" ${PLATFORM})

#-----------------------------------------------------------------------------
# Third Party & System Libs

include(${CMAKE_SOURCE_DIR}/../cmake/util_compiler.cmake)

get_filename_component(SDKROOT ../../thirdparty/ ABSOLUTE)
message(STATUS "SDK Root:" ${SDKROOT})
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/../cmake")
if(BUILD_ENV_APPLE)
    set(THIRDPARTY_LIBS_HINTS "/usr/local/Cellar/" "/opt/include" ${SDKROOT})
elseif(BUILD_ENV_MSVC OR BUILD_ENV_UNIXLIKE)
    set(THIRDPARTY_LIBS_HINTS ${SDKROOT})
endif()

message(STATUS "Thirdpary Library Search path:" ${THIRDPARTY_LIBS_HINTS})

if(BUILD_ENV_MSVC)
    set(QT_PATH "C:/Qt/5.15.2/msvc2019${PLATFORM2}" CACHE PATH "Qt installation directory")
    if(NOT EXISTS ${QT_PATH})
        message("Specify QT_PATH properly")
        return()
    endif()
    set(QT_LIB_PATH ${QT_PATH})
    set(CMAKE_PREFIX_PATH "${CMAKE_PREFIX_PATH};${QT_PATH}/lib/cmake/")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /wd4251")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4251")
    add_definitions(
        -DVC_EXTRALEAN
        -DNOMINMAX
        -D_USE_MATH_DEFINES
    )
elseif(BUILD_ENV_APPLE)
    set(QT_PATH "~/Qt5.9.2/5.9.2/clang_${PLATFORM}/lib" CACHE PATH "Qt installation directory")
    set(QT_LIB_PATH "${QT_PATH}/")
    set(CMAKE_PREFIX_PATH "${CMAKE_PREFIX_PATH};${QT_LIB_PATH}cmake/")
    foreach(path ${CMAKE_PREFIX_PATH})
      message("CMAKE_PREFIX_PATH: " ${path})
    endforeach(path)
    add_definitions(
        -DMACOSX
        -Di386
        -D__MACOS__
    )
    if(PLATFORM EQUAL 64)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m64 -std=c++11 -stdlib=libc++ -fno-implicit-templates")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -m64")
    elseif(PLATFORM EQUAL 32)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m32 -std=c++11 -stdlib=libc++ -fno-implicit-templates -D HAS_QUICKDRAW")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -m32")
        set(CMAKE_LINKER_FLAGS "${CMAKE_LINKER_FLAGS} -m32")
    else()
        message(FATAL_ERROR "Invalid PLATFORM:" ${PLATFORM} ". 'PLATFORM' must be 32 or 64.")
    endif()
elseif(BUILD_ENV_UNIXLIKE)
    # Needed for correct Qt detection
    cmake_minimum_required(VERSION 2.8.12)
    if(CMAKE_SYSTEM_NAME MATCHES "Linux")
        add_definitions(-DLINUX)
    elseif(CMAKE_SYSTEM_NAME MATCHES "^.*BSD$|DragonFly")
        set(BUILD_TARGET_BSD ON)
        add_definitions(-D_BSD_SOURCE -DFREEBSD)
        include_directories("/usr/local/include/")
        message(STATUS "Configuring for BSD system")
    elseif(NOT BUILD_TARGET_WIN)
        message(WARNING "Support for generic Unix (Not Apple or Linux) isn't yet working!")
    endif()

    set(CMAKE_CXX_STANDARD 11)

    find_package(Qt5Widgets)

    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
    if (NOT (CMAKE_CXX_COMPILER_ID STREQUAL "Clang"))
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -lstdc++")
    endif()
endif()

add_definitions(
    -DQT_GUI_LIB
    -DQT_NETWORK_LIB
    -DQT_CORE_LIB
    -DQT_SHARED
)

# Find includes in corresponding build directories
set(CMAKE_INCLUDE_CURRENT_DIR ON)

if(BUILD_ENV_APPLE)
    set(CMAKE_MACOSX_RPATH ON)
    set(CMAKE_SKIP_BUILD_RPATH FALSE)
    set(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)
    set(CMAKE_INSTALL_RPATH ${QT_LIB_PATH})
    set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
    set(CMAKE_FIND_FRAMEWORK LAST)
    list(FIND CMAKE_PLATFORM_IMPLICIT_LINK_DIRECTORIES "${QT_LIB_PATH}" isSystemDir)
    if("${isSystemDir}" STREQUAL "-1")
        set(CMAKE_INSTALL_RPATH "${QT_LIB_PATH}")
    endif()
endif()

include_directories(
    include
)

# Find the QtWidgets library
find_package(Qt5 REQUIRED
    Core
    Gui
    Network
    OpenGL
    Svg
    Xml
    Script
    Widgets
    PrintSupport
    LinguistTools
    Multimedia
    MultimediaWidgets
    SerialPort
)

set(QT_MINIMUM_VERSION 5.5.0)
if(Qt5Core_VERSION VERSION_LESS QT_MINIMUM_VERSION)
    MESSAGE(FATAL_ERROR "Minimum supported Qt version: ${QT_MINIMUM_VERSION}.
    Installed version: ${Qt5Core_VERSION}")
endif()
unset(QT_MINIMUM_VERSION)

if(BUILD_ENV_MSVC)
    include_directories(
        SYSTEM
        ${SDKROOT}/glut/3.7.6/include
        ${SDKROOT}/zlib-1.2.8
        #${SDKROOT}/LibJPEG/jpeg-9
        ${SDKROOT}/libjpeg-turbo/include
    )
    add_definitions(
        -DGLUT_NO_LIB_PRAGMA
    )
endif()


if(PLATFORM EQUAL 64)
    if(BUILD_ENV_UNIXLIKE)
        find_package(OpenCV REQUIRED)
    else() # Windows and MacOS case
        find_package(OpenCV 4.1 REQUIRED)
    endif()
    #include_directories(
    #    ${SDKROOT}/libjpeg-turbo64/include
    #)
    if(WITH_CANON)
        include_directories(
            ${SDKROOT}/canon/Header
        )
    endif()
endif()

get_target_property(QtCore_location Qt5::Core LOCATION)
get_target_property(QtWidget_location Qt5::Widgets LOCATION)
message(STATUS "Qt Core Location:" ${QtCore_location})

macro(_find_toonz_library OUT_LIB_LIST IN_LIB_LIST)
    set(OUTLIST "")
    foreach(arg ${IN_LIB_LIST})
        unset(COREPATH CACHE)
        # find しても cmake が走るときにはできていないので完全なパス名を生成して返すだけ
        if(BUILD_ENV_APPLE)
            set(LIBPATH "\$<TARGET_FILE:${arg}>")
        else()
            set(LIBPATH ${arg})
        endif()
        set(OUTLIST ${OUTLIST} ${LIBPATH})
    endforeach()
    set(${OUT_LIB_LIST} ${OUTLIST})
endmacro()

set(TNZSTDFX_Location)
set(TFARM_Location)

set(QT_LIB)  # avoid warning

if(BUILD_ENV_MSVC)
    set(MSVC_LIB_VERSION 2015)

    set(GLUT_LIB ${SDKROOT}/glut/3.7.6/lib/glut${PLATFORM}.lib)
    set(GL_LIB opengl32.lib)
    set(Z_LIB
        optimized ${SDKROOT}/zlib-1.2.8/lib/zlib-1.2.8_${MSVC_LIB_VERSION}${PLATFORM2}.lib
        debug ${SDKROOT}/zlib-1.2.8/lib/zlib-1.2.8_${MSVC_LIB_VERSION}${PLATFORM2}d.lib
    )
    #set(JPEG_LIB ${SDKROOT}/LibJPEG/jpeg-9/lib/LibJPEG-9_${MSVC_LIB_VERSION}${PLATFORM2}.lib)
    set(JPEG_LIB ${SDKROOT}/libjpeg-turbo/lib/jpeg-static_${MSVC_LIB_VERSION}${PLATFORM2}.lib)
    set(TURBOJPEG_LIB ${SDKROOT}/libjpeg-turbo/lib/turbojpeg-static_${MSVC_LIB_VERSION}${PLATFORM2}.lib)
    set(TIFF_INCLUDE_DIR ${SDKROOT}/tiff-4.0.3/libtiff)
    set(TIFF_LIB
        optimized ${SDKROOT}/tiff-4.0.3/lib/LibTIFF-4.0.3_${MSVC_LIB_VERSION}${PLATFORM2}.lib
        debug  ${SDKROOT}/tiff-4.0.3/lib/LibTIFF-4.0.3_${MSVC_LIB_VERSION}${PLATFORM2}d.lib
    )
    set(PNG_LIB
        optimized ${SDKROOT}/libpng-1.6.21/lib/libpng16_${MSVC_LIB_VERSION}${PLATFORM2}.lib
        debug  ${SDKROOT}/libpng-1.6.21/lib/libpng16_${MSVC_LIB_VERSION}${PLATFORM2}d.lib
    )
    set(GLEW_LIB ${SDKROOT}/glew/glew-1.9.0/lib/glew${PLATFORM}.lib)
    set(LZ4_LIB ${SDKROOT}/Lz4/Lz4_131/lz4_${PLATFORM}.lib)
    set(SUPERLU_LIB ${SDKROOT}/superlu/SuperLU_${MSVC_LIB_VERSION}_${PLATFORM}.lib)
    set(OPENBLAS_LIB ${SDKROOT}/openblas/libopenblas_${PLATFORM}.lib)
    set(USB_LIB)  # unused
    
    if(PLATFORM EQUAL 32)
        set(QT_LIB ${SDKROOT}/quicktime/QT73SDK/Libraries/QTMLClient.lib)
    endif()
    set(MYPAINT_LIB_INCLUDE_DIRS ${SDKROOT}/libmypaint/dist/${PLATFORM}/include/libmypaint)
    set(MYPAINT_LIB_LDFLAGS ${SDKROOT}/libmypaint/dist/${PLATFORM}/libmypaint.lib)

    if(PLATFORM EQUAL 64)
        if (WITH_CANON)
            set(CANON_LIB ${SDKROOT}/canon/library/EDSDK.lib)
        endif()
    endif()
elseif(BUILD_ENV_APPLE)
    find_library(GLUT_LIB GLUT)
    find_library(GL_LIB OpenGL)
    find_library(Z_LIB z)

    find_package(TIFF REQUIRED)
    set(TIFF_LIB ${TIFF_LIBRARY})
    # the libraries have not .pc nor preferred Find*.cmake. use custom modules.
    find_package(PNG REQUIRED)
    message("******* libpng:" ${PNG_LIBRARY})
    set(PNG_LIB ${PNG_LIBRARY})
    find_package(GLEW)

    find_package(SuperLU REQUIRED)
    set(SUPERLU_INCLUDE_DIR ${SUPERLU_INCLUDE_DIR}/superlu)
    set(SUPERLU_LIB ${SUPERLU_LIBRARY})
    message("SuperLU:" ${SUPERLU_INCLUDE_DIR})

    find_package(JPEG REQUIRED)

    # the libraries have .pc
    find_package(PkgConfig)
    if(GLEW-NOTFOUND)
        pkg_check_modules(GLEW REQUIRED glew)
    endif()
    if (TARGET GLEW::GLEW)
      set(GLEW_LIB GLEW::GLEW)
    else()
      set(GLEW_LIB ${GLEW_LIBRARIES})
    endif()

    pkg_check_modules(LZ4_LIB REQUIRED liblz4)

    message("**************** lz4 lib:" ${LZ4_LIB_INCLUDE_DIRS})
    find_path(LZ4_STATIC lz4frame_static.h PATH ${LZ4_LIB_INCLUDE_DIRS})
    if(LZ4_STATIC-FOUND)
        add_definitions(-DLZ4_STATIC)
    endif()

    pkg_check_modules(USB_LIB REQUIRED libusb-1.0)
    set(OPENBLAS_LIB) # unused?
    if(PLATFORM EQUAL 32)
        find_library(QT_LIB QuickTime)
    endif()

    set(JPEG_LIB ${JPEG_LIBRARY})
    set(LZ4_LIB ${LZ4_LIB_LDFLAGS})

    find_package(LZO REQUIRED)
    message("LZO:" ${LZO_INCLUDE_DIR})

    pkg_check_modules(MYPAINT_LIB REQUIRED libmypaint)

    if(PLATFORM EQUAL 64)
    	pkg_check_modules(TURBOJPEG REQUIRED libturbojpeg)
        find_library(TURBOJPEG_LIB turbojpeg ${TURBOJPEG_LIBRARY_DIRS})
        message("**************** turbojpeg lib:" ${TURBOJPEG_LIB})

        if (WITH_CANON)
            set(CANON_LIB ${SDKROOT}/canon/Framework/EDSDK.framework)
        endif()
    endif()

elseif(BUILD_ENV_UNIXLIKE)
    if(BUILD_TARGET_WIN)
        find_library(GL_LIB opengl32)
        find_library(GLU_LIB glu32)
        find_library(GLUT_LIB NAMES glut freeglut)
    else()
        find_library(GL_LIB GL)
        find_library(GLU_LIB GLU)
        find_library(GLUT_LIB glut)
    endif()
    find_library(Z_LIB z)

    if(NOT BUILD_TARGET_BSD)
        find_package(TIFF REQUIRED)
    endif()
    find_package(PNG REQUIRED)
    set(PNG_LIB ${PNG_LIBRARY})
    message("******* libpng:" ${PNG_LIB})
    find_package(GLEW)

    find_package(SuperLU REQUIRED)
    set(SUPERLU_LIB ${SUPERLU_LIBRARY})

    find_package(JPEG REQUIRED)

    # the libraries have .pc
    find_package(PkgConfig)

    # This is not required for OpenToonz itself, but libtiff will pick it up if
    # present and libimage won't be aware causing linker to fail
    pkg_check_modules(LZMA liblzma)
    set(TIFF_LIB ${TIFF_LIBRARY} ${LZMA_LIBRARIES})

    if(GLEW-NOTFOUND)
        pkg_check_modules(GLEW REQUIRED glew)
    endif()
    set(GLEW_LIB ${GLEW_LIBRARIES})

    pkg_check_modules(LZ4_LIB REQUIRED liblz4)

    message("**************** lz4 lib:" ${LZ4_LIB_INCLUDE_DIRS})
    find_path(LZ4_STATIC lz4frame_static.h PATH ${LZ4_LIB_INCLUDE_DIRS})
    if(LZ4_STATIC-FOUND)
        add_definitions(-DLZ4_STATIC)
    endif()

    # TODO: check for them properly in some sane order with pkg-config
    find_library(OPENBLAS_LIB NAMES cblas gslcblas openblas blas)
    message("OpenBLAS:" ${OPENBLAS_LIB})

    set(JPEG_LIB ${JPEG_LIBRARY})
    set(LZ4_LIB ${LZ4_LIB_LDFLAGS})

    find_package(LZO REQUIRED)
    message("LZO:" ${LZO_INCLUDE_DIR})

    find_library(PTHREAD_LIBRARY pthread)

    if(NOT BUILD_TARGET_WIN)
        pkg_check_modules(USB_LIB REQUIRED libusb-1.0)
        pkg_check_modules(FREETYPE REQUIRED freetype2)
    endif()
    # Can be 'libmypaint' or 'libmypaint-1.x'
    pkg_search_module(MYPAINT_LIB REQUIRED libmypaint libmypaint-1.3>=1.3)
    
    if(PLATFORM EQUAL 64)
        find_library(TURBOJPEG_LIB turbojpeg)
        message("**************** turbojpeg lib:" ${TURBOJPEG_LIB})
    endif()
endif()


include(TestBigEndian)
test_big_endian(_SYSTEM_BIG_ENDIAN)
if(_SYSTEM_BIG_ENDIAN)
    add_definitions(-DTNZ_LITTLE_ENDIAN=0)
else()
    add_definitions(-DTNZ_LITTLE_ENDIAN=1)
endif()
unset(_SYSTEM_BIG_ENDIAN)


set(C_WARNINGS)
set(CXX_WARNINGS)

if(CMAKE_COMPILER_IS_GNUCC)
    ADD_CHECK_C_COMPILER_FLAG(C_WARNINGS C_WARN_WRITE_STRINGS -Wwrite-strings)
    ADD_CHECK_C_COMPILER_FLAG(C_WARNINGS C_WARN_UNDEF -Wundef)
    ADD_CHECK_C_COMPILER_FLAG(C_WARNINGS C_WARN_MISSING_DECLARATIONS -Wmissing-declarations)

    ADD_CHECK_CXX_COMPILER_FLAG(CXX_WARNINGS CXX_WARN_WRITE_STRINGS -Wwrite-strings)
    ADD_CHECK_CXX_COMPILER_FLAG(CXX_WARNINGS CXX_WARN_UNDEF -Wundef)
    ADD_CHECK_CXX_COMPILER_FLAG(CXX_WARNINGS CXX_WARN_MISSING_DECLARATIONS -Wmissing-declarations)
endif()


include_directories(
    SYSTEM
    BEFORE
    ${TIFF_INCLUDE_DIR}
    ${PNG_INCLUDE_DIRS}
)

if(BUILD_ENV_MSVC OR BUILD_ENV_APPLE)
    find_path(
        BOOST_ROOT
            include/boost
            boost
        HINTS
            ${THIRDPARTY_LIBS_HINTS}
        PATH_SUFFIXES
            boost/boost_1_75_0/
            boost/boost_1_74_0/
            boost/boost_1_73_0/
            boost/boost_1_72_0/
    )
    find_package(Boost 1.55 REQUIRED)
else()
    find_package(Boost)
endif()

include_directories(
    SYSTEM
    ${Boost_INCLUDE_DIR}
    ${LZ4_LIB_INCLUDE_DIRS}
    ${USB_LIB_INCLUDE_DIRS}
    ${SUPERLU_INCLUDE_DIR}
    ${JPEG_INCLUDE_DIR}
    ${MYPAINT_LIB_INCLUDE_DIRS}
)

if(PLATFORM EQUAL 64)
    add_definitions(-Dx64)
endif()

if(BUILD_ENV_MSVC)
    # place Toonz.exe and dlls in the same directory
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")
elseif(BUILD_ENV_APPLE)
    # pass
elseif(BUILD_ENV_UNIXLIKE)
    set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR}/bin)

    # Mirror relative bin/lib location for installation
    # so the generated shell script works in both cases.
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib/opentoonz)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib/opentoonz)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

    if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
        set(CMAKE_INSTALL_PREFIX "/opt/opentoonz" CACHE PATH "OpenToonz install prefix" FORCE)
    endif()
endif()

if(BUILD_ENV_MSVC AND MSVC_VERSION EQUAL 1800)
    # Visual C++ 2013 cannot handle UTF-8 without BOM
    # Non-ascii characters in comments would not be a problem
    add_compile_options("/wd4819")
endif()

if(WITH_TRANSLATION)
    # generate Qt translations and messages
    set(LANGUAGES japanese italian french spanish chinese german russian korean czech)

    function(add_translation module)
        set(translation)
        foreach(lang ${LANGUAGES})
            set(name "${CMAKE_SOURCE_DIR}/translations/${lang}/${module}.ts")
            list(APPEND translation ${name})
            set_source_files_properties(${name} PROPERTIES
                OUTPUT_LOCATION "${CMAKE_BINARY_DIR}/loc/${lang}"
            )
        endforeach()

        qt5_create_translation(message ${translation} ${ARGN})

        add_custom_target("translation_${module}" DEPENDS ${message})
        set_target_properties("translation_${module}" PROPERTIES
            EXCLUDE_FROM_DEFAULT_BUILD TRUE
        )
    endfunction()
endif()

set(CMAKE_C_FLAGS "${C_WARNINGS} ${CMAKE_C_FLAGS}")
set(CMAKE_CXX_FLAGS "${CXX_WARNINGS} ${CMAKE_CXX_FLAGS}")

add_subdirectory(tnzcore)
add_subdirectory(tnzbase)
add_subdirectory(tnzext)
add_subdirectory(toonzlib)
add_subdirectory(toonzfarm/tfarm)
add_subdirectory(stdfx)
add_subdirectory(sound)
add_subdirectory(colorfx)
add_subdirectory(image)
add_subdirectory(toonzqt)
add_subdirectory(tnztools)

add_subdirectory(${SDKROOT}/lzo/driver lzodriver)

add_subdirectory(toonz)
add_subdirectory(tcleanupper)
add_subdirectory(tcomposer)
add_subdirectory(tconverter)
add_subdirectory(toonzfarm)

if(BUILD_ENV_APPLE)
    add_subdirectory(mousedragfilter)
endif()

if((PLATFORM EQUAL 32) AND ((BUILD_TARGET_WIN AND BUILD_ENV_MSVC) OR BUILD_TARGET_APPLE))
    add_subdirectory(t32bitsrv)
endif()

if(BUILD_ENV_APPLE)
    add_custom_command(TARGET executable
        POST_BUILD COMMAND
        ${CMAKE_INSTALL_NAME_TOOL} -add_rpath ${CMAKE_INSTALL_RPATH}
        $<TARGET_FILE:executable>
    )
    # add capability for retina displays
    set_target_properties(OpenToonz PROPERTIES MACOSX_BUNDLE_INFO_PLIST ${CMAKE_SOURCE_DIR}/../cmake/BundleInfo.plist.in)
endif()

if(BUILD_ENV_UNIXLIKE AND NOT BUILD_TARGET_WIN)
    add_subdirectory(xdg-data)
endif()
