add_executable(tconverter
    tconverter.cpp
)

target_link_libraries(tconverter
    Qt5::Core
    toonzlib
    image
)
