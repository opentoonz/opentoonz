add_executable(tcomposer
    tcomposer.cpp
)

target_link_libraries(tcomposer
    ${QT_CORE_LINK_TARGETS}
    ${QT_GUI_TARGET}
    ${QT_WIDGETS_TARGET}
    toonzlib
    tfarm
    tnzstdfx
    sound
    image
    colorfx
    toonzqt
)
