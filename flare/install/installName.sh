
#!/bin/sh

# TOONZ LIBRARIES

# RICORDARSI PRIMA DI COPIARE LE LIBRERIE IN Toonz\ 7.1.app/Contents/Frameworks

#echo /depot/angelo/toonz/main/binmacosxd

#tnzcore

install_name_tool -id @executable_path/../Frameworks/libtnzcore.1.dylib $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/libtnzcore.1.dylib 
install_name_tool -change libtnzcore.1.dylib @executable_path/../Frameworks/libtnzcore.1.dylib $BINROOT/bin/Toonz\ 7.1.app/Contents/MacOS/Toonz\ 7.1

# tnzcore dipende da QtGui QtCore QtOpenGl e QtNetwork

install_name_tool -change /usr/local/Trolltech/Qt-4.8.0/lib/QtGui.framework/Versions/4/QtGui @executable_path/../Frameworks/QtGui.framework/Versions/4/QtGui $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/libtnzcore.1.dylib
install_name_tool -change /usr/local/Trolltech/Qt-4.8.0/lib/QtCore.framework/Versions/4/QtCore @executable_path/../Frameworks/QtCore.framework/Versions/4/QtCore $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/libtnzcore.1.dylib
install_name_tool -change /usr/local/Trolltech/Qt-4.8.0/lib/QtOpenGL.framework/Versions/4/QtOpenGL @executable_path/../Frameworks/QtOpenGL.framework/Versions/4/QtOpenGL $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/libtnzcore.1.dylib
install_name_tool -change /usr/local/Trolltech/Qt-4.8.0/lib/QtNetwork.framework/Versions/4/QtNetwork @executable_path/../Frameworks/QtNetwork.framework/Versions/4/QtNetwork $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/libtnzcore.1.dylib


# tnzbase

install_name_tool -id @executable_path/../Frameworks/libtnzbase.1.dylib $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/libtnzbase.1.dylib
install_name_tool -change libtnzbase.1.dylib @executable_path/../Frameworks/libtnzbase.1.dylib $BINROOT/bin/Toonz\ 7.1.app/Contents/MacOS/Toonz\ 7.1

# tnzbase dipende da tnzcore QtGui QtCore QtOpenGl e QtNetwork

install_name_tool -change libtnzcore.1.dylib @executable_path/../Frameworks/libtnzcore.1.dylib $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/libtnzbase.1.dylib
install_name_tool -change /usr/local/Trolltech/Qt-4.8.0/lib/QtGui.framework/Versions/4/QtGui @executable_path/../Frameworks/QtGui.framework/Versions/4/QtGui $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/libtnzbase.1.dylib
install_name_tool -change /usr/local/Trolltech/Qt-4.8.0/lib/QtCore.framework/Versions/4/QtCore @executable_path/../Frameworks/QtCore.framework/Versions/4/QtCore $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/libtnzbase.1.dylib
install_name_tool -change /usr/local/Trolltech/Qt-4.8.0/lib/QtOpenGL.framework/Versions/4/QtOpenGL @executable_path/../Frameworks/QtOpenGL.framework/Versions/4/QtOpenGL $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/libtnzbase.1.dylib
install_name_tool -change /usr/local/Trolltech/Qt-4.8.0/lib/QtNetwork.framework/Versions/4/QtNetwork @executable_path/../Frameworks/QtNetwork.framework/Versions/4/QtNetwork $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/libtnzbase.1.dylib


# toonzlib

install_name_tool -id @executable_path/../Frameworks/libtoonzlib.1.dylib $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/libtoonzlib.1.dylib 
install_name_tool -change libtoonzlib.1.dylib @executable_path/../Frameworks/libtoonzlib.1.dylib $BINROOT/bin/Toonz\ 7.1.app/Contents/MacOS/Toonz\ 7.1

# toonzlib dipende da tnzbase tnzcore tnzext QtGui QtCore QtOpenGl QtNetwork e QtScript

install_name_tool -change libtnzbase.1.dylib @executable_path/../Frameworks/libtnzbase.1.dylib $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/libtoonzlib.1.dylib
install_name_tool -change libtnzcore.1.dylib @executable_path/../Frameworks/libtnzcore.1.dylib $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/libtoonzlib.1.dylib
install_name_tool -change libtnzext.1.dylib @executable_path/../Frameworks/libtnzext.1.dylib $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/libtoonzlib.1.dylib
install_name_tool -change /usr/local/Trolltech/Qt-4.8.0/lib/QtGui.framework/Versions/4/QtGui @executable_path/../Frameworks/QtGui.framework/Versions/4/QtGui $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/libtoonzlib.1.dylib
install_name_tool -change /usr/local/Trolltech/Qt-4.8.0/lib/QtCore.framework/Versions/4/QtCore @executable_path/../Frameworks/QtCore.framework/Versions/4/QtCore $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/libtoonzlib.1.dylib
install_name_tool -change /usr/local/Trolltech/Qt-4.8.0/lib/QtOpenGL.framework/Versions/4/QtOpenGL @executable_path/../Frameworks/QtOpenGL.framework/Versions/4/QtOpenGL $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/libtoonzlib.1.dylib
install_name_tool -change /usr/local/Trolltech/Qt-4.8.0/lib/QtNetwork.framework/Versions/4/QtNetwork @executable_path/../Frameworks/QtNetwork.framework/Versions/4/QtNetwork $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/libtoonzlib.1.dylib
install_name_tool -change /usr/local/Trolltech/Qt-4.8.0/lib/QtScript.framework/Versions/4/QtScript @executable_path/../Frameworks/QtScript.framework/Versions/4/QtScript $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/libtoonzlib.1.dylib


# image

install_name_tool -id @executable_path/../Frameworks/libimage.1.dylib $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/libimage.1.dylib 
install_name_tool -change libimage.1.dylib @executable_path/../Frameworks/libimage.1.dylib $BINROOT/bin/Toonz\ 7.1.app/Contents/MacOS/Toonz\ 7.1

# image dipende da tnzbase tnzcore QtGui QtCore QtOpenGl e QtNetwork

