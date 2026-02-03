add_executable(tcomposer
    tcomposer.cpp
)

target_link_libraries(tcomposer
    Qt5::Core
    Qt5::Gui
    Qt5::Widgets
    toonzlib
    tfarm
    tnzstdfx
    sound
    image
    colorfx
    toonzqt
)
