add_executable(tcleanup
    tcleanupper.cpp
)

target_link_libraries(tcleanup
    ${QT_CORE_LINK_TARGETS}
    ${QT_WIDGETS_TARGET}
    tfarm
    image
)