install_name_tool -change libtnzbase.1.dylib @executable_path/../Frameworks/libtnzbase.1.dylib $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/libimage.1.dylib
install_name_tool -change libtnzcore.1.dylib @executable_path/../Frameworks/libtnzcore.1.dylib $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/libimage.1.dylib
install_name_tool -change /usr/local/Trolltech/Qt-4.8.0/lib/QtGui.framework/Versions/4/QtGui @executable_path/../Frameworks/QtGui.framework/Versions/4/QtGui $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/libimage.1.dylib
install_name_tool -change /usr/local/Trolltech/Qt-4.8.0/lib/QtCore.framework/Versions/4/QtCore @executable_path/../Frameworks/QtCore.framework/Versions/4/QtCore $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/libimage.1.dylib
install_name_tool -change /usr/local/Trolltech/Qt-4.8.0/lib/QtOpenGL.framework/Versions/4/QtOpenGL @executable_path/../Frameworks/QtOpenGL.framework/Versions/4/QtOpenGL $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/libimage.1.dylib
install_name_tool -change /usr/local/Trolltech/Qt-4.8.0/lib/QtNetwork.framework/Versions/4/QtNetwork @executable_path/../Frameworks/QtNetwork.framework/Versions/4/QtNetwork $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/libimage.1.dylib


# sound

install_name_tool -id @executable_path/../Frameworks/libsound.1.dylib $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/libsound.1.dylib 
install_name_tool -change libsound.1.dylib @executable_path/../Frameworks/libsound.1.dylib $BINROOT/bin/Toonz\ 7.1.app/Contents/MacOS/Toonz\ 7.1

# sound dipende da tnzcore QtGui QtCore QtOpenGl e QtNetwork

install_name_tool -change libtnzcore.1.dylib @executable_path/../Frameworks/libtnzcore.1.dylib $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/libsound.1.dylib
install_name_tool -change /usr/local/Trolltech/Qt-4.8.0/lib/QtGui.framework/Versions/4/QtGui @executable_path/../Frameworks/QtGui.framework/Versions/4/QtGui $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/libsound.1.dylib
install_name_tool -change /usr/local/Trolltech/Qt-4.8.0/lib/QtCore.framework/Versions/4/QtCore @executable_path/../Frameworks/QtCore.framework/Versions/4/QtCore $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/libsound.1.dylib
install_name_tool -change /usr/local/Trolltech/Qt-4.8.0/lib/QtOpenGL.framework/Versions/4/QtOpenGL @executable_path/../Frameworks/QtOpenGL.framework/Versions/4/QtOpenGL $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/libsound.1.dylib
install_name_tool -change /usr/local/Trolltech/Qt-4.8.0/lib/QtNetwork.framework/Versions/4/QtNetwork @executable_path/../Frameworks/QtNetwork.framework/Versions/4/QtNetwork $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/libsound.1.dylib


# tnzext

install_name_tool -id @executable_path/../Frameworks/libtnzext.1.dylib $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/libtnzext.1.dylib 
install_name_tool -change libtnzext.1.dylib @executable_path/../Frameworks/libtnzext.1.dylib $BINROOT/bin/Toonz\ 7.1.app/Contents/MacOS/Toonz\ 7.1

# tnzext dipende da tnzcore tnzbase QtGui QtCore QtOpenGl e QtNetwork

install_name_tool -change libtnzcore.1.dylib @executable_path/../Frameworks/libtnzcore.1.dylib $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/libtnzext.1.dylib
install_name_tool -change libtnzbase.1.dylib @executable_path/../Frameworks/libtnzbase.1.dylib $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/libtnzext.1.dylib
install_name_tool -change /usr/local/Trolltech/Qt-4.8.0/lib/QtGui.framework/Versions/4/QtGui @executable_path/../Frameworks/QtGui.framework/Versions/4/QtGui $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/libtnzext.1.dylib
install_name_tool -change /usr/local/Trolltech/Qt-4.8.0/lib/QtCore.framework/Versions/4/QtCore @executable_path/../Frameworks/QtCore.framework/Versions/4/QtCore $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/libtnzext.1.dylib
install_name_tool -change /usr/local/Trolltech/Qt-4.8.0/lib/QtOpenGL.framework/Versions/4/QtOpenGL @executable_path/../Frameworks/QtOpenGL.framework/Versions/4/QtOpenGL $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/libtnzext.1.dylib
install_name_tool -change /usr/local/Trolltech/Qt-4.8.0/lib/QtNetwork.framework/Versions/4/QtNetwork @executable_path/../Frameworks/QtNetwork.framework/Versions/4/QtNetwork $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/libtnzext.1.dylib


# tnzstdfx

install_name_tool -id @executable_path/../Frameworks/libtnzstdfx.1.dylib $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/libtnzstdfx.1.dylib 
install_name_tool -change libtnzstdfx.1.dylib @executable_path/../Frameworks/libtnzstdfx.1.dylib $BINROOT/bin/Toonz\ 7.1.app/Contents/MacOS/Toonz\ 7.1
install_name_tool -id @executable_path/../Frameworks/libGLEW.dylib $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/libtnzstdfx.1.dylib 
install_name_tool -change /usr/lib/libGLEW.1.9.0.dylib @executable_path/../Frameworks/libGLEW.dylib $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/libtnzstdfx.1.dylib

# image dipende da tnzbase tnzcore toonzlib QtGui QtCore QtOpenGl e QtNetwork

install_name_tool -change libtnzbase.1.dylib @executable_path/../Frameworks/libtnzbase.1.dylib $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/libtnzstdfx.1.dylib
install_name_tool -change libtnzcore.1.dylib @executable_path/../Frameworks/libtnzcore.1.dylib $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/libtnzstdfx.1.dylib
install_name_tool -change libtoonzlib.1.dylib @executable_path/../Frameworks/libtoonzlib.1.dylib $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/libtnzstdfx.1.dylib
install_name_tool -change /usr/local/Trolltech/Qt-4.8.0/lib/QtGui.framework/Versions/4/QtGui @executable_path/../Frameworks/QtGui.framework/Versions/4/QtGui $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/libtnzstdfx.1.dylib
install_name_tool -change /usr/local/Trolltech/Qt-4.8.0/lib/QtCore.framework/Versions/4/QtCore @executable_path/../Frameworks/QtCore.framework/Versions/4/QtCore $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/libtnzstdfx.1.dylib
install_name_tool -change /usr/local/Trolltech/Qt-4.8.0/lib/QtOpenGL.framework/Versions/4/QtOpenGL @executable_path/../Frameworks/QtOpenGL.framework/Versions/4/QtOpenGL $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/libtnzstdfx.1.dylib
install_name_tool -change /usr/local/Trolltech/Qt-4.8.0/lib/QtNetwork.framework/Versions/4/QtNetwork @executable_path/../Frameworks/QtNetwork.framework/Versions/4/QtNetwork $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/libtnzstdfx.1.dylib


