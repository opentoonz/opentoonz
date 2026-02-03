include_directories(
    ../include
)

add_executable(tfarmserver
    tfarmserver.cpp
)

target_link_libraries(tfarmserver
    Qt5::Core
    tfarm
)
