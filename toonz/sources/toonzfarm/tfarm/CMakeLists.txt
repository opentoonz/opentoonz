set(HEADERS
    ../include/service.h
    ../../include/tfarmcontroller.h
    ../include/tfarmexecutor.h
    ../../include/tfarmplatforms.h
    ../include/tfarmproxy.h
    ../../include/tfarmserver.h
    ../../include/tfarmtask.h
    ../include/tlog.h)

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
    ttcpipserver.cpp)

add_library(tfarm SHARED ${HEADERS} ${SOURCES})
add_definitions(
    -DTFARM_EXPORTS
)

if(APPLE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wl,-install_name,@rpath/libtfarm.dylib")
endif()

message("subdir: tfarm")
message("Bin: " ${CMAKE_CURRENT_BINARY_DIR})

include_directories(
    ../include
)

if(WIN32)
    set(EXTRA_LIBS
        tnzcore tnzbase toonzlib
        ws2_32.lib)
elseif(APPLE)
    set(EXTRA_LIBS
        ${CMAKE_CURRENT_BINARY_DIR}/../../tnzcore/libtnzcore.dylib
        ${CMAKE_CURRENT_BINARY_DIR}/../../tnzbase/libtnzbase.dylib
        ${CMAKE_CURRENT_BINARY_DIR}/../../toonzlib/libtoonzlib.dylib)
elseif(UNIX)
    _find_toonz_library(EXTRA_LIBS "tnzcore;tnzbase;toonzlib;tnzext")
endif()

target_link_libraries(tfarm
    Qt5::Core Qt5::Gui Qt5::OpenGL Qt5::Network
    ${GLUT_LIB} ${GL_LIB} ${EXTRA_LIBS})