# colorfx

install_name_tool -id @executable_path/../Frameworks/libcolorfx.1.dylib $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/libcolorfx.1.dylib 
install_name_tool -change libcolorfx.1.dylib @executable_path/../Frameworks/libcolorfx.1.dylib $BINROOT/bin/Toonz\ 7.1.app/Contents/MacOS/Toonz\ 7.1

# colorfx dipende da tnzbase tnzcore toonzlib QtGui QtCore QtOpenGl e QtNetwork

install_name_tool -change libtnzbase.1.dylib @executable_path/../Frameworks/libtnzbase.1.dylib $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/libcolorfx.1.dylib
install_name_tool -change libtnzcore.1.dylib @executable_path/../Frameworks/libtnzcore.1.dylib $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/libcolorfx.1.dylib
install_name_tool -change /usr/local/Trolltech/Qt-4.8.0/lib/QtGui.framework/Versions/4/QtGui @executable_path/../Frameworks/QtGui.framework/Versions/4/QtGui $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/libcolorfx.1.dylib
install_name_tool -change /usr/local/Trolltech/Qt-4.8.0/lib/QtCore.framework/Versions/4/QtCore @executable_path/../Frameworks/QtCore.framework/Versions/4/QtCore $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/libcolorfx.1.dylib
install_name_tool -change /usr/local/Trolltech/Qt-4.8.0/lib/QtOpenGL.framework/Versions/4/QtOpenGL @executable_path/../Frameworks/QtOpenGL.framework/Versions/4/QtOpenGL $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/libcolorfx.1.dylib
install_name_tool -change /usr/local/Trolltech/Qt-4.8.0/lib/QtNetwork.framework/Versions/4/QtNetwork @executable_path/../Frameworks/QtNetwork.framework/Versions/4/QtNetwork $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/libcolorfx.1.dylib


# tfarm

install_name_tool -id @executable_path/../Frameworks/libtfarm.1.dylib $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/libtfarm.1.dylib 
install_name_tool -change libtfarm.1.dylib @executable_path/../Frameworks/libtfarm.1.dylib $BINROOT/bin/Toonz\ 7.1.app/Contents/MacOS/Toonz\ 7.1

# tfarm dipende da tnzbase tnzcore toonzlib QtGui QtCore QtOpenGl e QtNetwork

install_name_tool -change libtnzbase.1.dylib @executable_path/../Frameworks/libtnzbase.1.dylib $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/libtfarm.1.dylib
install_name_tool -change libtnzcore.1.dylib @executable_path/../Frameworks/libtnzcore.1.dylib $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/libtfarm.1.dylib
install_name_tool -change libtoonzlib.1.dylib @executable_path/../Frameworks/libtoonzlib.1.dylib $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/libtfarm.1.dylib
install_name_tool -change /usr/local/Trolltech/Qt-4.8.0/lib/QtGui.framework/Versions/4/QtGui @executable_path/../Frameworks/QtGui.framework/Versions/4/QtGui $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/libtfarm.1.dylib
install_name_tool -change /usr/local/Trolltech/Qt-4.8.0/lib/QtCore.framework/Versions/4/QtCore @executable_path/../Frameworks/QtCore.framework/Versions/4/QtCore $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/libtfarm.1.dylib
install_name_tool -change /usr/local/Trolltech/Qt-4.8.0/lib/QtOpenGL.framework/Versions/4/QtOpenGL @executable_path/../Frameworks/QtOpenGL.framework/Versions/4/QtOpenGL $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/libtfarm.1.dylib
install_name_tool -change /usr/local/Trolltech/Qt-4.8.0/lib/QtNetwork.framework/Versions/4/QtNetwork @executable_path/../Frameworks/QtNetwork.framework/Versions/4/QtNetwork $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/libtfarm.1.dylib


# toonzqt

install_name_tool -id @executable_path/../Frameworks/libtoonzqt.1.dylib $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/libtoonzqt.1.dylib 
install_name_tool -change libtoonzqt.1.dylib @executable_path/../Frameworks/libtoonzqt.1.dylib $BINROOT/bin/Toonz\ 7.1.app/Contents/MacOS/Toonz\ 7.1

# toonzqt dipende da tnzbase tnzcore toonzlib tnzext sound QtGui QtCore QtOpenGl e QtNetwork

install_name_tool -change libtnzbase.1.dylib @executable_path/../Frameworks/libtnzbase.1.dylib $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/libtoonzqt.1.dylib
install_name_tool -change libtnzcore.1.dylib @executable_path/../Frameworks/libtnzcore.1.dylib $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/libtoonzqt.1.dylib
install_name_tool -change libtoonzlib.1.dylib @executable_path/../Frameworks/libtoonzlib.1.dylib $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/libtoonzqt.1.dylib
install_name_tool -change libtnzext.1.dylib @executable_path/../Frameworks/libtnzext.1.dylib $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/libtoonzqt.1.dylib
install_name_tool -change libsound.1.dylib @executable_path/../Frameworks/libsound.1.dylib $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/libtoonzqt.1.dylib
install_name_tool -change /usr/local/Trolltech/Qt-4.8.0/lib/QtGui.framework/Versions/4/QtGui @executable_path/../Frameworks/QtGui.framework/Versions/4/QtGui $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/libtoonzqt.1.dylib
install_name_tool -change /usr/local/Trolltech/Qt-4.8.0/lib/QtCore.framework/Versions/4/QtCore @executable_path/../Frameworks/QtCore.framework/Versions/4/QtCore $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/libtoonzqt.1.dylib
install_name_tool -change /usr/local/Trolltech/Qt-4.8.0/lib/QtOpenGL.framework/Versions/4/QtOpenGL @executable_path/../Frameworks/QtOpenGL.framework/Versions/4/QtOpenGL $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/libtoonzqt.1.dylib
install_name_tool -change /usr/local/Trolltech/Qt-4.8.0/lib/QtNetwork.framework/Versions/4/QtNetwork @executable_path/../Frameworks/QtNetwork.framework/Versions/4/QtNetwork $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/libtoonzqt.1.dylib


# tnztools

