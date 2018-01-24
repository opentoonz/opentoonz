set(HEADERS
    ../include/colorfx.h
    colorfxutils.h
    rasterstyles.h
    regionstyles.h
    strokestyles.h
    zigzagstyles.h
)

set(SOURCES
    colorfx.cpp
    colorfxutils.cpp
    rasterstyles.cpp
    regionstyles.cpp
    strokestyles.cpp
    zigzagstyles.cpp
)

add_translation(colorfx ${HEADERS} ${SOURCES})

add_library(colorfx SHARED ${HEADERS} ${SOURCES})
add_definitions(
    -D_USRDLL
    -DCOLORFX_EXPORTS
    -DUSE_MESA
)

if(BUILD_ENV_APPLE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wl,-install_name,@rpath/libcolorfx.dylib")
    add_dependencies(colorfx tnzcore tnzbase)
endif()

message("subdir: colorfx")
message("type:" ${CMAKE_SHARED_LIBRARY_SUFFIX})
message("Bin: " ${CMAKE_CURRENT_BINARY_DIR})

_find_toonz_library(EXTRA_LIBS "tnzcore;tnzbase")

message("FIND_FILE:" ${EXTRA_LIBS})

target_link_libraries(colorfx Qt5::Core ${GL_LIB} ${EXTRA_LIBS})
