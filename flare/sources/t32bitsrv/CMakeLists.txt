add_executable(t32bitsrv
    main.cpp
    t323gpmsg.cpp
    t323gpmsg.h
    t32fontmsg.cpp
    t32fontmsg.h
    t32movmsg.cpp
    t32movmsg.h
)

if(BUILD_TARGET_WIN)
    include_directories(
        SYSTEM
        ${SDKROOT}/quicktime/QT73SDK/CIncludes
    )
endif()

target_link_libraries(t32bitsrv
    Qt5::Core
    Qt5::Network
    tnzcore
    image
)

if(BUILD_ENV_APPLE AND PLATFORM EQUAL 32)
    get_target_property(bin t32bitsrv LOCATION)
    get_filename_component(bindir ${bin} DIRECTORY)

    get_target_property(loc tnzcore LOCATION)
    add_custom_command(TARGET t32bitsrv POST_BUILD COMMAND cp ${loc} ${bindir})
    get_target_property(loc image LOCATION)
    add_custom_command(TARGET t32bitsrv POST_BUILD COMMAND cp ${loc} ${bindir})
    get_target_property(loc tnzbase LOCATION)
    add_custom_command(TARGET t32bitsrv POST_BUILD COMMAND cp ${loc} ${bindir})

    add_custom_command(TARGET t32bitsrv
        POST_BUILD COMMAND
        ${CMAKE_INSTALL_NAME_TOOL} -add_rpath @executable_path/. ${bin}
    )
endif()