install_name_tool -id @executable_path/../Frameworks/libtnztools.1.dylib $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/libtnztools.1.dylib 
install_name_tool -change libtnztools.1.dylib @executable_path/../Frameworks/libtnztools.1.dylib $BINROOT/bin/Toonz\ 7.1.app/Contents/MacOS/Toonz\ 7.1

# tnztools dipende da tnzbase tnzcore toonzlib toonzqt tnzext QtGui QtCore QtOpenGL

install_name_tool -change libtnzbase.1.dylib @executable_path/../Frameworks/libtnzbase.1.dylib $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/libtnztools.1.dylib
install_name_tool -change libtnzcore.1.dylib @executable_path/../Frameworks/libtnzcore.1.dylib $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/libtnztools.1.dylib
install_name_tool -change libtoonzlib.1.dylib @executable_path/../Frameworks/libtoonzlib.1.dylib $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/libtnztools.1.dylib
install_name_tool -change libtoonzqt.1.dylib @executable_path/../Frameworks/libtoonzqt.1.dylib $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/libtnztools.1.dylib
install_name_tool -change libtnzext.1.dylib @executable_path/../Frameworks/libtnzext.1.dylib $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/libtnztools.1.dylib
install_name_tool -change /usr/local/Trolltech/Qt-4.8.0/lib/QtGui.framework/Versions/4/QtGui @executable_path/../Frameworks/QtGui.framework/Versions/4/QtGui $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/libtnztools.1.dylib
install_name_tool -change /usr/local/Trolltech/Qt-4.8.0/lib/QtCore.framework/Versions/4/QtCore @executable_path/../Frameworks/QtCore.framework/Versions/4/QtCore $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/libtnztools.1.dylib
install_name_tool -change /usr/local/Trolltech/Qt-4.8.0/lib/QtOpenGL.framework/Versions/4/QtOpenGL @executable_path/../Frameworks/QtOpenGL.framework/Versions/4/QtOpenGL $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/libtnztools.1.dylib






#QT LIBRARIES

# RICORDARSI PRIMA DI COPIARE I FRAMEWORK QT (AD ESEMPIO PER COPIARE QtCore)
# cp -R /Library/Frameworks/QtCore.framework Toonz\ 7.1.app/Contents/Frameworks 

# $BINROOT/bin/deployqt $BINROOT/bin/Toonz\ 7.1.app

install_name_tool -id @executable_path/../Frameworks/QtCore.framework/Versions/4/QtCore $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/QtCore.framework/Versions/4/QtCore
install_name_tool -change /usr/local/Trolltech/Qt-4.8.0/lib/QtCore.framework/Versions/4/QtCore @executable_path/../Frameworks/QtCore.framework/Versions/4/QtCore $BINROOT/bin/Toonz\ 7.1.app/Contents/MacOS/Toonz\ 7.1

install_name_tool -id @executable_path/../Frameworks/QtGui.framework/Versions/4/QtGui $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/QtGui.framework/Versions/4/QtGui
install_name_tool -change /usr/local/Trolltech/Qt-4.8.0/lib/QtGui.framework/Versions/4/QtGui @executable_path/../Frameworks/QtGui.framework/Versions/4/QtGui $BINROOT/bin/Toonz\ 7.1.app/Contents/MacOS/Toonz\ 7.1

install_name_tool -id @executable_path/../Frameworks/QtOpenGL.framework/Versions/4/QtOpenGL $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/QtOpenGL.framework/Versions/4/QtOpenGL
install_name_tool -change /usr/local/Trolltech/Qt-4.8.0/lib/QtOpenGL.framework/Versions/4/QtOpenGL @executable_path/../Frameworks/QtOpenGL.framework/Versions/4/QtOpenGL $BINROOT/bin/Toonz\ 7.1.app/Contents/MacOS/Toonz\ 7.1

install_name_tool -id @executable_path/../Frameworks/QtXml.framework/Versions/4/QtXml $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/QtXml.framework/Versions/4/QtXml
install_name_tool -change /usr/local/Trolltech/Qt-4.8.0/lib/QtXml.framework/Versions/4/QtXml @executable_path/../Frameworks/QtXml.framework/Versions/4/QtXml $BINROOT/bin/Toonz\ 7.1.app/Contents/MacOS/Toonz\ 7.1

install_name_tool -id @executable_path/../Frameworks/QtSvg.framework/Versions/4/QtSvg $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/QtSvg.framework/Versions/4/QtSvg
install_name_tool -change /usr/local/Trolltech/Qt-4.8.0/lib/QtSvg.framework/Versions/4/QtSvg @executable_path/../Frameworks/QtSvg.framework/Versions/4/QtSvg $BINROOT/bin/Toonz\ 7.1.app/Contents/MacOS/Toonz\ 7.1

install_name_tool -id @executable_path/../Frameworks/QtNetwork.framework/Versions/4/QtNetwork $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/QtNetwork.framework/Versions/4/QtNetwork
install_name_tool -change /usr/local/Trolltech/Qt-4.8.0/lib/QtNetwork.framework/Versions/4/QtNetwork @executable_path/../Frameworks/QtNetwork.framework/Versions/4/QtNetwork $BINROOT/bin/Toonz\ 7.1.app/Contents/MacOS/Toonz\ 7.1


install_name_tool -id @executable_path/../Frameworks/QtScript.framework/Versions/4/QtScript $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/QtScript.framework/Versions/4/QtScript
install_name_tool -change /usr/local/Trolltech/Qt-4.8.0/lib/QtScript.framework/Versions/4/QtScript @executable_path/../Frameworks/QtScript.framework/Versions/4/QtScript $BINROOT/bin/Toonz\ 7.1.app/Contents/MacOS/Toonz\ 7.1


#  QtGui dipende da QtCore... gli aggiorno le path (internamente)

install_name_tool -change QtCore.framework/Versions/4/QtCore @executable_path/../Frameworks/QtCore.framework/Versions/4/QtCore $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/QtGui.framework/Versions/4/QtGui

# QtXml dipende da QTCore

install_name_tool -change QtCore.framework/Versions/4/QtCore @executable_path/../Frameworks/QtCore.framework/Versions/4/QtCore $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/QtXml.framework/Versions/4/QtXml

#  QtOpenGL dipende da QtGui e da QtCore ... gli aggiorno le path (internamente)

install_name_tool -change QtGui.framework/Versions/4/QtGui @executable_path/../Frameworks/QtGui.framework/Versions/4/QtGui  $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/QtOpenGL.framework/Versions/4/QtOpenGL

