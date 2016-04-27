cmake_minimum_required(VERSION 2.8.11)
project(OpenToonz)

get_filename_component(SDKROOT ../../thirdparty/ ABSOLUTE)
message("SDK Root:" ${SDKROOT})
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/../cmake")
if(APPLE)
    set(THIRDPARTY_LIBS_HINTS "/usr/local/Cellar/" "/opt/include" ${SDKROOT})
elseif(WIN32)
    set(THIRDPARTY_LIBS_HINTS ${SDKROOT})
endif()

message("Thirdpary Library Search path:" ${THIRDPARTY_LIBS_HINTS})

if (WIN32)
    message("Windows System")
    if(CMAKE_SIZEOF_VOID_P EQUAL 4)
        set(QT_PATH "C:/Qt/Qt5.6.0_32/5.6/msvc2013" CACHE PATH "Qt instlattion directory")
    else()
        set(QT_PATH "C:/Qt/Qt5.6.0/5.6/msvc2013_64" CACHE PATH "Qt instlattion directory")
    endif()
    if(NOT EXISTS ${QT_PATH})
        message("Specify QT_PATH properly")
        return()
    endif()
    set(QT_LIB_PATH ${QT_PATH})
    set(CMAKE_PREFIX_PATH "${QT_PATH}/lib/cmake/")
    add_definitions(-DNOMINMAX)
elseif (APPLE)
    message("Apple System")
    if (NOT PLATFORM)
        set(PLATFORM 64)
    endif()
    message("PLATFORM:" ${PLATFORM})
    if (PLATFORM EQUAL 64)
        set(QT_PATH "~/Qt/5.6/clang_64/lib" CACHE PATH "Qt instlattion directory")
        set(QT_LIB_PATH "${QT_PATH}/")
        set(CMAKE_PREFIX_PATH "${QT_LIB_PATH}cmake/")

        message("CMAKE_PREFIX_PATH:" ${CMAKE_PREFIX_PATH})
        add_definitions(-DMACOSX -Di386)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m64 -std=c++11 -stdlib=libc++ -fno-implicit-templates")
    elseif(PLATFORM EQUAL 32)
        set(QT_PATH "~/Qt/5.6/clang_32/lib" CACHE PATH "Qt instlattion directory")
        set(QT_LIB_PATH "${QT_PATH}/")
        set(CMAKE_PREFIX_PATH "${QT_LIB_PATH}cmake/")

        message("CMAKE_PREFIX_PATH:" ${CMAKE_PREFIX_PATH})
        add_definitions(-DMACOSX -Di386)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m32 -std=c++11 -stdlib=libc++ -fno-implicit-templates -D HAS_QUICKDRAW")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -m32")
        set(CMAKE_LINKER_FLAGS "${CMAKE_LINKER_FLAGS} -m32")
    else()
        message(FATAL_ERROR "Invalid PLATFORM:" ${PLATFORM} ". 'PLATFORM' must be 32 or 64.")
    endif()
else ()
    message("Unknown System")
endif ()

add_definitions(-DQT_GUI_LIB -DQT_NETWORK_LIB -DQT_CORE_LIB -DQT_SHARED)

# Find includes in corresponding build directories
set(CMAKE_INCLUDE_CURRENT_DIR ON)

if(APPLE)
    set(CMAKE_MACOSX_RPATH ON)
    set(CMAKE_SKIP_BUILD_RPATH FALSE)
    set(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE) 
    set(CMAKE_INSTALL_RPATH ${QT_LIB_PATH})
    set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
    list(FIND CMAKE_PLATFORM_IMPLICIT_LINK_DIRECTORIES "${QT_LIB_PATH}" isSystemDir)
    if("${isSystemDir}" STREQUAL "-1")
        set(CMAKE_INSTALL_RPATH "${QT_LIB_PATH}")
    endif()
endif()

include_directories(include)

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
    LinguistTools)

if(WIN32)
    include_directories(
        ${SDKROOT}/glut/3.7.6/include
        ${SDKROOT}/zlib-1.2.8
        ${SDKROOT}/LibJPEG/jpeg-9)
    add_definitions(-DGLUT_NO_LIB_PRAGMA)
endif()

get_target_property(QtCore_location Qt5::Core LOCATION)
get_target_property(QtWidget_location Qt5::Widgets LOCATION)
message("Qt Core Location:" ${QtCore_location})

macro(_find_toonz_library OUT_LIB_LIST IN_LIB_LIST)
    set(OUTLIST "")
    foreach(arg ${IN_LIB_LIST})
        unset(COREPATH CACHE)
        # find しても cmake が走るときにはできていないので完全なパス名を生成して返すだけ
        if(WIN32)
            set(LIBPATH ${arg})
        else()
            set(LIBPATH ${CMAKE_CURRENT_BINARY_DIR}/../${arg}/lib${arg}${CMAKE_SHARED_LIBRARY_SUFFIX})
        endif()
        set(OUTLIST ${OUTLIST} ${LIBPATH})
    endforeach()
    set(${OUT_LIB_LIST} ${OUTLIST})
endmacro()

set(TNZSTDFX_Location)
set(TFARM_Location)

