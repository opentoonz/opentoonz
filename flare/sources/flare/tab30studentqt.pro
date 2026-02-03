
TEMPLATE = app
TARGET = tab30student
DESTDIR = $(SOURCEROOT)/tab30qt
QT += opengl svg xml network
CONFIG += debug_and_release
DEFINES += MACOSX STUDENT $(processor)
release: DEFINES += NDEBUG
INCLUDEPATH += ./GeneratedFiles \
    ./GeneratedFiles/Debug \
    $(SOURCEROOT)/include \
    $(SOURCEROOT)/toonz \
    /depot/sdk/boost/boost_1_33_1/ \
    /depot/sdk/cryptopp/cryptopp5.5.1 \
    /Developer/Headers/FlatCarbon 
LIBS += -L"$(BINROOT)/bin" \
    -ltoonzlib \
    -ltnzbase \
    -ltnzcore \
    -ltnzext \
    -L/depot/sdk/boost/boost_1_33_1/lib_toonz/darwin/$(processor)/ \
    -lboost_thread-1_33_1 \
    -framework GLUT \
    -framework Quicktime \
    /depot/sdk/cryptopp/cryptopp5.5.1/libcryptopp_$(processor).a
DEPENDPATH += .
MOC_DIR += ./GeneratedFiles/debug
OBJECTS_DIR += debug
UI_DIR += ./GeneratedFiles
RCC_DIR += ./GeneratedFiles

# Quando avremo l'icona abilitare la riga seguente
ICON = tab30student.icns

include(tab30qt.pri)
