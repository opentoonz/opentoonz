set(VERSION 1.0)

set(MOC_HEADERS
    actioncommand.h
    addfilmstripframespopup.h
    adjustlevelspopup.h
    adjustthicknesspopup.h
    antialiaspopup.h
    batches.h
    batchserversviewer.h
    binarizepopup.h
    brightnessandcontrastpopup.h
    cachefxcommand.h
    camerasettingspopup.h
    canvassizepopup.h
    castselection.h
    castviewer.h
    celldata.h
    cellkeyframedata.h
    cellkeyframeselection.h
    cellselection.h
    cleanuppaletteviewer.h
    cleanuppopup.h
    cleanuppreview.h
    cleanupsettingsmodel.h
    cleanupsettingspopup.h
    cleanupswatch.h
    colormodelviewer.h
    columncommand.h
    columnselection.h
    convertpopup.h
    curveio.h
    drawingdata.h
    duplicatepopup.h
    dvdirtreeview.h
    dvitemview.h
    dvwidgets.h
    exportlevelcommand.h
    exportlevelpopup.h
    exportscenepopup.h
    exportpanel.h
    filebrowser.h
    filebrowsermodel.h
    filebrowserpopup.h
    filedata.h
    fileinfopopup.h
    fileselection.h
    filmstrip.h
    filmstripcommand.h
    filmstripselection.h
    flipbook.h
    floatingpanelcommand.h
    formatsettingspopups.h
    frameheadgadget.h
    fxparameditorpopup.h
    histogrampopup.h
    history.h
    imageviewer.h
    insertfxpopup.h
    iocommand.h
    keyframedata.h
    keyframeselection.h
    keyframemover.h
    levelcreatepopup.h
    levelsettingspopup.h
    linesfadepopup.h
    linetestcapturepane.h
    linetestpane.h
    linetestviewer.h
    loadfolderpopup.h
    magpiefileimportpopup.h
    mainwindow.h
    matchline.h
    menubar.h
    menubarpopup.h
    menubarcommandids.h
    meshifypopup.h
    messagepanel.h
    moviegenerator.h
    onionskinmaskgui.h
    outputsettingspopup.h
    overwritepopup.h
    pane.h
    pltgizmopopup.h
    preferencespopup.h
    previewer.h
    previewfxmanager.h
    projectpopup.h
    psdsettingspopup.h
    renumberpopup.h
    reslist.h
    ruler.h
    savepresetpopup.h
    scanlist.h
    scanpopup.h
    scenesettingspopup.h
    sceneviewer.h
    sceneviewercontextmenu.h
    scriptconsolepanel.h
    selectionutils.h
    shifttracetool.h
    shortcutpopup.h
    subcameramanager.h
    subscenecommand.h
    svncleanupdialog.h
    svncommitdialog.h
    svndeletedialog.h
    svnlockdialog.h
    svnlockframerangedialog.h
    svnrevertdialog.h
    svnupdateandlockdialog.h
    svnupdatedialog.h
    svnpurgedialog.h
    tapp.h
    tasksviewer.h
    testpanel.h
    tfarmstuff.h
    timestretchpopup.h
    toolbar.h
    tpanels.h
    trackerpopup.h
    vectorizerpopup.h
    vectorizerswatch.h
    versioncontrol.h
    versioncontrolgui.h
    versioncontroltimeline.h
    versioncontrolxmlreader.h
    versioncontrolwidget.h
    viewerdraw.h
    viewerpane.h
    viewerpopup.h
    xshcellmover.h
    xshcellviewer.h
    xshcolumnviewer.h
    xsheetdragtool.h
    xsheetviewer.h
    xshnoteviewer.h
    xshrowviewer.h
    comboviewerpane.h
    historypane.h
    cleanupsettingspane.h
# Tracker file
    dummyprocessor.h
    metnum.h
    ObjectTracker.h
    predict3d.h
    processor.h
    viewer.h
    writer.h)

set(HEADERS ${MOC_HEADERS})

