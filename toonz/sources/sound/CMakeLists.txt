set(HEADERS
    wav/tsio_wav.h
    aiff/tsio_aiff.h
    raw/tsio_raw.h
    mp3/tsio_mp3.h
    ../include/tnzsound.h
    tsio.h
)

set(SOURCES
    tsio.cpp
    tsioutils.cpp
    wav/tsio_wav.cpp
    aiff/tsio_aiff.cpp
    raw/tsio_raw.cpp
    mp3/tsio_mp3.cpp
)

add_library(sound SHARED ${HEADERS} ${SOURCES})
add_definitions(
    -DSOUND_EXPORTS
)

if(BUILD_ENV_APPLE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wl,-install_name,@rpath/libsound.dylib")
    add_dependencies(sound tnzcore tnzbase toonzlib)
endif()

message("subdir: sound")
message("type:" ${CMAKE_SHARED_LIBRARY_SUFFIX})
message("Bin: " ${CMAKE_CURRENT_BINARY_DIR})

_find_toonz_library(EXTRA_LIBS "tnzcore;tnzbase;toonzlib")
message("ToonzCore: " ${EXTRA_LIBS})

target_link_libraries(sound Qt5::Core ${EXTRA_LIBS})
