#!/bin/sh

echo '   Create Frameworks folder inside toonz 7.1.app/Contents…'
mkdir $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks

echo '   Copy tnzcore to Frameworks...':
cp -RH $BINROOT/bin/libtnzcore.1.dylib $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks
echo '   Copy tnzbase to Frameworks...'
cp -RH $BINROOT/bin/libtnzbase.1.dylib $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks
echo '   Copy tnzext to Frameworks...'
cp -RH $BINROOT/bin/libtnzext.1.dylib $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks
echo '   Copy toonzlib to Frameworks...'
cp -RH $BINROOT/bin/libtoonzlib.1.dylib $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks
echo '   Copy image to Frameworks...'
cp -RH $BINROOT/bin/libimage.1.dylib $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks
echo '   Copy sound to Frameworks...'
cp -RH $BINROOT/bin/libsound.1.dylib $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks
echo '   Copy colorfx to Frameworks...'
cp -RH $BINROOT/bin/libcolorfx.1.dylib $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks
echo '   Copy tnzstdfx to Frameworks...'
cp -RH $BINROOT/bin/libtnzstdfx.1.dylib $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks
echo '   Copy tfarm to Frameworks...'
cp -RH $BINROOT/bin/libtfarm.1.dylib $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks
echo '   Copy tnztools to Frameworks...'
cp -RH $BINROOT/bin/libtnztools.1.dylib $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks
echo '   Copy toonzqt to Frameworks...'
cp -RH $BINROOT/bin/libtoonzqt.1.dylib $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks

echo '   Copy composer...'
cp -RH $BINROOT/bin/tcomposer.app/Contents/MacOS/tcomposer $BINROOT/bin/Toonz\ 7.1.app/Contents/MacOS/
echo '   Copy cleanup...'
cp -RH $BINROOT/bin/tcleanup.app/Contents/MacOS/tcleanup $BINROOT/bin/Toonz\ 7.1.app/Contents/MacOS/
echo '   Copy farmserver...'
cp -RH $BINROOT/bin/tfarmserver.app/Contents/MacOS/tfarmserver $BINROOT/bin/Toonz\ 7.1.app/Contents/MacOS/
echo '   Copy farmcontroller...'
cp -RH $BINROOT/bin/tfarmcontroller.app/Contents/MacOS/tfarmcontroller $BINROOT/bin/Toonz\ 7.1.app/Contents/MacOS/
echo '   Copy converter...'
cp -RH $BINROOT/bin/tconverter.app/Contents/MacOS/tconverter $BINROOT/bin/Toonz\ 7.1.app/Contents/MacOS/

echo '   Copy t32bitsrv...'
lipo -remove x86_64 $BINROOT/bin/t32bitsrv.app/Contents/MacOS/t32bitsrv -output $BINROOT/bin/t32bitsrv.app/Contents/MacOS/t32bitsrv
cp -RH $BINROOT/bin/t32bitsrv.app/Contents/MacOS/t32bitsrv $BINROOT/bin/Toonz\ 7.1.app/Contents/MacOS/

echo '   Copy QTCore 4.8 Frameworks...'
mkdir -p $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/QtCore.framework/Versions/4
cp /Library/Frameworks/QtCore.framework/Versions/4/QtCore $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/QtCore.framework/Versions/4/
echo '   Copy QTGui 4.8 Frameworks...'
mkdir -p $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/QtGui.framework/Versions/4
cp /Library/Frameworks/QtGui.framework/Versions/4/QtGui $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/QtGui.framework/Versions/4/
echo '   Copy QTOpenGL 4.8 Frameworks...'
mkdir -p $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/QtOpenGL.framework/Versions/4
cp /Library/Frameworks/QtOpenGL.framework/Versions/4/QtOpenGL $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/QtOpenGL.framework/Versions/4/
echo '   Copy QtXml 4.8 Frameworks...'
mkdir -p $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/QtXml.framework/Versions/4
cp /Library/Frameworks/QtXml.framework/Versions/4/QtXml $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/QtXml.framework/Versions/4/
echo '   Copy QtSvg 4.8 Frameworks...'
mkdir -p $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/QtSvg.framework/Versions/4
cp /Library/Frameworks/QtSvg.framework/Versions/4/QtSvg $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/QtSvg.framework/Versions/4/
echo '   Copy QtNetwork 4.8 Frameworks...'
mkdir -p $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/QtNetwork.framework/Versions/4
cp /Library/Frameworks/QtNetwork.framework/Versions/4/QtNetwork $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/QtNetwork.framework/Versions/4/
echo '   Copy QtScript 4.8 Frameworks...'
mkdir -p $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/QtScript.framework/Versions/4
cp /Library/Frameworks/QtScript.framework/Versions/4/QtScript $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks/QtScript.framework/Versions/4/

echo '   Copy Qt plugins...'
mkdir -p $BINROOT/bin/Toonz\ 7.1.app/Contents/MacOS/imageformats
cp /Developer/Applications/Qt/plugins/imageformats/libqjpeg.dylib $BINROOT/bin/Toonz\ 7.1.app/Contents/MacOS/imageformats/
cp /Developer/Applications/Qt/plugins/imageformats/libqgif.dylib $BINROOT/bin/Toonz\ 7.1.app/Contents/MacOS/imageformats/
cp /Developer/Applications/Qt/plugins/imageformats/libqtiff.dylib $BINROOT/bin/Toonz\ 7.1.app/Contents/MacOS/imageformats/
cp /Developer/Applications/Qt/plugins/imageformats/libqsvg.dylib $BINROOT/bin/Toonz\ 7.1.app/Contents/MacOS/imageformats/

echo '   Copy GLEW'
cp -RH  /depot/sdk/glew/glew-1.9.0/lib/libGLEW.dylib $BINROOT/bin/Toonz\ 7.1.app/Contents/Frameworks

echo '   Create qt.conf...'
mkdir $BINROOT/bin/Toonz\ 7.1.app/Contents/Resources
echo 'Prefix = .' > $BINROOT/bin/Toonz\ 7.1.app/Contents/Resources/qt.conf
echo '   Copy SystemVar...'
cp ./SystemVar.ini $BINROOT/bin/Toonz\ 7.1.app/Contents/Resources/
chmod 777 $BINROOT/bin/Toonz\ 7.1.app/Contents/Resources/SystemVar.ini
echo '   Copy configfarmroot...'
cp ./configfarmroot.txt $BINROOT/bin/Toonz\ 7.1.app/Contents/Resources/
chmod 777 $BINROOT/bin/Toonz\ 7.1.app/Contents/Resources/configfarmroot.txt
echo '   Copy qt_menu.nib...'
cp -R /Library/Frameworks/QtGui.framework/Versions/Current/Resources/qt_menu.nib $BINROOT/bin/Toonz\ 7.1.app/Contents/Resources/
