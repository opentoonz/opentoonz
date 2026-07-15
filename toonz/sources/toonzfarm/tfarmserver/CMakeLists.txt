include_directories(
    ../include
)

add_executable(tfarmserver
    tfarmserver.cpp
)

target_link_libraries(tfarmserver
    ${QT_CORE_LINK_TARGETS}
    tfarm
)
