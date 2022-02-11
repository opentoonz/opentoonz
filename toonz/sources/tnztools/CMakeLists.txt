set(MOC_HEADERS
    controlpointselection.h
    edittoolgadgets.h
    filltool.h
    fullcolorbrushtool.h
    fullcolorfilltool.h
    plastictool.h
    skeletonsubtools.h
    tooloptionscontrols.h
    toonzrasterbrushtool.h
    viewtools.h
    controlpointeditortool.h
    ../include/tools/imagegrouping.h
    ../include/tools/screenpicker.h
    ../include/tools/toolhandle.h
    ../include/tools/tooloptions.h
)

set(HEADERS
    ${MOC_HEADERS}
    autofill.h
    bluredbrush.h
    hookselection.h
    mypainttoonzbrush.h
    rasterselectiontool.h
    rgbpickertool.h
    rulertool.h
    selectiontool.h
    setsaveboxtool.h
    shifttracetool.h
    stylepickertool.h
    toonzvectorbrushtool.h
    vectorselectiontool.h
    ../include/tools/RGBpicker.h
    ../include/tools/cursormanager.h
    ../include/tools/cursors.h
    ../include/tools/levelselection.h
    ../include/tools/rasterselection.h
    ../include/tools/strokeselection.h
    ../include/tools/stylepicker.h
    ../include/tools/tool.h
    ../include/tools/toolcommandids.h
    ../include/tools/toolutils.h
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
    controlpointeditortool.cpp
    cuttertool.cpp
    edittool.cpp
    edittoolgadgets.cpp
    filltool.cpp
    fullcolorbrushtool.cpp
    fullcolorerasertool.cpp
    fullcolorfilltool.cpp
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
    shifttracetool.cpp
    toonzrasterbrushtool.cpp
	toonzvectorbrushtool.cpp
)

set(RESOURCES tnztools.qrc)

qt5_add_resources(SOURCES ${RESOURCES})

if(WITH_TRANSLATION)
    add_translation(tnztools ${HEADERS} ${SOURCES})
endif()

qt5_wrap_cpp(SOURCES ${MOC_HEADERS})

add_library(tnztools SHARED ${HEADERS} ${SOURCES} ${RESOURCES})
add_definitions(
    -DTNZTOOLS_EXPORTS
)

if(BUILD_ENV_APPLE)
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-install_name,@executable_path/libtnztools.dylib")
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