set(SOURCES
    floatingpanelcommand.cpp
    canvassizepopup.cpp
    history.cpp
    loadfoldercommand.cpp
    loadfolderpopup.cpp
    main.cpp
    mainwindow.cpp
    matchline.cpp
    menubar.cpp
    menubarpopup.cpp
    mergecolumns.cpp
    moviegenerator.cpp
    pane.cpp
    previewer.cpp
    previewfxmanager.cpp
    ruler.cpp
    scanlist.cpp
    tapp.cpp
    toolbar.cpp
    batchserversviewer.cpp
    castviewer.cpp
    exportlevelpopup.cpp
    exportscenepopup.cpp
    colormodelviewer.cpp
    exportpanel.cpp
    filebrowser.cpp
    filebrowserversioncontrol.cpp
    filmstrip.cpp
    matchlinecommand.cpp
    messagepanel.cpp
    tasksviewer.cpp
    testpanel.cpp
    tpanels.cpp
    xshcellmover.cpp
    xshcellviewer.cpp
    xshcolumnviewer.cpp
    xsheetdragtool.cpp
    xsheetviewer.cpp
    xshnoteviewer.cpp
    xshrowviewer.cpp
    sceneviewer.cpp
    sceneviewerevents.cpp
    viewerdraw.cpp
    viewerpane.cpp
    castselection.cpp
    cellselection.cpp
    columnselection.cpp
    fileselection.cpp
    filmstripselection.cpp
    keyframeselection.cpp
    addfilmstripframespopup.cpp
    camerasettingspopup.cpp
    convertpopup.cpp
    duplicatepopup.cpp
    dvdirtreeview.cpp
    filebrowserpopup.cpp
    formatsettingspopups.cpp
    fxparameditorpopup.cpp
    histogrampopup.cpp
    insertfxpopup.cpp
    levelcreatepopup.cpp
    levelsettingspopup.cpp
    linetestcapturepane.cpp
    linetestpane.cpp
    linetestviewer.cpp
    magpiefileimportpopup.cpp
    outputsettingspopup.cpp
    overwritepopup.cpp
    pltgizmopopup.cpp
    preferencespopup.cpp
    projectpopup.cpp
    psdsettingspopup.cpp
    renumberpopup.cpp
    runscriptcommand.cpp
    savepresetpopup.cpp
    scanpopup.cpp
    sceneviewercontextmenu.cpp
    scenesettingspopup.cpp
    scriptconsolepanel.cpp
    shifttracetool.cpp
    shortcutpopup.cpp
    subcameramanager.cpp
    timestretchpopup.cpp
    trackerpopup.cpp
    vectorizerpopup.cpp
    vectorizerswatch.cpp
    versioncontrol.cpp
    versioncontroltimeline.cpp
    versioncontrolxmlreader.cpp
    versioncontrolwidget.cpp
    viewerpopup.cpp
    cellselectioncommand.cpp
    columncommand.cpp
    filmstripcommand.cpp
    iocommand.cpp
    levelcommand.cpp
    print.cpp
    subscenecommand.cpp
    svncleanupdialog.cpp
    svncommitdialog.cpp
    svndeletedialog.cpp
    svnlockdialog.cpp
    svnlockframerangedialog.cpp
    svnrevertdialog.cpp
    svnupdateandlockdialog.cpp
    svnupdatedialog.cpp
    svnpurgedialog.cpp
    vcrcommand.cpp
    xsheetcmd.cpp
    celldata.cpp
    drawingdata.cpp
    filedata.cpp
    imageviewer.cpp
    keyframedata.cpp
    dvitemview.cpp
    dvwidgets.cpp
    flipbook.cpp
    frameheadgadget.cpp
    onionskinmaskgui.cpp
    batches.cpp
    tfarmstuff.cpp
    cleanuppaletteviewer.cpp
    cleanuppopup.cpp
    cleanuppreview.cpp
    cleanupsettingsmodel.cpp
    cleanupsettingspopup.cpp
    cleanupswatch.cpp
    cellkeyframedata.cpp
    cellkeyframeselection.cpp
    keyframemover.cpp
    cachefxcommand.cpp
    exportlevelcommand.cpp
    adjustlevelspopup.cpp
    adjustthicknesspopup.cpp
    binarizepopup.cpp
    brightnessandcontrastpopup.cpp
    linesfadepopup.cpp
    rendercommand.cpp
    curveio.cpp
    meshifypopup.cpp
    selectionutils.cpp
    antialiaspopup.cpp
    mergecmapped.cpp
    comboviewerpane.cpp
    historypane.cpp
    cleanupsettingspane.cpp
# Tracker file
    dummyprocessor.cpp
    metnum.cpp
    ObjectTracker.cpp
    predict3d.cpp
    viewer.cpp
    writer.cpp)

add_translation(toonz ${HEADERS} ${SOURCES})

set(OBJCSOURCES filebrowsermodel.cpp)

# Resource file(s)
set(RESOURCES toonz.qrc)

if(WIN32)
    set(RC_FILE toonz.rc)
endif()

qt5_add_resources(SOURCES ${RESOURCES})

if(APPLE)
    #set_source_files_properties(${OBJCSOURCES} PROPERTIES LANGUAGE OBJCXX)
    set_source_files_properties(${OBJCSOURCES} PROPERTIES COMPILE_FLAGS "-x objective-c++")
endif()

qt5_wrap_cpp(SOURCES ${MOC_HEADERS})

if(WIN32)
    add_executable(OpenToonz_${VERSION} WIN32 ${HEADERS} ${SOURCES} ${OBJCSOURCES} ${RESOURCES} ${RC_FILE})
