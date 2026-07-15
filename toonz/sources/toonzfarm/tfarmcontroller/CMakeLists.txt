include_directories(
    ../include
)

add_executable(tfarmcontroller
    tfarmcontroller.cpp
)

target_link_libraries(tfarmcontroller
    ${QT_CORE_LINK_TARGETS}
    tfarm
)
