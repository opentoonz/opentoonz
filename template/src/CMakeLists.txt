set(FRAMEWORK_SOURCES
    framework/docklayout.cpp
    framework/dockwidget.cpp
    framework/tdockwindows.cpp
    framework/pane.cpp
    framework/mainwindow.cpp
    framework/menubar.cpp
    framework/menubarcommand.cpp
    framework/appcontext.cpp
    framework/floatingpanelcommand.cpp
    framework/thememanager.cpp
    framework/preferences/checkbox.cpp
    framework/preferences/lineedit.cpp
    framework/preferences/intfield.cpp
    framework/preferences/doublefield.cpp
    framework/preferences/filefield.cpp
    framework/preferences/dvdialog.cpp
    framework/preferences/prefsystem.cpp
    framework/preferences/prefpopup.cpp
)

set(PANEL_SOURCES
    panels/register.cpp
    panels/logpanel.cpp
    panels/propertypanel.cpp
    panels/canvaspanel.cpp
    panels/commandpalette.cpp
    panels/welcomepanel.cpp
    panels/combo/comboviewerpane.cpp
    panels/combo/ruler.cpp
    panels/design/layerspanel.cpp
    panels/design/propertiespanel.cpp
    panels/design/designtoolbar.cpp
)

qt_add_executable(yz-ui-template
    main.cpp
    ${FRAMEWORK_SOURCES}
    ${PANEL_SOURCES}
    ${CMAKE_SOURCE_DIR}/resources/resources.qrc
    ${CMAKE_SOURCE_DIR}/resources/translations.qrc
)

target_include_directories(yz-ui-template PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(yz-ui-template PRIVATE
    Qt6::Widgets
    Qt6::Core
    Qt6::Gui
    Qt6::Svg
    Qt6::Network
    Qt6::Sql
    spdlog::spdlog
    nlohmann_json::nlohmann_json
)
