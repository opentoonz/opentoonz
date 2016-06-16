set(HEADERS
    ../include/ext/ContextStatus.h
    ../include/ext/ExtUtil.h
    ../include/ext/Selector.h
    ../include/ext/StrokeParametricDeformer.h
    ../include/ext/Types.h
    ../include/ext/Designer.h
    ../include/ext/OverallDesigner.h
    ../include/ext/CornerDeformation.h
    ../include/ext/SmoothDeformation.h
    ../include/ext/StraightCornerDeformation.h
    ../include/ext/StrokeDeformation.h
    ../include/ext/StrokeDeformationImpl.h
    ../include/ext/LinearPotential.h
    ../include/ext/NotSimmetricBezierPotential.h
    ../include/ext/NotSimmetricExpPotential.h
    ../include/ext/Potential.h
    ../include/ext/SquarePotential.h
    DeformationSelector.h
    ../include/ext/meshbuilder.h
    ../include/ext/meshtexturizer.h
    ../include/ext/meshutils.h
    ../include/ext/plasticdeformer.h
    ../include/ext/plasticdeformerstorage.h
    ../include/ext/plastichandle.h
    ../include/ext/plasticskeleton.h
    ../include/ext/plasticskeletondeformation.h
    ../include/ext/plasticvisualsettings.h
    ../include/ext/ttexturesstorage.h
    ../include/tlin/cblas.h
    ../include/tlin/tlin.h
    ../include/tlin/tlin_basicops.h
    ../include/tlin/tlin_cblas_wrap.h
    ../include/tlin/tlin_matrix.h
    ../include/tlin/tlin_sparsemat.h
    ../include/tlin/tlin_superlu_wrap.h
    ../include/tlin/tlin_vector.h)

set(SOURCES
    ContextStatus.cpp
    DeformationSelector.cpp
    ExtUtil.cpp
    Selector.cpp
    StrokeParametricDeformer.cpp
    Types.cpp
    LinearPotential.cpp
    NotSimmetricBezierPotential.cpp
    NotSimmetricExpPotential.cpp
    Potential.cpp
    SquarePotential.cpp
    CornerDeformation.cpp
    SmoothDeformation.cpp
    StraightCornerDeformation.cpp
    StrokeDeformation.cpp
    StrokeDeformationImpl.cpp
    Designer.cpp
    OverallDesigner.cpp
    meshbuilder.cpp
    meshtexturizer.cpp
    meshutils.cpp
    plasticdeformer.cpp
    plasticdeformerstorage.cpp
    plastichandle.cpp
    plasticskeleton.cpp
    plasticskeletondeformation.cpp
    ttexturesstorage.cpp
    tlin/tlin_cblas_wrap.cpp
    tlin/tlin_superlu_wrap.cpp)

if(WIN32 AND PLATFORM EQUAL 32)
    # can not link libopenblas_32.lib with SAFESEH
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /SAFESEH:NO")
endif()

add_library(tnzext SHARED ${HEADERS} ${SOURCES} ${OBJCSOURCES})

if(APPLE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wl,-install_name,@rpath/libtnzext.dylib")
endif()

add_definitions(-DTNZEXT_EXPORTS)

message("subdir: tnzext")
message("Bin: " ${CMAKE_CURRENT_BINARY_DIR})

if(APPLE)
    find_library(ACCE_LIB Accelerate)
endif()

include_directories(${SDKROOT}/superlu/SuperLU_4.1/SRC)

if(WIN32)
    target_link_libraries(tnzext
        Qt5::Core Qt5::Gui Qt5::OpenGL Qt5::Network
        ${GLUT_LIB} ${GL_LIB} ${SUPERLU_LIB} ${OPENBLAS_LIB} ${EXTRA_LIBS}
        tnzcore tnzbase)
elseif(APPLE)
    set(EXTRA_LIBS
        ${CMAKE_CURRENT_BINARY_DIR}/../tnzcore/libtnzcore.dylib
        ${CMAKE_CURRENT_BINARY_DIR}/../tnzbase/libtnzbase.dylib)

    target_link_libraries(tnzext
        Qt5::Core Qt5::Gui Qt5::OpenGL Qt5::Network
        ${ACCE_LIB} ${GLUT_LIB} ${GL_LIB} ${SUPERLU_LIB} ${EXTRA_LIBS})
elseif(UNIX)
    _find_toonz_library(EXTRA_LIBS "tnzcore;tnzbase")

    set(EXTRA_LIBS ${EXTRA_LIBS} ${OPENBLAS_LIB})

    target_link_libraries(tnzext
        Qt5::Core Qt5::Gui Qt5::OpenGL Qt5::Network
        ${GLUT_LIB} ${GL_LIB} ${SUPERLU_LIB} ${EXTRA_LIBS})
endif()
