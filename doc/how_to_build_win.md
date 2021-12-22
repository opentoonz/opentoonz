
# Building on Windows

This software can be built using Visual Studio 2019 and Qt 5.x

## Required Software

### Visual Studio Community 2019
- https://www.visualstudio.microsoft.com
- During the installation, make sure to select all the Visual C++ packages.

### CMake
- https://cmake.org/download/
- This will be used to create the `MSVC 2019` project file.

### Qt
- https://www.qt.io/download-open-source/
- Qt is a cross-platform GUI framework.
- Install Qt 5.9 or higher (64-bit version, tested up to 5.15.2) with the [Qt Online Installer for Windows](http://download.qt.io/official_releases/online_installers/qt-unified-windows-x86-online.exe).

#### Customized Qt v5.15.2 with WinTab support
- Qt have started to support Windows Ink from 5.12. Unlike WinTab API used in Qt 5.9 the tablet behaviors are different and are (at least, for OT) problematic.
- The customized Qt5.15.2 are made with cherry-picking the WinTab feature to be officially introduced from 6.0.
- You can build OT with WinTab support by using the prebuilt package of the customized version of Qt for MSVC2019-x64 provided [here](https://github.com/shun-iwasawa/qt5/releases/tag/v5.15.2_wintab) and checking the `WITH_WINTAB` checkbox in CMake to build.

### boost
- Boost 1.55.0 or later is required (tested up to 1.73.0).
- http://www.boost.org/users/history/version_1_73_0.html
- Download boost_1_73_0.zip from the above link. Extract all contents to the - '$opentoonz/thirdparty/boost' directory.

### OpenCV
- v4.1.0 and later
- https://opencv.org/
- On configuring with CMake or in the environmental variables, specify `OpenCV_DIR` to the `build` folder in the install folder of OpenCV (like `C:/opencv/build`).

## Acquiring the Source Code
- Note: You can also perform these next commands with Github for Desktop client.
- Clone the base repository.
- Throughout the explanation `$opentoonz` will represent the root for the base repository.

### lib and dll

- `lib` and `dll` files are tracked by [Git Large File Storage](https://git-lfs.github.com/).
- Note: git-lfs is also installed with Github Desktop.
- Visual Studio cannot recognize UTF-8 without BOM source code properly. Furthermore, since the endline character is represented with only the LF character, one line comments in Japanese will often cause the following line to be treated as a comment by `MSVS` as well.
- In order to prevent this, please change the following setting in git so that it will preserve the proper endline characters:
- `git config core.safecrlf true`
- Execute `git lfs pull` after `git clone` by using the lfs client.

### Using CMake to Create a Visual Studio Project
- Launch CMake
- In `Where is the source code`, navigate to `$opentoonz/toonz/sources`
- In `Where to build the binaries`, navigate to `$opentoonz/toonz/build`
- Or to wherever you usually build to.
- If the build directory is in the git repository, be sure to add the directory to .gitignore
- If the build directory is different from the one above, be sure to change to the specified directory where appropriate below.
-Click on Configure and select the version of Visual Studio you are using.
-If Qt was installed to a directory other than the default, and the error Specify QT_PATH properly appears, navigate to the `QT_DIR` install folder and specify the path to `msvc2019_64`. Rerun Configure.
-If red lines appear in the bottom box, you can safely ignore them.
-Click Generate
-Should the CMakeLists.txt file change, such as during automatic build cleanup, there is no need to rerun CMake.

## Setting Up Libraries
Rename the following files:
- `$opentoonz/thirdparty/LibJPEG/jpeg-9/jconfig.vc` to `$opentoonz/thirdparty/LibJPEG/jpeg-9/jconfig.h`
- `$opentoonz/thirdparty/tiff-4.0.3/libtiff/tif_config.vc.h` to `$opentoonz/thirdparty/tiff-4.0.3/libtiff/tif_config.h`
- `$opentoonz/thirdparty/tiff-4.0.3/libtiff/tiffconf.vc.h to $opentoonz/thirdparty/tiff-4.0.3/libtiff/tiffconf.h`
- `$opentoonz/thirdparty/libpng-1.6.21/scripts/pnglibconf.h.prebuilt to $opentoonz/thirdparty/libpng-1.6.21/pnglibconf.h`
- Note that the destination is a different folder for the last file.

## Building
- Open `$opentoonz/toonz/build/OpenToonz.sln` and change to `Debug` or `Release` in the top bar.
- Compile the build.
- The output will be in the corresponding folder in `$opentoonz/toonz/build/`

## Building with Canon DSLR camera support
 You will need the Canon SDK. This requires applying for the Canon developer program and downloading the SDK.

- Copy the Header and library folders from the Canon SDK to `$opentoonz/thirdparty/canon`
  - Make sure that the library is the one from the EDSDK_64 folder.

Check the `WITH_CANON` checkbox in CMake to build with Canon DSLR camera support.

To run the program with Canon DSLR camera support, you will need to copy the .dll files from the Canon SDK into the folder where your project is built.

## Running the Program
### Setting Up the Program's Path
1. Copy the entire contents of $opentoonz/toonz/build/Release to an appropriate folder.

2. Open a Command Prompt and navigate to `QT_DIR/msvc2015_64/bin`. Run the Qt program `windeployqt.exe` with the path for `OpenToonz.exe` as an argument. (Another way to do this is navigate to the exe that was created in your Release folder and drag and drop the Opentoonz.exe on top of the windeployqt.exe This will automatically generate the QT files and folders you will need.)
 - The necessary Qt library files should be in the same folder as `OpenToonz.exe`
  - These include:
    - `Qt5Core.dll`
    - `Qt5Gui.dll`
    - `Qt5Network.dll`
    - `Qt5OpenGL.dll`
    - `Qt5PrintSupport.dll`
    - `Qt5Script.dll`
    - `Qt5Svg.dll`
    - `Qt5Widgets.dll`
    - `Qt5Multimedia.dll`
  - These files should be in the corresponding folders in the same folder `OpenToonz.exe`
    - `/bearer/qgenericbearer.dll`
    - `/bearer/qnativewifibearer.dll`
    - `/iconengines/qsvgicon.dll`
    - `/imageformats/qdds.dll`
    - `/imageformats/qgif.dll`
    - `/imageformats/qicns.dll`
    - `/imageformats/qico.dll`
    - `/imageformats/qjpeg.dll`
    - `/imageformats/qsvg.dll`
    - `/imageformats/qtga.dll`
    - `/imageformats/qtiff.dll`
    - `/imageformats/qwbmp.dll`
    - `/imageformats/qwebp.dll`
    - `/platforms/qwindows.dll`

3. Copy the following files to the same folder as `OpenToonz.exe`
  - `$opentoonz/thirdparty/glut/3.7.6/lib/glut64.dll`
  - `$opentoonz/thirdparty/glew/glew-1.9.0/bin/64bit/glew32.dll`
  - `turbojpeg.dll` from libjpeg-turbo package
  - `opencv_world***.dll` from OpenCV package

4. Copy the `srv` folder from the previous OpenToonz installation to the same folder as `OpenToonz.exe`
  - If there is no `srv` folder, OpenToonz can still be used.  However, various file formats such as `mov` cannot be used.
  - Creating the files for `srv` is discussed later.

### Creating the stuff Folder
If a previous binary of OpenToonz was already installed, this step and the following about creating a registry key has already been dealt with.  So feel free to skip these parts.

1. Copy the files from `$opentoonz/stuff` to an appropriate folder.

### Creating Registry Keys
1. Using the registry editor, create the following key and copy the path of the `$opentoonz/stuff` folder from above to it.
  - HKEY_LOCAL_MACHINE\SOFTWARE\OpenToonz\OpenToonz\TOONZROOT

### Running
`OpenToonz.exe` can now be run.  Congratulations!

## Debugging
1. In the Solution Explorer, select the OpenToonz project within the OpenToonz solution.
2. In the Project menu, choose Set as StartUp Project.

## Creating the Files for the `srv` Folder
OpenToonz utilizes the QuickTime SDK's `mov` and associated file formats.  Since the QuickTime SDK only comes in 32-bit versions, the 32-bit file `t32bitsrv.exe` from the QuickTime SDK is used in both the 64-bit and 32-bit versions of OpenToonz.  As a result, the following instructions apply to both the 32 and 64-bit versions of OpenToonz.

### Qt
- https://www.qt.io/download-open-source/
- Install Qt 5.x (32-bit version) by the  by [Qt Online Installer for Windows](http://download.qt.io/official_releases/online_installers/qt-unified-windows-x86-online.exe).

### QuickTime SDK
1. Sign in using your Apple developer ID and download `QuickTime 7.3 SDK for Windows.zip` from the following url.
  - https://developer.apple.com/downloads/?q=quicktime
2. After installing the QuickTime SDK, copy the contents of `C:\Program Files (x86)\QuickTime SDK` to `$opentoonz/thirdparty/quicktime/QT73SDK`

### Using CMake to Create a Visual Studio 32-bit Project
- Follow the same instructions as for the 64-bit version, but change the following:
  - `$opentoonz/toonz/build` to `$opentoonz/toonz/build32`
  - `Visual Studio 16 2019 x64` to `Visual Studio 16 2019 Win32`
- Change `QT_PATH` to the path of your 32-bit version of Qt

### Building the 32-bit Version
1. Open `$opentoonz/toonz/build32/OpenToonz.sln`

### Layout of the `srv` Folder
- For the 64-bit version, copy the following files to the `srv` folder:
  - From `$opentoonz/toonz/build32/Release`
    - t32bitsrv.exe
    - image.dll
    - tnzbase.dll
    - tnzcore.dll
    - tnzext.dll
    - toonzlib.dll
  - From the 32-bit version of Qt
    - run `windeployqt` to obtain required libraries
    - additionally, Qt5Gui.dll
  - `$opentoonz/thirdparty/glut/3.7.6/lib/glut32.dll`

## Creating Translation Files
Qt translation files are generated first from the source code to .ts files, then from .ts files to a .qm file.  These files can be created in Visual Studio if the `translation_` project and `Build translation_??? only` (`translation_???`のみをビルド」) is used.  These files are not created in the default `Build Project Solution`.
