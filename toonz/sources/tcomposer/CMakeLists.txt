add_executable(tcomposer.bin
    tcomposer.cpp)

target_link_libraries(tcomposer.bin
    Qt5::Core
    Qt5::Gui
    Qt5::Widgets
    toonzlib
    tfarm
    tnzstdfx
    sound
    image
    colorfx
    toonzqt)
