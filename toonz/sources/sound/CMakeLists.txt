set(HEADERS
    wav/tsio_wav.h
    aiff/tsio_aiff.h
    raw/tsio_raw.h
    ../include/tnzsound.h
    tsio.h)

set(SOURCES
    tsio.cpp
    tsioutils.cpp
    wav/tsio_wav.cpp
    aiff/tsio_aiff.cpp
    raw/tsio_raw.cpp)
    
add_library(sound SHARED ${HEADERS} ${SOURCES})
add_definitions(-DSOUND_EXPORTS)
if (APPLE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wl,-install_name,@rpath/libsound.${CMAKE_SHARED_LIBRARY_SUFFIX}")
endif (APPLE)

message("subdir: sound")
message("type:" ${CMAKE_SHARED_LIBRARY_SUFFIX})
message("Bin: " ${CMAKE_CURRENT_BINARY_DIR})

_find_toonz_library(EXTRA_LIBS tnzcore)
message("ToonzCore: " ${EXTRA_LIBS})

target_link_libraries(sound Qt5::Core ${EXTRA_LIBS})
