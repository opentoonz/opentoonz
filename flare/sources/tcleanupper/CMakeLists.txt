add_executable(tcleanup
    tcleanupper.cpp
)

target_link_libraries(tcleanup
    Qt5::Core
    Qt5::Widgets
    tfarm
    image
)