install_name_tool -change QtCore.framework/Versions/4/QtCore @executable_path/../Frameworks/QtCore.framework/Versions/4/QtCore  $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/QtOpenGL.framework/Versions/4/QtOpenGL

# QtNetwork dipende da QTCore

install_name_tool -change QtCore.framework/Versions/4/QtCore @executable_path/../Frameworks/QtCore.framework/Versions/4/QtCore  $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/QtNetwork.framework/Versions/4/QtNetwork

# QtSvg dipende da QTCore QTGui

install_name_tool -change QtGui.framework/Versions/4/QtGui @executable_path/../Frameworks/QtGui.framework/Versions/4/QtGui  $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/QtSvg.framework/Versions/4/QtSvg

install_name_tool -change QtCore.framework/Versions/4/QtCore @executable_path/../Frameworks/QtCore.framework/Versions/4/QtCore $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/QtSvg.framework/Versions/4/QtSvg

install_name_tool -change QtXml.framework/Versions/4/QtXml @executable_path/../Frameworks/QtXml.framework/Versions/4/QtXml  $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/QtSvg.framework/Versions/4/QtSvg

# QtScript dipende da QTCore

install_name_tool -change QtCore.framework/Versions/4/QtCore @executable_path/../Frameworks/QtCore.framework/Versions/4/QtCore $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/QtScript.framework/Versions/4/QtScript


# PLUGINS QT (jpg) dipende da QtGui e QtCore

install_name_tool -change QtGui.framework/Versions/4/QtGui @executable_path/../Frameworks/QtGui.framework/Versions/4/QtGui $BINROOT/bin/Toonz\ 7.1.app/Contents/MacOS/imageformats/libqjpeg.dylib
install_name_tool -change QtCore.framework/Versions/4/QtCore @executable_path/../Frameworks/QtCore.framework/Versions/4/QtCore $BINROOT/bin/Toonz\ 7.1.app/Contents/MacOS/imageformats/libqjpeg.dylib

# PLUGINS QT (GIF) dipende da QtGui e QtCore

install_name_tool -change QtGui.framework/Versions/4/QtGui @executable_path/../Frameworks/QtGui.framework/Versions/4/QtGui $BINROOT/bin/Toonz\ 7.1.app/Contents/MacOS/imageformats/libqgif.dylib
install_name_tool -change QtCore.framework/Versions/4/QtCore @executable_path/../Frameworks/QtCore.framework/Versions/4/QtCore $BINROOT/bin/Toonz\ 7.1.app/Contents/MacOS/imageformats/libqgif.dylib

# PLUGINS QT (TIFF) dipende da QtGui e QtCore

install_name_tool -change QtGui.framework/Versions/4/QtGui @executable_path/../Frameworks/QtGui.framework/Versions/4/QtGui $BINROOT/bin/Toonz\ 7.1.app/Contents/MacOS/imageformats/libqtiff.dylib
install_name_tool -change QtCore.framework/Versions/4/QtCore @executable_path/../Frameworks/QtCore.framework/Versions/4/QtCore $BINROOT/bin/Toonz\ 7.1.app/Contents/MacOS/imageformats/libqtiff.dylib

# PLUGINS QT (svg) dipende da QtGui QtCore QtSvg e QtXml

install_name_tool -change QtGui.framework/Versions/4/QtGui @executable_path/../Frameworks/QtGui.framework/Versions/4/QtGui $BINROOT/bin/Toonz\ 7.1.app/Contents/MacOS/imageformats/libqsvg.dylib
install_name_tool -change QtCore.framework/Versions/4/QtCore @executable_path/../Frameworks/QtCore.framework/Versions/4/QtCore $BINROOT/bin/Toonz\ 7.1.app/Contents/MacOS/imageformats/libqsvg.dylib
install_name_tool -change QtSvg.framework/Versions/4/QtSvg @executable_path/../Frameworks/QtSvg.framework/Versions/4/QtSvg $BINROOT/bin/Toonz\ 7.1.app/Contents/MacOS/imageformats/libqsvg.dylib
install_name_tool -change QtXml.framework/Versions/4/QtXml @executable_path/../Frameworks/QtXml.framework/Versions/4/QtXml $BINROOT/bin/Toonz\ 7.1.app/Contents/MacOS/imageformats/libqsvg.dylib


# GLUT forse non serve

#install_name_tool -id @executable_path/../Frameworks/libglut.3.dylib Toonz\ 7.1.app/Contents/Frameworks/libglut.3.dylib 
#install_name_tool -change /sw/lib/libglut.3.dylib @executable_path/../Frameworks/libglut.3.dylib Toonz\ 7.1.app/Contents/MacOS/Toonz\ 7.1



# modifico le liblrerie in tcomposer

#tnzcore

install_name_tool -id @executable_path/../Frameworks/libtnzcore.1.dylib $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/libtnzcore.1.dylib 
install_name_tool -change libtnzcore.1.dylib @executable_path/../Frameworks/libtnzcore.1.dylib $BINROOT/bin/Toonz\ 7.1.app/Contents/MacOS/tcomposer


# tnzbase

install_name_tool -id @executable_path/../Frameworks/libtnzbase.1.dylib $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/libtnzbase.1.dylib
install_name_tool -change libtnzbase.1.dylib @executable_path/../Frameworks/libtnzbase.1.dylib $BINROOT/bin/Toonz\ 7.1.app/Contents/MacOS/tcomposer

# toonzlib

install_name_tool -id @executable_path/../Frameworks/libtoonzlib.1.dylib $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/libtoonzlib.1.dylib 
install_name_tool -change libtoonzlib.1.dylib @executable_path/../Frameworks/libtoonzlib.1.dylib $BINROOT/bin/Toonz\ 7.1.app/Contents/MacOS/tcomposer

# sound

install_name_tool -id @executable_path/../Frameworks/libsound.1.dylib $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/libsound.1.dylib 
install_name_tool -change libsound.1.dylib @executable_path/../Frameworks/libsound.1.dylib $BINROOT/bin/Toonz\ 7.1.app/Contents/MacOS/tcomposer

# tnzstdfx

install_name_tool -id @executable_path/../Frameworks/libtnzstdfx.1.dylib $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/libtnzstdfx.1.dylib 
install_name_tool -change libtnzstdfx.1.dylib @executable_path/../Frameworks/libtnzstdfx.1.dylib $BINROOT/bin/Toonz\ 7.1.app/Contents/MacOS/tcomposer

