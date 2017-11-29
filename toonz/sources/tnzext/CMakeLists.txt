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
    ../include/ext/NotSymmetricBezierPotential.h
    ../include/ext/NotSymmetricExpPotential.h
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
    ../include/tlin/tlin_vector.h
)

set(SOURCES
    ContextStatus.cpp
    DeformationSelector.cpp
    ExtUtil.cpp
    Selector.cpp
    StrokeParametricDeformer.cpp
    Types.cpp
    LinearPotential.cpp
    NotSymmetricBezierPotential.cpp
    NotSymmetricExpPotential.cpp
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
    tlin/tlin_superlu_wrap.cpp
)

set(OBJCSOURCES
)

if(BUILD_ENV_MSVC AND PLATFORM EQUAL 32)
    # can not link libopenblas_32.lib with SAFESEH
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /SAFESEH:NO")
endif()

add_library(tnzext SHARED ${HEADERS} ${SOURCES} ${OBJCSOURCES})

if(BUILD_ENV_APPLE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wl,-install_name,@rpath/libtnzext.dylib")
    add_dependencies(tnzext tnzcore tnzbase)
endif()

add_definitions(
    -DTNZEXT_EXPORTS
)

message("subdir: tnzext")
message("Bin: " ${CMAKE_CURRENT_BINARY_DIR})

if(BUILD_ENV_APPLE)
    find_library(ACCE_LIB Accelerate)
endif()

if(BUILD_ENV_MSVC OR BUILD_ENV_APPLE)
    # Warning, this looks wrong,
    # should _only_ use SUPERLU_INCLUDE_DIR, for now do this with Linux.
    include_directories(
        SYSTEM
        ${SDKROOT}/superlu/SuperLU_4.1/SRC
    )
else()
    include_directories(
        SYSTEM
        ${SUPERLU_INCLUDE_DIR}
    )
endif()

if(BUILD_ENV_MSVC)
    target_link_libraries(tnzext
        Qt5::Core Qt5::Gui Qt5::OpenGL Qt5::Network
        ${GLUT_LIB} ${GL_LIB} ${SUPERLU_LIB} ${OPENBLAS_LIB} ${EXTRA_LIBS}
        tnzcore tnzbase
    )
elseif(BUILD_ENV_APPLE)
    set(EXTRA_LIBS
        ${CMAKE_CURRENT_BINARY_DIR}/../tnzcore/libtnzcore.dylib
        ${CMAKE_CURRENT_BINARY_DIR}/../tnzbase/libtnzbase.dylib
    )

    target_link_libraries(tnzext
        Qt5::Core Qt5::Gui Qt5::OpenGL Qt5::Network
        ${ACCE_LIB} ${GLUT_LIB} ${GL_LIB} ${SUPERLU_LIB} ${EXTRA_LIBS}
    )
elseif(BUILD_ENV_UNIXLIKE)
    _find_toonz_library(EXTRA_LIBS "tnzcore;tnzbase")

    target_link_libraries(tnzext
        Qt5::Core Qt5::Gui Qt5::OpenGL Qt5::Network
        ${GLUT_LIB} ${GL_LIB} ${SUPERLU_LIB} ${OPENBLAS_LIB} ${EXTRA_LIBS}
    )
endif()
