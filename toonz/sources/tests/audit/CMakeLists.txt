if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/svg_parser_regression.cpp")
    add_executable(svg_parser_regression svg_parser_regression.cpp)

    target_include_directories(svg_parser_regression PRIVATE ${CMAKE_SOURCE_DIR})

    target_link_libraries(svg_parser_regression
        Qt5::Core
        Qt5::Gui
        Qt5::Network
        Qt5::OpenGL
        Qt5::Svg
        Qt5::Xml
        Qt5::Script
        Qt5::Widgets
        Qt5::PrintSupport
        Qt5::Multimedia
        tnzcore
        tnzbase
        toonzlib
    )

    if(BUILD_ENV_APPLE)
        add_custom_command(
            TARGET svg_parser_regression POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:tnzcore>
                $<TARGET_FILE:tnzbase>
                $<TARGET_FILE:tnzext>
                $<TARGET_FILE:toonzlib>
                $<TARGET_FILE_DIR:svg_parser_regression>
            VERBATIM
        )
    endif()

    add_test(NAME svg_parser_regression COMMAND svg_parser_regression)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tzp_color_names_regression.c")
    add_executable(tzp_color_names_regression
        tzp_color_names_regression.c
        ${CMAKE_SOURCE_DIR}/image/tzp/avl.c
    )

    target_include_directories(tzp_color_names_regression PRIVATE ${CMAKE_SOURCE_DIR})

    add_test(NAME tzp_color_names_regression COMMAND tzp_color_names_regression)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tzl_header_regression.cpp")
    add_executable(tzl_header_regression tzl_header_regression.cpp)

    target_include_directories(tzl_header_regression PRIVATE ${CMAKE_SOURCE_DIR})

    target_link_libraries(tzl_header_regression
        Qt5::Core
        image
    )

    if(BUILD_ENV_APPLE)
        add_custom_command(
            TARGET tzl_header_regression POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:tnzcore>
                $<TARGET_FILE:tnzbase>
                $<TARGET_FILE:tnzext>
                $<TARGET_FILE:toonzlib>
                $<TARGET_FILE:image>
                $<TARGET_FILE_DIR:tzl_header_regression>
            VERBATIM
        )
    endif()

    add_test(NAME tzl_header_regression COMMAND tzl_header_regression)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/externfx_process_regression.cpp")
    add_executable(externfx_process_regression externfx_process_regression.cpp)

    target_include_directories(externfx_process_regression PRIVATE ${CMAKE_SOURCE_DIR})

    target_link_libraries(externfx_process_regression
        Qt5::Core
        tnzbase
        image
    )

    if(BUILD_ENV_APPLE)
        add_custom_command(
            TARGET externfx_process_regression POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:tnzcore>
                $<TARGET_FILE:tnzbase>
                $<TARGET_FILE:tnzext>
                $<TARGET_FILE:toonzlib>
                $<TARGET_FILE:image>
                $<TARGET_FILE_DIR:externfx_process_regression>
            VERBATIM
        )
    endif()

    add_test(NAME externfx_process_regression COMMAND externfx_process_regression)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty_empty_path_regression.cpp")
    add_executable(thirdparty_empty_path_regression thirdparty_empty_path_regression.cpp)

    target_include_directories(thirdparty_empty_path_regression PRIVATE ${CMAKE_SOURCE_DIR})

    target_link_libraries(thirdparty_empty_path_regression
        Qt5::Core
        toonzlib
    )

    if(BUILD_ENV_APPLE)
        add_custom_command(
            TARGET thirdparty_empty_path_regression POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:tnzcore>
                $<TARGET_FILE:tnzbase>
                $<TARGET_FILE:tnzext>
                $<TARGET_FILE:toonzlib>
                $<TARGET_FILE_DIR:thirdparty_empty_path_regression>
            VERBATIM
        )
    endif()

    add_test(NAME thirdparty_empty_path_regression COMMAND thirdparty_empty_path_regression)
endif()