# image

install_name_tool -id @executable_path/../Frameworks/libimage.1.dylib $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/libimage.1.dylib 
install_name_tool -change libimage.1.dylib @executable_path/../Frameworks/libimage.1.dylib $BINROOT/bin/Toonz\ 7.1.app/Contents/MacOS/tcomposer

# colorfx

install_name_tool -id @executable_path/../Frameworks/libcolorfx.1.dylib $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/libcolorfx.1.dylib 
install_name_tool -change libcolorfx.1.dylib @executable_path/../Frameworks/libcolorfx.1.dylib $BINROOT/bin/Toonz\ 7.1.app/Contents/MacOS/tcomposer

# tfarm

install_name_tool -id @executable_path/../Frameworks/libtfarm.1.dylib $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/libtfarm.1.dylib 
install_name_tool -change libtfarm.1.dylib @executable_path/../Frameworks/libtfarm.1.dylib $BINROOT/bin/Toonz\ 7.1.app/Contents/MacOS/tcomposer

# Qt

install_name_tool -id @executable_path/../Frameworks/QtCore.framework/Versions/4/QtCore $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/QtCore.framework/Versions/4/QtCore
install_name_tool -change /usr/local/Trolltech/Qt-4.8.0/lib/QtCore.framework/Versions/4/QtCore @executable_path/../Frameworks/QtCore.framework/Versions/4/QtCore $BINROOT/bin/Toonz\ 7.1.app/Contents/MacOS/tcomposer

install_name_tool -id @executable_path/../Frameworks/QtGui.framework/Versions/4/QtGui $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/QtGui.framework/Versions/4/QtGui
install_name_tool -change /usr/local/Trolltech/Qt-4.8.0/lib/QtGui.framework/Versions/4/QtGui @executable_path/../Frameworks/QtGui.framework/Versions/4/QtGui $BINROOT/bin/Toonz\ 7.1.app/Contents/MacOS/tcomposer

install_name_tool -id @executable_path/../Frameworks/QtOpenGL.framework/Versions/4/QtOpenGL $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/QtOpenGL.framework/Versions/4/QtOpenGL
install_name_tool -change /usr/local/Trolltech/Qt-4.8.0/lib/QtOpenGL.framework/Versions/4/QtOpenGL @executable_path/../Frameworks/QtOpenGL.framework/Versions/4/QtOpenGL $BINROOT/bin/Toonz\ 7.1.app/Contents/MacOS/tcomposer

install_name_tool -id @executable_path/../Frameworks/QtNetwork.framework/Versions/4/QtNetwork $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/QtNetwork.framework/Versions/4/QtNetwork
install_name_tool -change /usr/local/Trolltech/Qt-4.8.0/lib/QtNetwork.framework/Versions/4/QtNetwork @executable_path/../Frameworks/QtNetwork.framework/Versions/4/QtNetwork $BINROOT/bin/Toonz\ 7.1.app/Contents/MacOS/tcomposer

# modifico le liblrerie in t32bitsrv

#tnzcore

install_name_tool -id @executable_path/../Frameworks/libtnzcore.1.dylib $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/libtnzcore.1.dylib 
install_name_tool -change libtnzcore.1.dylib @executable_path/../Frameworks/libtnzcore.1.dylib $BINROOT/bin/Toonz\ 7.1.app/Contents/MacOS/t32bitsrv


# image

install_name_tool -id @executable_path/../Frameworks/libimage.1.dylib $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/libimage.1.dylib 
install_name_tool -change libimage.1.dylib @executable_path/../Frameworks/libimage.1.dylib $BINROOT/bin/Toonz\ 7.1.app/Contents/MacOS/t32bitsrv


# Qt

install_name_tool -id @executable_path/../Frameworks/QtCore.framework/Versions/4/QtCore $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/QtCore.framework/Versions/4/QtCore
install_name_tool -change /usr/local/Trolltech/Qt-4.8.0/lib/QtCore.framework/Versions/4/QtCore @executable_path/../Frameworks/QtCore.framework/Versions/4/QtCore $BINROOT/bin/Toonz\ 7.1.app/Contents/MacOS/t32bitsrv

install_name_tool -id @executable_path/../Frameworks/QtGui.framework/Versions/4/QtGui $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/QtGui.framework/Versions/4/QtGui
install_name_tool -change /usr/local/Trolltech/Qt-4.8.0/lib/QtGui.framework/Versions/4/QtGui @executable_path/../Frameworks/QtGui.framework/Versions/4/QtGui $BINROOT/bin/Toonz\ 7.1.app/Contents/MacOS/t32bitsrv


install_name_tool -id @executable_path/../Frameworks/QtNetwork.framework/Versions/4/QtNetwork $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/QtNetwork.framework/Versions/4/QtNetwork
install_name_tool -change /usr/local/Trolltech/Qt-4.8.0/lib/QtNetwork.framework/Versions/4/QtNetwork @executable_path/../Frameworks/QtNetwork.framework/Versions/4/QtNetwork $BINROOT/bin/Toonz\ 7.1.app/Contents/MacOS/t32bitsrv




# modifico le liblrerie in tcleanup

#tnzcore

install_name_tool -id @executable_path/../Frameworks/libtnzcore.1.dylib $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/libtnzcore.1.dylib 
install_name_tool -change libtnzcore.1.dylib @executable_path/../Frameworks/libtnzcore.1.dylib $BINROOT/bin/Toonz\ 7.1.app/Contents/MacOS/tcleanup


# tnzbase

install_name_tool -id @executable_path/../Frameworks/libtnzbase.1.dylib $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/libtnzbase.1.dylib
install_name_tool -change libtnzbase.1.dylib @executable_path/../Frameworks/libtnzbase.1.dylib $BINROOT/bin/Toonz\ 7.1.app/Contents/MacOS/tcleanup

# toonzlib

install_name_tool -id @executable_path/../Frameworks/libtoonzlib.1.dylib $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/libtoonzlib.1.dylib 
install_name_tool -change libtoonzlib.1.dylib @executable_path/../Frameworks/libtoonzlib.1.dylib $BINROOT/bin/Toonz\ 7.1.app/Contents/MacOS/tcleanup

# sound

install_name_tool -id @executable_path/../Frameworks/libsound.1.dylib $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/libsound.1.dylib 
install_name_tool -change libsound.1.dylib @executable_path/../Frameworks/libsound.1.dylib $BINROOT/bin/Toonz\ 7.1.app/Contents/MacOS/tcleanup

