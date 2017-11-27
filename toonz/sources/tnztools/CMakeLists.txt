set(MOC_HEADERS
    fullcolorbrushtool.h
    controlpointselection.h
    ../include/tools/imagegrouping.h
    edittoolgadgets.h
    skeletonsubtools.h
    tooloptionscontrols.h
    plastictool.h
    ../include/tools/toolhandle.h
    ../include/tools/tooloptions.h
    ../include/tools/screenpicker.h
    rgbpickertool.h
    rulertool.h
    stylepickertool.h
)

set(HEADERS ${MOC_HEADERS}
    autofill.h
    bluredbrush.h
    brushtool.h
    ../include/tools/cursormanager.h
    ../include/tools/cursors.h
    ../include/tools/levelselection.h
    ../include/tools/rasterselection.h
    hookselection.h
    selectiontool.h
    setsaveboxtool.h
    rasterselectiontool.h
    vectorselectiontool.h
    ../include/tools/strokeselection.h
    ../include/tools/stylepicker.h
    ../include/tools/tool.h
    ../include/tools/toolcommandids.h
    ../include/tools/toolutils.h
    ../include/tools/RGBpicker.h
    mypainttoonzbrush.h
)

set(SOURCES
    autofillpli.cpp
    autofilltlv.cpp
    cursormanager.cpp
    stylepicker.cpp
    tool.cpp
    toolhandle.cpp
    toolutils.cpp
    bendertool.cpp
    bluredbrush.cpp
    brushtool.cpp
    controlpointeditortool.cpp
    cuttertool.cpp
    edittool.cpp
    edittoolgadgets.cpp
    filltool.cpp
    fullcolorbrushtool.cpp
    fullcolorerasertool.cpp
    geometrictool.cpp
    hooktool.cpp
    hookselection.cpp
    imagegrouping.cpp
    irontool.cpp
    levelselection.cpp
    magnettool.cpp
    morphtool.cpp
    paintbrushtool.cpp
    pinchtool.cpp
    plastictool.cpp
    plastictool_animate.cpp
    plastictool_build.cpp
    plastictool_meshedit.cpp
    plastictool_rigidity.cpp
    pumptool.cpp
    rastererasertool.cpp
    rastertapetool.cpp
    rasterselectiontool.cpp
    rgbpickertool.cpp
    selectiontool.cpp
    setsaveboxtool.cpp
    skeletonsubtools.cpp
    skeletontool.cpp
    stylepickertool.cpp
    trackertool.cpp
    typetool.cpp
    tooloptionscontrols.cpp
    tooloptions.cpp
    vectorerasertool.cpp
    vectorselectiontool.cpp
    vectortapetool.cpp
    viewtools.cpp
    controlpointselection.cpp
    rasterselection.cpp
    strokeselection.cpp
    screenpicker.cpp
    fingertool.cpp
    rulertool.cpp
    mypainttoonzbrush.cpp
)

set(RESOURCES tnztools.qrc)

qt5_add_resources(SOURCES ${RESOURCES})

add_translation(tnztools ${HEADERS} ${SOURCES})

qt5_wrap_cpp(SOURCES ${MOC_HEADERS})

add_library(tnztools SHARED ${HEADERS} ${SOURCES} ${RESOURCES})
add_definitions(
    -DTNZTOOLS_EXPORTS
)

if(BUILD_ENV_APPLE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wl,-install_name,@rpath/libtnztools.dylib")
    add_dependencies(tnztools tnzcore tnzbase tnzext toonzlib toonzqt)
endif()

message("subdir: tnztools")
message("Bin: " ${CMAKE_CURRENT_BINARY_DIR})

include_directories(
    SYSTEM
    ${SDKROOT}/superlu/SupperLU_4.1/SRC
)
include_directories(
    ../include/tools
)

_find_toonz_library(EXTRA_LIBS "tnzcore;tnzbase;tnzext;toonzlib;toonzqt")

target_link_libraries(tnztools Qt5::Core Qt5::Gui Qt5::Widgets Qt5::Network Qt5::OpenGL ${GLUT_LIB} ${EXTRA_LIBS} ${GL_LIB} ${MYPAINT_LIB_LDFLAGS})
