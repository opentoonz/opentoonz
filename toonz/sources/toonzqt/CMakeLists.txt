set(MOC_HEADERS
    ../include/toonzqt/addfxcontextmenu.h
    ../include/toonzqt/checkbox.h
    ../include/toonzqt/colorfield.h
    docklayout.h
    ../include/toonzqt/doublefield.h
    ../include/toonzqt/doublepairfield.h
    ../include/toonzqt/dvdialog.h
    ../include/toonzqt/dvmimedata.h
    ../include/toonzqt/dvscrollwidget.h
    ../include/toonzqt/dvtextedit.h
    ../include/toonzqt/expressionfield.h
    ../include/toonzqt/filefield.h
    ../include/toonzqt/flipconsole.h
    ../include/toonzqt/framenavigator.h
    ../include/toonzqt/freelayout.h
    ../include/toonzqt/functionkeyframenavigator.h
    ../include/toonzqt/functionpanel.h
    ../include/toonzqt/functionsegmentviewer.h
    ../include/toonzqt/functionselection.h
    ../include/toonzqt/functionsheet.h
    ../include/toonzqt/functiontoolbar.h
    ../include/toonzqt/functiontreeviewer.h
    ../include/toonzqt/functionviewer.h
    fxdata.h
    ../include/toonzqt/fxhistogramrender.h
    ../include/toonzqt/fxschematicnode.h
    ../include/toonzqt/fxschematicscene.h
    ../include/toonzqt/fxselection.h
    ../include/toonzqt/fxsettings.h
    ../include/toonzqt/gutil.h
    ../include/toonzqt/histogram.h
    ../include/toonzqt/icongenerator.h
    ../include/toonzqt/imageutils.h
    ../include/toonzqt/infoviewer.h
    ../include/toonzqt/intfield.h
    ../include/toonzqt/intpairfield.h
    ../include/toonzqt/keyframenavigator.h
    ../include/toonzqt/lineedit.h
    ../include/toonzqt/menubarcommand.h
    ../include/toonzqt/multipleselection.h
    palettedata.h
    palettesscanpopup.h
    ../include/toonzqt/paletteviewer.h
    ../include/toonzqt/paletteviewergui.h
    ../include/toonzqt/paramfield.h
    ../include/toonzqt/planeviewer.h
    ../include/toonzqt/popupbutton.h
    ../include/toonzqt/rasterimagedata.h
    ../include/toonzqt/schematicnode.h
    ../include/toonzqt/schematicgroupeditor.h
    ../include/toonzqt/schematicviewer.h
    ../include/toonzqt/scriptconsole.h
    ../include/toonzqt/selectioncommandids.h
    ../include/toonzqt/selection.h
    stageobjectselection.h
    ../include/toonzqt/stageobjectsdata.h
    ../include/toonzqt/spectrumfield.h
    ../include/toonzqt/spreadsheetviewer.h
    ../include/toonzqt/stageschematicnode.h
    ../include/toonzqt/stageschematicscene.h
    ../include/toonzqt/strokesdata.h
    ../include/toonzqt/studiopaletteviewer.h
    styledata.h
    ../include/toonzqt/styleeditor.h
    ../include/toonzqt/styleindexlineedit.h
    ../include/toonzqt/styleselection.h
    ../include/toonzqt/swatchviewer.h
    ../include/toonzqt/tabbar.h
    tdockwindows.h
    ../include/toonzqt/tonecurvefield.h
    ../include/toonzqt/treemodel.h
    ../include/toonzqt/tselectionhandle.h
    ../include/toonzqt/updatechecker.h
    ../include/toonzqt/validatedchoicedialog.h
    ../include/toonzqt/viewcommandids.h
    ../include/toonzqt/trepetitionguard.h
    ../include/toonzqt/camerasettingswidget.h
    ../include/toonzqt/cleanupcamerasettingswidget.h
    ../include/toonzqt/pickrgbutils.h
    ../include/toonzqt/screenboard.h
    ../include/toonzqt/marksbar.h
    ../include/toonzqt/tmessageviewer.h
    ../include/toonzqt/stylenameeditor.h
    ../include/historytypes.h
    ../include/toonzqt/flipconsoleowner.h
    ../include/toonzqt/combohistogram.h
    ../include/toonzqt/fxiconmanager.h
    ../include/toonzqt/glwidget_for_highdpi.h
    ../include/toonzqt/lutcalibrator.h
    pluginhost.h
)

set(HEADERS
    ${MOC_HEADERS}
    functionpaneltools.h
    toonz_hostif.h
    toonz_plugin.h
    plugin_tile_interface.h
    plugin_port_interface.h
    plugin_fxnode_interface.h
)