# tnzstdfx

install_name_tool -id @executable_path/../Frameworks/libtnzstdfx.1.dylib $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/libtnzstdfx.1.dylib 
install_name_tool -change libtnzstdfx.1.dylib @executable_path/../Frameworks/libtnzstdfx.1.dylib $BINROOT/bin/Toonz\ 7.1.app/Contents/MacOS/tcleanup

# image

install_name_tool -id @executable_path/../Frameworks/libimage.1.dylib $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/libimage.1.dylib 
install_name_tool -change libimage.1.dylib @executable_path/../Frameworks/libimage.1.dylib $BINROOT/bin/Toonz\ 7.1.app/Contents/MacOS/tcleanup

# colorfx

install_name_tool -id @executable_path/../Frameworks/libcolorfx.1.dylib $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/libcolorfx.1.dylib 
install_name_tool -change libcolorfx.1.dylib @executable_path/../Frameworks/libcolorfx.1.dylib $BINROOT/bin/Toonz\ 7.1.app/Contents/MacOS/tcleanup

# tfarm

install_name_tool -id @executable_path/../Frameworks/libtfarm.1.dylib $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/libtfarm.1.dylib 
install_name_tool -change libtfarm.1.dylib @executable_path/../Frameworks/libtfarm.1.dylib $BINROOT/bin/Toonz\ 7.1.app/Contents/MacOS/tcleanup

# Qt

install_name_tool -id @executable_path/../Frameworks/QtCore.framework/Versions/4/QtCore $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/QtCore.framework/Versions/4/QtCore
install_name_tool -change /usr/local/Trolltech/Qt-4.8.0/lib/QtCore.framework/Versions/4/QtCore @executable_path/../Frameworks/QtCore.framework/Versions/4/QtCore $BINROOT/bin/Toonz\ 7.1.app/Contents/MacOS/tcleanup

install_name_tool -id @executable_path/../Frameworks/QtGui.framework/Versions/4/QtGui $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/QtGui.framework/Versions/4/QtGui
install_name_tool -change /usr/local/Trolltech/Qt-4.8.0/lib/QtGui.framework/Versions/4/QtGui @executable_path/../Frameworks/QtGui.framework/Versions/4/QtGui $BINROOT/bin/Toonz\ 7.1.app/Contents/MacOS/tcleanup

install_name_tool -id @executable_path/../Frameworks/QtOpenGL.framework/Versions/4/QtOpenGL $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/QtOpenGL.framework/Versions/4/QtOpenGL
install_name_tool -change /usr/local/Trolltech/Qt-4.8.0/lib/QtOpenGL.framework/Versions/4/QtOpenGL @executable_path/../Frameworks/QtOpenGL.framework/Versions/4/QtOpenGL $BINROOT/bin/Toonz\ 7.1.app/Contents/MacOS/tcleanup

install_name_tool -id @executable_path/../Frameworks/QtNetwork.framework/Versions/4/QtNetwork $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/QtNetwork.framework/Versions/4/QtNetwork
install_name_tool -change /usr/local/Trolltech/Qt-4.8.0/lib/QtNetwork.framework/Versions/4/QtNetwork @executable_path/../Frameworks/QtNetwork.framework/Versions/4/QtNetwork $BINROOT/bin/Toonz\ 7.1.app/Contents/MacOS/tcleanup



# modifico le liblrerie in tfarmserver

#tnzcore

install_name_tool -id @executable_path/../Frameworks/libtnzcore.1.dylib $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/libtnzcore.1.dylib 
install_name_tool -change libtnzcore.1.dylib @executable_path/../Frameworks/libtnzcore.1.dylib $BINROOT/bin/Toonz\ 7.1.app/Contents/MacOS/tfarmserver

#tnzbase

install_name_tool -id @executable_path/../Frameworks/libtnzbase.1.dylib $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/libtnzbase.1.dylib 
install_name_tool -change libtnzbase.1.dylib @executable_path/../Frameworks/libtnzbase.1.dylib $BINROOT/bin/Toonz\ 7.1.app/Contents/MacOS/tfarmserver


# tfarm

install_name_tool -id @executable_path/../Frameworks/libtfarm.1.dylib $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/libtfarm.1.dylib 
install_name_tool -change libtfarm.1.dylib @executable_path/../Frameworks/libtfarm.1.dylib $BINROOT/bin/Toonz\ 7.1.app/Contents/MacOS/tfarmserver

# Qt

install_name_tool -id @executable_path/../Frameworks/QtCore.framework/Versions/4/QtCore $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/QtCore.framework/Versions/4/QtCore
install_name_tool -change /usr/local/Trolltech/Qt-4.8.0/lib/QtCore.framework/Versions/4/QtCore @executable_path/../Frameworks/QtCore.framework/Versions/4/QtCore $BINROOT/bin/Toonz\ 7.1.app/Contents/MacOS/tfarmserver

install_name_tool -id @executable_path/../Frameworks/QtGui.framework/Versions/4/QtGui $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/QtGui.framework/Versions/4/QtGui
install_name_tool -change /usr/local/Trolltech/Qt-4.8.0/lib/QtGui.framework/Versions/4/QtGui @executable_path/../Frameworks/QtGui.framework/Versions/4/QtGui $BINROOT/bin/Toonz\ 7.1.app/Contents/MacOS/tfarmserver

install_name_tool -id @executable_path/../Frameworks/QtOpenGL.framework/Versions/4/QtOpenGL $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/QtOpenGL.framework/Versions/4/QtOpenGL
install_name_tool -change /usr/local/Trolltech/Qt-4.8.0/lib/QtOpenGL.framework/Versions/4/QtOpenGL @executable_path/../Frameworks/QtOpenGL.framework/Versions/4/QtOpenGL $BINROOT/bin/Toonz\ 7.1.app/Contents/MacOS/tfarmserver

install_name_tool -id @executable_path/../Frameworks/QtNetwork.framework/Versions/4/QtNetwork $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/QtNetwork.framework/Versions/4/QtNetwork
install_name_tool -change /usr/local/Trolltech/Qt-4.8.0/lib/QtNetwork.framework/Versions/4/QtNetwork @executable_path/../Frameworks/QtNetwork.framework/Versions/4/QtNetwork $BINROOT/bin/Toonz\ 7.1.app/Contents/MacOS/tfarmserver



