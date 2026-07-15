add_executable(tconverter
    tconverter.cpp
)

target_link_libraries(tconverter
    ${QT_CORE_LINK_TARGETS}
    toonzlib
    image
)