set(SOURCES
    checkbox.cpp
    colorfield.cpp
    docklayout.cpp
    dockwidget.cpp
    doublefield.cpp
    doublepairfield.cpp
    dvdialog.cpp
    dvscrollwidget.cpp
    dvtextedit.cpp
    expressionfield.cpp
    filefield.cpp
    flipconsole.cpp
    framenavigator.cpp
    freelayout.cpp
    fxhistogramrender.cpp
    fxsettings.cpp
    gutil.cpp
    histogram.cpp
    icongenerator.cpp
    imageutils.cpp
    infoviewer.cpp
    intfield.cpp
    intpairfield.cpp
    keyframenavigator.cpp
    lineedit.cpp
    menubarcommand.cpp
    palettesscanpopup.cpp
    paletteviewer.cpp
    paletteviewergui.cpp
    paramfield.cpp
    planeviewer.cpp
    popupbutton.cpp
    rasterimagedata.cpp
    scriptconsole.cpp
    selection.cpp
    stageobjectsdata.cpp
    spectrumfield.cpp
    spreadsheetviewer.cpp
    strokesdata.cpp
    studiopaletteviewer.cpp
    styleeditor.cpp
    styleindexlineedit.cpp
    swatchviewer.cpp
    tabbar.cpp
    tdockwindows.cpp
    tonecurvefield.cpp
    treemodel.cpp
    tselectionhandle.cpp
    addfxcontextmenu.cpp
    fxschematicnode.cpp
    fxschematicscene.cpp
    schematicnode.cpp
    schematicviewer.cpp
    schematicgroupeditor.cpp
    stageschematicnode.cpp
    stageschematicscene.cpp
    functionselection.cpp
    fxselection.cpp
    stageobjectselection.cpp
    styleselection.cpp
    dvmimedata.cpp
    fxdata.cpp
    palettedata.cpp
    styledata.cpp
    functionkeyframenavigator.cpp
    functionpanel.cpp
    functionpaneltools.cpp
    functionsegmentviewer.cpp
    functionsheet.cpp
    functiontoolbar.cpp
    functiontreeviewer.cpp
    functionviewer.cpp
    updatechecker.cpp
    validatedchoicedialog.cpp
    trepetitionguard.cpp
    camerasettingswidget.cpp
    cleanupcamerasettingswidget.cpp
    pickrgbutils.cpp
    screenboard.cpp
    marksbar.cpp
    tmessageviewer.cpp
    stylenameeditor.cpp
    combohistogram.cpp
    fxiconmanager.cpp
    pluginhost.cpp
    plugin_tile_interface.cpp
    plugin_port_interface.cpp
    plugin_fxnode_interface.cpp
    plugin_param_interface.cpp
    plugin_param_view_interface.cpp
    plugin_ui_page_interface.cpp
    toonz_params.cpp
	lutcalibrator.cpp
)

set(RESOURCES toonzqt.qrc)

add_translation(toonzqt ${HEADERS} ${SOURCES})

get_target_property(QW_LOC Qt5::Widgets INTERFACE_INCLUDE_DIRECTORIES)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(incs)
foreach(dir ${Qt5Widgets_INCLUDE_DIRS})
    set(incs ${incs} -I${dir})
endforeach()

qt5_add_resources(SOURCES ${RESOURCES})

qt5_wrap_cpp(SOURCES ${MOC_HEADERS} OPTIONS ${incs})

add_library(toonzqt SHARED ${HEADERS} ${SOURCES} ${RESOURCES})
if(BUILD_ENV_APPLE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wl,-install_name,@rpath/libtoonzqt.dylib")
    add_dependencies(toonzqt tnzcore tnzbase tnzext toonzlib sound)
endif()

add_definitions(
    -DQT_WIDGETS_LIB
    -DTOONZQT_EXPORTS
)

message("subdir: toonzqt")
message("type:" ${CMAKE_SHARED_LIBRARY_SUFFIX})
message("Bin: " ${CMAKE_CURRENT_BINARY_DIR})

_find_toonz_library(EXTRA_LIBS "tnzcore;tnzbase;tnzext;toonzlib;sound")

if(BUILD_ENV_MSVC)
    set(EXTRA_LIBS ${EXTRA_LIBS}
        advapi32.lib
    )
elseif(BUILD_ENV_UNIXLIKE)
    if(BUILD_TARGET_WIN)
        set(EXTRA_LIBS ${EXTRA_LIBS}
            -ladvapi32
        )
    endif()
endif()

target_link_libraries(toonzqt Qt5::Core Qt5::Gui Qt5::Widgets Qt5::Network Qt5::OpenGL Qt5::Svg ${EXTRA_LIBS} ${GL_LIB})