# modifico le liblrerie in tfarmcontroller

#tnzcore

install_name_tool -id @executable_path/../Frameworks/libtnzcore.1.dylib $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/libtnzcore.1.dylib 
install_name_tool -change libtnzcore.1.dylib @executable_path/../Frameworks/libtnzcore.1.dylib $BINROOT/bin/Toonz\ 7.1.app/Contents/MacOS/tfarmcontroller

#tnzbase

install_name_tool -id @executable_path/../Frameworks/libtnzbase.1.dylib $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/libtnzbase.1.dylib 
install_name_tool -change libtnzbase.1.dylib @executable_path/../Frameworks/libtnzbase.1.dylib $BINROOT/bin/Toonz\ 7.1.app/Contents/MacOS/tfarmcontroller

# tfarm

install_name_tool -id @executable_path/../Frameworks/libtfarm.1.dylib $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/libtfarm.1.dylib 
install_name_tool -change libtfarm.1.dylib @executable_path/../Frameworks/libtfarm.1.dylib $BINROOT/bin/Toonz\ 7.1.app/Contents/MacOS/tfarmcontroller

# Qt

install_name_tool -id @executable_path/../Frameworks/QtCore.framework/Versions/4/QtCore $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/QtCore.framework/Versions/4/QtCore
install_name_tool -change /usr/local/Trolltech/Qt-4.8.0/lib/QtCore.framework/Versions/4/QtCore @executable_path/../Frameworks/QtCore.framework/Versions/4/QtCore $BINROOT/bin/Toonz\ 7.1.app/Contents/MacOS/tfarmcontroller

install_name_tool -id @executable_path/../Frameworks/QtGui.framework/Versions/4/QtGui $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/QtGui.framework/Versions/4/QtGui
install_name_tool -change /usr/local/Trolltech/Qt-4.8.0/lib/QtGui.framework/Versions/4/QtGui @executable_path/../Frameworks/QtGui.framework/Versions/4/QtGui $BINROOT/bin/Toonz\ 7.1.app/Contents/MacOS/tfarmcontroller

install_name_tool -id @executable_path/../Frameworks/QtOpenGL.framework/Versions/4/QtOpenGL $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/QtOpenGL.framework/Versions/4/QtOpenGL
install_name_tool -change /usr/local/Trolltech/Qt-4.8.0/lib/QtOpenGL.framework/Versions/4/QtOpenGL @executable_path/../Frameworks/QtOpenGL.framework/Versions/4/QtOpenGL $BINROOT/bin/Toonz\ 7.1.app/Contents/MacOS/tfarmcontroller

install_name_tool -id @executable_path/../Frameworks/QtNetwork.framework/Versions/4/QtNetwork $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/QtNetwork.framework/Versions/4/QtNetwork
install_name_tool -change /usr/local/Trolltech/Qt-4.8.0/lib/QtNetwork.framework/Versions/4/QtNetwork @executable_path/../Frameworks/QtNetwork.framework/Versions/4/QtNetwork $BINROOT/bin/Toonz\ 7.1.app/Contents/MacOS/tfarmcontroller



# modifico le liblrerie in tconverter

#tnzcore

install_name_tool -id @executable_path/../Frameworks/libtnzcore.1.dylib $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/libtnzcore.1.dylib 
install_name_tool -change libtnzcore.1.dylib @executable_path/../Frameworks/libtnzcore.1.dylib $BINROOT/bin/Toonz\ 7.1.app/Contents/MacOS/tconverter


# tnzbase

install_name_tool -id @executable_path/../Frameworks/libtnzbase.1.dylib $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/libtnzbase.1.dylib
install_name_tool -change libtnzbase.1.dylib @executable_path/../Frameworks/libtnzbase.1.dylib $BINROOT/bin/Toonz\ 7.1.app/Contents/MacOS/tconverter

# toonzlib

install_name_tool -id @executable_path/../Frameworks/libtoonzlib.1.dylib $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/libtoonzlib.1.dylib 
install_name_tool -change libtoonzlib.1.dylib @executable_path/../Frameworks/libtoonzlib.1.dylib $BINROOT/bin/Toonz\ 7.1.app/Contents/MacOS/tconverter

# image

install_name_tool -id @executable_path/../Frameworks/libimage.1.dylib $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/libimage.1.dylib 
install_name_tool -change libimage.1.dylib @executable_path/../Frameworks/libimage.1.dylib $BINROOT/bin/Toonz\ 7.1.app/Contents/MacOS/tconverter

# Qt

install_name_tool -id @executable_path/../Frameworks/QtCore.framework/Versions/4/QtCore $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/QtCore.framework/Versions/4/QtCore
install_name_tool -change /usr/local/Trolltech/Qt-4.8.0/lib/QtCore.framework/Versions/4/QtCore @executable_path/../Frameworks/QtCore.framework/Versions/4/QtCore $BINROOT/bin/Toonz\ 7.1.app/Contents/MacOS/tconverter

install_name_tool -id @executable_path/../Frameworks/QtGui.framework/Versions/4/QtGui $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/QtGui.framework/Versions/4/QtGui
install_name_tool -change /usr/local/Trolltech/Qt-4.8.0/lib/QtGui.framework/Versions/4/QtGui @executable_path/../Frameworks/QtGui.framework/Versions/4/QtGui $BINROOT/bin/Toonz\ 7.1.app/Contents/MacOS/tconverter

install_name_tool -id @executable_path/../Frameworks/QtOpenGL.framework/Versions/4/QtOpenGL $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/QtOpenGL.framework/Versions/4/QtOpenGL
install_name_tool -change /usr/local/Trolltech/Qt-4.8.0/lib/QtOpenGL.framework/Versions/4/QtOpenGL @executable_path/../Frameworks/QtOpenGL.framework/Versions/4/QtOpenGL $BINROOT/bin/Toonz\ 7.1.app/Contents/MacOS/tconverter

install_name_tool -id @executable_path/../Frameworks/QtNetwork.framework/Versions/4/QtNetwork $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/QtNetwork.framework/Versions/4/QtNetwork
install_name_tool -change /usr/local/Trolltech/Qt-4.8.0/lib/QtNetwork.framework/Versions/4/QtNetwork @executable_path/../Frameworks/QtNetwork.framework/Versions/4/QtNetwork $BINROOT/bin/Toonz\ 7.1.app/Contents/MacOS/tconverter





