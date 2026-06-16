set(FRAMEWORK_SOURCES
    framework/docklayout.cpp
    framework/dockwidget.cpp
    framework/tdockwindows.cpp
    framework/pane.cpp
    framework/mainwindow.cpp
    framework/menubar.cpp
    framework/menubarcommand.cpp
    framework/appcontext.cpp
)

set(PANEL_SOURCES
    panels/register.cpp
    panels/logpanel.cpp
    panels/propertypanel.cpp
    panels/canvaspanel.cpp
    panels/commandpalette.cpp
    panels/welcomepanel.cpp
)

qt_add_executable(yz-ui-template
    main.cpp
    ${FRAMEWORK_SOURCES}
    ${PANEL_SOURCES}
)

target_include_directories(yz-ui-template PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(yz-ui-template PRIVATE
    Qt6::Widgets
    Qt6::Core
    Qt6::Gui
)