if(WIN32)
    if (CMAKE_SIZEOF_VOID_P EQUAL 4)
        set(PLATFORM 32)
        set(PLATFORM2)
    else()
        set(PLATFORM 64)
        set(PLATFORM2 _64)
    endif()

    set(GLUT_LIB ${SDKROOT}/glut/3.7.6/lib/glut${PLATFORM}.lib)
    set(GL_LIB opengl32.lib)
    set(Z_LIB
        optimized ${SDKROOT}/zlib-1.2.8/lib/zlib-1.2.8${PLATFORM2}.lib
        debug ${SDKROOT}/zlib-1.2.8/lib/zlib-1.2.8${PLATFORM2}d.lib)
    set(JPEG_LIB ${SDKROOT}/LibJPEG/jpeg-9/lib/LibJPEG-9${PLATFORM2}.lib)
    set(TIFF_INCLUDE_DIR ${SDKROOT}/tiff-4.0.3/libtiff)
    set(TIFF_LIB
        optimized ${SDKROOT}/tiff-4.0.3/lib/LibTIFF-4.0.3${PLATFORM2}.lib
        debug  ${SDKROOT}/tiff-4.0.3/lib/LibTIFF-4.0.3${PLATFORM2}d.lib)
    set(PNG_LIB
        optimized ${SDKROOT}/libpng-1.6.21/lib/libpng16${PLATFORM2}.lib
        debug  ${SDKROOT}/libpng-1.6.21/lib/libpng16${PLATFORM2}d.lib)
    set(GLEW_LIB ${SDKROOT}/glew/glew-1.9.0/lib/glew${PLATFORM}.lib)
    set(LZ4_LIB ${SDKROOT}/Lz4/Lz4_131/lz4_${PLATFORM}.lib)
    set(SUPERLU_LIB ${SDKROOT}/superlu/SuperLU_${PLATFORM}.lib)
    set(OPENBLAS_LIB ${SDKROOT}/openblas/libopenblas_${PLATFORM}.lib)
    set(USB_LIB)  # unused
    if (PLATFORM EQUAL 32)
        set(QT_LIB ${SDKROOT}/quicktime/QT73SDK/Libraries/QTMLClient.lib)
    endif()
else()
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
    set(GLEW_LIB ${GLEW_LIBRARIES})

    pkg_check_modules(LZ4_LIB REQUIRED liblz4)

    message("**************** lz4 lib:" ${LZ4_LIB_INCLUDE_DIRS})
    find_path(LZ4_STATIC lz4frame_static.h PATH ${LZ4_LIB_INCLUDE_DIRS})
    if(LZ4_STATIC-FOUND)
       add_definitions(-DLZ4_STATIC)
    endif()

    pkg_check_modules(USB_LIB REQUIRED libusb-1.0)
    set(OPENBLAS_LIB) # unused?
    if (PLATFORM EQUAL 32)
        find_library(QT_LIB QuickTime)
    endif()

    set(JPEG_LIB ${JPEG_LIBRARY})
    set(LZ4_LIB ${LZ4_LIB_LDFLAGS})

    find_package(LZO REQUIRED)
    message("LZO:" ${LZO_INCLUDE_DIR})
endif()

include_directories(BEFORE ${TIFF_INCLUDE_DIR} ${PNG_INCLUDE_DIRS})

find_path(BOOST_ROOT include/boost boost HINTS ${THIRDPARTY_LIBS_HINTS}
	PATH_SUFFIXES boost/boost_1_60_0/ boost/boost_1_59_0/
	boost/boost_1_58_0/ boost/boost_1_57_0/ boost/boost_1_56_0/ boost155/1.55.0_1/ boost/boost_1_55_0/ )
find_package(Boost 1.55 REQUIRED)

include_directories(${Boost_INCLUDE_DIR} ${LZ4_LIB_INCLUDE_DIRS} ${USB_LIB_INCLUDE_DIRS} ${SUPERLU_INCLUDE_DIR} ${JPEG_INCLUDE_DIR})

if(WIN32 AND PLATFORM EQUAL 64)
    add_definitions(-Dx64)
endif()

if(WIN32)
    # place Toonz.exe and dlls in the same directory
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")
endif()

if (MSVC AND MSVC_VERSION EQUAL 1800)
    # Visual C++ 2013 cannot handle UTF-8 without BOM
    # Non-ascii characters in comments would not be a problem
    add_compile_options("/wd4819")
endif()

# generate Qt translations and messages
set(LANGUAGES japanese italian french spanish)

function(add_translation module)
    set(translation)
    foreach(lang ${LANGUAGES})
        set(name "${CMAKE_SOURCE_DIR}/translations/${lang}/${module}.ts")
        list(APPEND translation ${name})
        set_source_files_properties(${name} PROPERTIES
            OUTPUT_LOCATION "${CMAKE_BINARY_DIR}/loc/${lang}")
    endforeach()

    qt5_create_translation(message ${translation} ${ARGN})

    add_custom_target("translation_${module}" DEPENDS ${message})
    set_target_properties("translation_${module}" PROPERTIES
        EXCLUDE_FROM_DEFAULT_BUILD TRUE)
endfunction()

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
if(PLATFORM EQUAL 32)
    add_subdirectory(t32bitsrv)
endif()

if (APPLE)
    add_custom_command(TARGET executable
        POST_BUILD COMMAND
        ${CMAKE_INSTALL_NAME_TOOL} -add_rpath ${CMAKE_INSTALL_RPATH}
        $<TARGET_FILE:executable>)
endif (APPLE)
