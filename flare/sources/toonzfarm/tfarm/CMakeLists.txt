set(HEADERS
    ../include/service.h
    ../../include/tfarmcontroller.h
    ../include/tfarmexecutor.h
    ../../include/tfarmplatforms.h
    ../include/tfarmproxy.h
    ../../include/tfarmserver.h
    ../../include/tfarmtask.h
    ../include/tlog.h
)

set(SOURCES
    service.cpp
    tfarmcontroller_c.cpp
    tfarmexecutor.cpp
    tfarmproxy.cpp
    tfarmserver_c.cpp
    tfarmserverstub.cpp
    tfarmtask.cpp
    tlog.cpp
    ttcpipclient.cpp
    ttcpipserver.cpp
)

add_library(tfarm SHARED ${HEADERS} ${SOURCES})
add_definitions(
    -DTFARM_EXPORTS
)

if(BUILD_ENV_APPLE)
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-install_name,@executable_path/libtfarm.dylib")
endif()

message("subdir: tfarm")
message("Bin: " ${CMAKE_CURRENT_BINARY_DIR})

include_directories(
    ../include
)

if(BUILD_ENV_MSVC)
    set(EXTRA_LIBS
        tnzcore tnzbase toonzlib
        ws2_32.lib
    )
elseif(BUILD_ENV_APPLE)
    set(EXTRA_LIBS
	    "$<TARGET_FILE:tnzcore>"
	    "$<TARGET_FILE:tnzbase>"
	    "$<TARGET_FILE:toonzlib>"
    )
    add_dependencies(tfarm tnzcore tnzbase toonzlib tnzext)
elseif(BUILD_ENV_UNIXLIKE)
    _find_toonz_library(EXTRA_LIBS "tnzcore;tnzbase;toonzlib;tnzext")
    if(BUILD_TARGET_WIN)
        set(EXTRA_LIBS ${EXTRA_LIBS} -lws2_32)
    endif()
endif()
target_link_libraries(tfarm
    Qt5::Core Qt5::Gui Qt5::OpenGL Qt5::Network
    ${GLUT_LIB} ${GL_LIB} ${EXTRA_LIBS}
)
