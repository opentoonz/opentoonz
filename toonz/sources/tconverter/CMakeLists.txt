add_executable(tconverter.bin
    tconverter.cpp)

target_link_libraries(tconverter.bin
    Qt5::Core
    toonzlib
    image)