else()
    set(MACOSX_BUNDLE_ICON_FILE "OpenToonz.icns")
    set_source_files_properties(OpenToonz.icns PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
    add_executable(OpenToonz_${VERSION} MACOSX_BUNDLE OpenToonz.icns ${HEADERS} ${SOURCES} ${OBJCSOURCES} ${RESOURCES} ${RC_FILE})
    #add_definitions(-DTNZBASE_EXPORTS -DTPARAM_EXPORTS -DTXSHEET_EXPORTS -DTFX_EXPORTS -DTAPPTOOLS_EXPORTS -DTTEST_EXPORTS)
endif()

if(WIN32)
    add_definitions(-DUNICODE -D_UNICODE)
endif()

message("subdir: toonz")
message("Bin: " ${CMAKE_CURRENT_BINARY_DIR})
#message("Lib: " ${LIBRARY_OUTPUT_PATH})
#message("Exe: " ${EXECUTABLE_OUTPUT_PATH})
#message("Sources:" ${SOURCES})

include_directories(../../sources/toonzfarm/include)
include_directories(../../sources/toonzqt)
if(WIN32)
    if(PLATFORM EQUAL 32)
        include_directories(${SDKROOT}/quicktime/QT73SDK/CIncludes)
    endif()
endif()

if(WIN32)
    target_link_libraries(OpenToonz_${VERSION}
        Qt5::WinMain Qt5::Core Qt5::Gui Qt5::Network Qt5::OpenGL Qt5::Svg Qt5::Xml Qt5::Script Qt5::Widgets Qt5::PrintSupport
        ${GL_LIB} ${GLUT_LIB}
        tnzcore tnzbase toonzlib colorfx tnzext image sound toonzqt tnztools tnzstdfx tfarm)
else()
    find_library(COCOA_LIB Cocoa)

    _find_toonz_library(EXTRA_LIBS "tnzcore;tnzbase;toonzlib;colorfx;tnzext;image;sound;toonzqt;tnztools")

    # 変なところにライブラリ生成するカスども
    set(EXTRA_LIBS ${EXTRA_LIBS} "$<TARGET_FILE:tnzstdfx>" "$<TARGET_FILE:tfarm>")

    target_link_libraries(OpenToonz_${VERSION} Qt5::Core Qt5::Gui Qt5::Network Qt5::OpenGL Qt5::Svg Qt5::Xml Qt5::Script Qt5::Widgets Qt5::PrintSupport ${GL_LIB} ${GLUT_LIB} ${COCOA_LIB} ${EXTRA_LIBS})

endif()

if(APPLE)
    # CMAKE_RUNTIME_OUTPUT_DIRECTORY should be equivalent to usage on windows despite empty
    # OSX だと CMAKE_RUNTIME_OUTPUT_DIRECTORY が空だが Windows 版と同じ使い方ができるようにしておく
    get_target_property(bin OpenToonz_${VERSION} LOCATION)
    get_filename_component(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${bin} DIRECTORY)
endif()

# copy utilty executables onto the directory after build
if(LZODRIVER_FOUND)
    add_custom_command(TARGET OpenToonz_${VERSION} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:lzocompress> ${CMAKE_RUNTIME_OUTPUT_DIRECTORY} DEPENDS lzocompress)
    add_custom_command(TARGET OpenToonz_${VERSION} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:lzodecompress> ${CMAKE_RUNTIME_OUTPUT_DIRECTORY} DEPENDS lzodecompress)
endif()

if(APPLE)
    get_target_property(loc OpenToonz_${VERSION} MACOSX_BUNDLE_NAME)
    message(" ==> App Bundle: " ${loc})
    message(" ==> Mach-o: " ${bin})
    foreach (lib ${EXTRA_LIBS})
        message(" copy:" ${lib} "==>" ${CMAKE_CURRENT_BINARY_DIR}/OpenToonz_${VERSION}.app/Contents/MacOS/)
        add_custom_command(TARGET OpenToonz_${VERSION} POST_BUILD COMMAND cp ${lib} ${CMAKE_CURRENT_BINARY_DIR}/OpenToonz_${VERSION}.app/Contents/MacOS/)
    endforeach ()

    add_custom_command(TARGET OpenToonz_${VERSION}
        POST_BUILD COMMAND
        ${CMAKE_INSTALL_NAME_TOOL} -add_rpath @executable_path/. ${bin})
    add_custom_command(TARGET OpenToonz_${VERSION}
        POST_BUILD COMMAND
        ${CMAKE_INSTALL_NAME_TOOL} -add_rpath ${CMAKE_INSTALL_RPATH} ${bin})
    add_custom_command(TARGET OpenToonz_${VERSION} POST_BUILD COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/OpenToonz_${VERSION}.app/Contents/Resources)

    add_custom_command(TARGET OpenToonz_${VERSION} POST_BUILD COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/OpenToonz_${VERSION}.app/Contents/Resources)
    add_custom_command(TARGET OpenToonz_${VERSION} POST_BUILD COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/qss)

    add_custom_command(TARGET OpenToonz_${VERSION} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy ../../install/SystemVar.ini ${CMAKE_CURRENT_BINARY_DIR}/OpenToonz_${VERSION}.app/Contents/Resources)
endif()


