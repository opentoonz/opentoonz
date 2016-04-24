# Building on Windows

This software can be built using Visual Studio 2013 and Qt 5.5

## Required Software

### Visual Studio Express 2013 for Windows Desktop
- https://www.microsoft.com/en-us/download/details.aspx?id=44914
- Since Qt 5.5 does not work with Visual Studio 2015, Visual Studio 2013 must be used.
- Make sure that the target platform is "for Windows Desktop" not "for Windows".
- Community and Provessional versions of Visual Studio 2013 for Windows Desktop also work.

### CMake
- https://cmake.org/download/
- This will be used to create the VS2013 project file.

## Acquiring the Source Code
- Clone the base repository.
- Throughout the explanation `$opentoonz` will represent the root for the base repository.
- Visual Studio 2013 cannot recognize UTF-8 without BOM source code properly.  Furthermore, since the endline character is represented with only the LF character, one line comments in Japanese will often cause the following line to be treated as a comment by VS2013 as well.
- In order to prevent this, please change the following setting in git so that it will preserve the proper endline characters:
  - `git config core.safecrlf true`

## Installation of Required Libraries
Because of the size of these libraries, they are not maintained in the git repository.
They will have to be installed seperately as follows.

### Qt
- http://download.qt.io/official_releases/qt/5.5/5.5.1/
- Qt is a cross-platform GUI framework.
- This project does not use the latest Qt framework (5.6).
- Select the following file from the above link:
  - qt-opensource-windows-x86-msvc2013_64-5.5.1.exe

### boost
- boost 1.55.0 or later is required (tested up to 1.60.0), but the support build is using 1.55.0 exactly.
- http://www.boost.org/users/history/version_1_55_0.html
- Download boost_1_55_0.zip from the above link.  Extract all contents to the '$opentoonz/thirdparty/boost' directory.
- Install the following path for Visual Studio 2013
  - https://svn.boost.org/trac/boost/attachment/ticket/9369/vc12_fix_has_member_function_callable_with.patch


## Building

### Using CMake to Create a Visual Studio Project
1. Launch CMake
2. In `Where is the source code`, navigate to `$opentoonz/toonz/sources`
3. In `Where to build the binaries`, navigate to `$opentoonz/toonz/build`
  - Or to wherever you usually build to.
  - If the build directory is in the git repository, be sure to add the directory to .gitignore
  - If the build directory is different from the one above, be sure to change to the specified directory where appropriate below.
4. Click on Configure and select Visual Studio 12 2013 Win64.
5. If Qt was installed to a directory other than the default, and the error `Specify QT_PATH properly` appears, navigate to the `QT_DIR` install folder and specify the path to `msvc2013_64`.
6. Click Generate
  - Should the CMakeLists.txt file change, such as during automatic build cleanup, there is no need to rerun CMake.

## Setting Up Libraries
Rename the following files:
  - `$opentoonz/thirdparty/LibJPEG/jpeg-9/jconfig.vc` → `$opentoonz/thirdparty/LibJPEG/jpeg-9/jconfig.h`
  - `$opentoonz/thirdparty/tiff-4.0.3/libtiff/tif_config.vc.h` → `$opentoonz/thirdparty/tiff-4.0.3/libtiff/tif_config.h`
  - `$opentoonz/thirdparty/tiff-4.0.3/libtiff/tiffconf.vc.h` → `$opentoonz/thirdparty/tiff-4.0.3/libtiff/tiffconf.h`
  - `$opentoonz/thirdparty/libpng-1.6.21/scripts/pnglibconf.h.prebuilt` → `$opentoonz/thirdparty/libpng-1.6.21/pnglibconf.h`

## Building
1. Open `$opentoonz/toonz/build/OpenToonz.sln` and change to `Release`
2. The output will be in `$opentoonz/toonz/build/Release`

## Running the Program
### Setting Up the Program's Path
1. Copy the entire contents of `$opentoonz/toonz/build/Release` to an appropriate folder.
2. In the path for `OpenToonz_1.0.exe`, append the Qt program `windeployqt.exe` as an argument.
  - The necessary Qt library files should be in the same folder as `OpenToonz_1.0.exe`
3. Copy the following files to the same folder as `OpenToonz_1.0.exe`
  - `$opentoonz/thirdparty/glut/3.7.6/lib/glut64.dll`
  - `$opentoonz/thirdparty/glew/glew-1.9.0/bin/64bit/glew32.dll`
4. Copy the `srv` folder from the previous OpenToonz installation to the same folder as `OpenToonz_1.0.exe`
  - If there is no `srv` folder, OpenToonz can still be used.  However, various file formats such as `mov` cannot be used.
  - Creating the files for `srv` is discussed later.

### Creating the stuff Folder
If a previous binary of OpenToonz was already installed, this step and the following about creating a registry key has already been dealt with.  So feel free to skip these parts.

1. Copy the files from `$opentoonz/stuff` to an appropriate folder.

### Creating Registry Keys
1. Using the registry editor, create the following key and copy the path of the `$opentoonz/stuff` folder from above to it.
  - HKEY_LOCAL_MACHINE\SOFTWARE\OpenToonz\OpenToonz\1.0\TOONZROOT

### Running
`OpenToonz_1.0.exe` can now be run.  Congratulations!

## Creating the Files for the srv Folder
OpenToonz utilizes the QuickTime SDK's `mov` and associated file formats.  Since the QuickTime SDK only comes in 32-bit versions, the 32-bit file `t32bitsrv.exe` from the QuickTime SDK is used in both the 64-bit and 32-bit versions of OpenToonz.  As a result, the following instructions apply to both the 32 and 64-bit versions of OpenToonz.

### Qt
- http://download.qt.io/official_releases/qt/5.5/5.5.1/
- Find the following file from the above link and install in the appropriate folder.
  - qt-opensource-windows-x86-msvc2013-5.5.1.exe

### QuickTime SDK
1. Sign in using your Apple developer ID and download `QuickTime 7.3 SDK for Windows.zip` from the following url.
  - https://developer.apple.com/downloads/?q=quicktime
2. After installing the QuickTime SDK, copy the contents of `C:\Program Files (x86)\QuickTime SDK` to `$opentoonz/thirdparty/quicktime/QT73SDK`

### Using CMake to Create a Visual Studio 32-bit Project
- Follow the same instructions as for the 64-bit version, but change the following:
  - `$opentoonz/toonz/build` → `$opentoonz/toonz/build32`
  - Visual Studio 12 2013 Win64 → Visual Studio 12 2013
- Change `QT_DIR` to the path of your 32-bit version of Qt

### Building the 32-bit Version
1. Open `$opentoonz/toonz/build32/OpenToonz.sln`

### Layout of the srv Folder
- For the 64-bit version, copy the following files to the `srv` folder:
  - From `$opentoonz/toonz/build32/Release`
    - t32bitsrv.exe
    - image.dll
    - tnzcore.dll
  - From the 32-bit version of Qt
    - Qt5Core.dll
    - Qt5Network.dll
  - `$opentoonz/thirdparty/glut/3.7.6/lib/glut32.dll`

## Creating Translation Files
Qt translation files are generated first from the source code to .ts files, then from .ts files to a .qm file.  These files can be created in Visual Studio if the `translation_` project and `Build translation_??? only` (`translation_???`のみをビルド」) is used.  These files are not created in the default `Build Project Solution`.
