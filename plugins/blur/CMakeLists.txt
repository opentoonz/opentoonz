project(blur_plugin C CXX)
set(PLUGINSDK_ROOT ../../toonz/sources/toonzqt)
set(PLUGINSDK_UTILS_PATH ../)

set(HEADERS
    ${PLUGINSDK_ROOT}/toonz_plugin.h
    ${PLUGINSDK_ROOT}/toonz_hostif.h
)

set(SOURCES
    blur.cpp
)

if(APPLE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -stdlib=libc++")
endif()

add_library(blur SHARED ${HEADERS} ${SOURCES})

set_target_properties(blur PROPERTIES
    PREFIX ""
    SUFFIX ".plugin"
)

include_directories(${PLUGINSDK_ROOT} ${PLUGINSDK_UTILS_PATH})
