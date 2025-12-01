set(PREVIEW_SOURCES
    toonzpreview.cpp
    toonzpreview.def
)

set(PREVIEW_HEADERS
    toonzpreview.h
)

add_library(toonzpreview SHARED ${PREVIEW_SOURCES} ${PREVIEW_HEADERS})

target_compile_definitions(toonzpreview PRIVATE
    TOONZPREVIEW_EXPORTS
    _ATL_NO_AUTOMATIC_NAMESPACE
    UNICODE
    _UNICODE
)

message("subdir: toonz")
message("Bin: " ${CMAKE_CURRENT_BINARY_DIR})

target_include_directories(toonzpreview PRIVATE
    ../include/toonzqt
    ../include/toonz
    ../include/tcg
    ${Qt5Widgets_INCLUDE_DIRS}
)

target_link_libraries(toonzpreview PRIVATE
    image
    toonzqt
    shlwapi
    tnzcore
    Qt5::Core
    ${OpenCV_LIBS}
)

set_target_properties(toonzpreview PROPERTIES
    PREFIX ""
    SUFFIX ".dll"
    OUTPUT_NAME "toonzpreview"
)