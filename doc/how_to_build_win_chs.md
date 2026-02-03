# 如何在Windows上构建

可以使用Visual Studio 2019以及Qt 5.x版本构建。
Visual Studio 2019构建的工程文件可以使用2022版本打开。

## 软件要求

### Visual Studio Community 2019
- https://www.visualstudio.microsoft.com
- 确保安装MSVC v142 - VS 2019 C++ x64/x86位生成工具。

### CMake
- https://cmake.org/download/
- 用于生成 VS2019 的项目文件。

### Qt
- https://www.qt.io/download-open-source/
- Qt 是一个跨平台的图形化应用开发框架。
- 安装 Qt 5.15： [Qt Online Installer for Windows](http://download.qt.io/official_releases/online_installers/qt-unified-windows-x86-online.exe).

#### 带有WinTab支持的定制化 Qt v5.15.2 （推荐）
- Qt 5.9及之前的版本中使用使用较老的 WinTab API 来实现数位笔输入支持，从Qt 5.12开始，Qt 弃用WinTab API，使用更新的 Windows Ink API支持。但与此同时也导致了一些问题（至少OT是这样）。
- 定制化的 Qt5.15.2 包含了在Qt6.0之后重新引入的WinTab API特性，避免了问题的产生。
- 你可以从[这里](https://github.com/shun-iwasawa/qt5/releases/tag/v5.15.2_wintab)下载为 MSVC2019-x64 构建好的定制化QT版本，并在使用cmake生成项目文件时启用 `WITH_WINTAB` 选项。

### boost库
- 1.55.0 以上版本
(已测试可行的最高版本是1.73.0)
- 下载 [boost_1_73_0.zip](http://www.boost.org/users/history/version_1_73_0.html) 后解压到 '$opentoonz/thirdparty/boost' 文件夹。

### OpenCV
- 4.1.0 以上版本
- https://opencv.org/
- 指定Cmake的环境变量 `OpenCV_DIR` 为OpenCV安装目录内的 `build` 文件夹（例如： `C:/opencv/build`）。

## 获取源码
- 使用VS或者git命令行从github上克隆仓库到本地。
- `$opentoonz` 代表仓库的根目录。

### 库文件（.lib和.dll）

- `lib` 和 `dll` 文件 使用 [Git Large File Storage](https://git-lfs.github.com/)进行版本管理。
（Github桌面版自带git-lfs）
- Visual Studio 无法正确识别 UTF-8 without BOM source code . 具体而言, 由于换行符仅由 LF 字符表示，而在 Visual Studio中，中文或日语的单行注释（CRLF）通常会导致下一行也被视为注释。
- 为了防止类似的情况发生, 请使用git做以下配置来始终使用正确的换行符（建议使用英文版本的VS）：
- `git config core.safecrlf true`
- 执行 `git clone` 后执行 `git lfs pull` 获取库文件。

### 使用CMake生成VS2019项目文件
- 启动 CMake
- 设置源码文件夹位置为 `$opentoonz/flare/sources`
- 设置构建文件夹位置为`$opentoonz/flare/build`
- （或者你自己常用的位置）
- 如果构建文件夹处于仓库文件夹内,确保将其添加到 .gitignore，避免提交到仓库内。
- 如果不使用推荐的构建文件夹，注意在下面的步骤中替换相应的路径。
- 点击 `Configure` 然后选择你使用的 Visual Studio版本
- 如果Qt不是安装在默认路径，并且出现`Specify QT_PATH properly` 的错误，指定CMake的 `QT_DIR` 变量到正确的安装目录，指定`msvc2019_64`的路径。然后重新点击 `Configure`。
- 可以忽略底部的红色行
-点击 `Generate`
-如果 CMakeLists.txt 文件内容有变化，比如自动构建清理（automatic build cleanup）, 不需要重新运行CMake。

## 配置外部头文件
（复制并）重命名以下文件:
- `$opentoonz/thirdparty/LibJPEG/jpeg-9/jconfig.vc` 重命名为 `$opentoonz/thirdparty/LibJPEG/jpeg-9/jconfig.h`
- `$opentoonz/thirdparty/tiff-4.0.3/libtiff/tif_config.vc.h` 重命名为 `$opentoonz/thirdparty/tiff-4.0.3/libtiff/tif_config.h`
- `$opentoonz/thirdparty/tiff-4.0.3/libtiff/tiffconf.vc.h` 重命名为 `$opentoonz/thirdparty/tiff-4.0.3/libtiff/tiffconf.h`
- `$opentoonz/thirdparty/libpng-1.6.21/scripts/pnglibconf.h.prebuilt ` 重命名为 `$opentoonz/thirdparty/libpng-1.6.21/pnglibconf.h`
- 注意：最后一个文件所处的文件夹和之前不一样

## 构建
- 打开 `$opentoonz/flare/build/OpenToonz.sln` 并在顶部切换到 `Debug` 或 `Release`。
- 编译。
- 完成后结果会输出到 `$opentoonz/flare/build/`

## 构建 Canon DSLR 相机支持
下载 Canon SDK 需要先申请加入 Canon 开发者计划。

- 复制 Canon SDK 中的头文件夹和库文件夹 到 `$opentoonz/thirdparty/canon`
  - 确保是来自 **EDSDK_64** 文件夹。

在CMake中选中 `WITH_CANON` 复选框来启用 Canon DSLR 相机支持。

要运行支持 Canon DSLR 相机的程序，你需要将 Canon SDK 中的 `.dll` 文件复制到项目的构建目录。

## 运行程序
### 配置程序目录
1. 复制 $opentoonz/flare/build/Release 内的所有内容到一个指定文件夹。

2. 打开命令行并进入 `QT_DIR/msvc2015_64/bin`目录。运行 `windeployqt.exe`，使用`OpenToonz.exe` 的路径作为参数。（另一种方式是先进入`OpenToonz.exe`所在的文件夹然后拖动`Opentoonz.exe`到`windeployqt.exe`的头上。这样会自动拉取所需文件。）
 - 以下Qt 库文件必须放在和`OpenToonz.exe`同一个文件夹内：
  - `D3Dcompiler_**.dll`
  - `Qt5Core.dll`
  - `Qt5Gui.dll`
  - `Qt5Multimedia.dll`
  - `Qt5Network.dll`
  - `Qt5OpenGL.dll`
  - `Qt5PrintSupport.dll`
  - `Qt5Script.dll`
  - `Qt5SerialPort.dll`
  - `Qt5Svg.dll`
  - `Qt5Widgets.dll`
  - `Qt5Xml.dll`
  
  - 以下文件同理
.
├── audio
│   ├── qtaudio_wasapid.dll
│   └── qtaudio_windowsd.dll
├── bearer
│   └── qgenericbearerd.dll
├── iconengines
│   └── qsvgicond.dll
├── imageformats
│   ├── qgifd.dll
│   ├── qicnsd.dll
│   ├── qicod.dll
│   ├── qjpegd.dll
│   ├── qsvgd.dll
│   ├── qtgad.dll
│   ├── qtiffd.dll
│   ├── qwbmpd.dll
│   └── qwebpd.dll
├── mediaservice
│   ├── dsengined.dll
│   ├── qtmedia_audioengined.dll
│   └── wmfengined.dll
├── platforms
│   └── qwindowsd.dll
├── playlistformats
│   └── qtmultimedia_m3ud.dll
├── printsupport
│   └── windowsprintersupportd.dll
├── styles
│   └── qwindowsvistastyled.dll

3. 将下列文件复制到与 `OpenToonz.exe` 相同目录下
  - `$opentoonz/thirdparty/glut/3.7.6/lib/glut64.dll`
  - `$opentoonz/thirdparty/glew/glew-1.9.0/bin/64bit/glew32.dll`
  - `$opentoonzthirdparty/libmypaint/dist/64/*.dll`
  - libjpeg-turbo的 `turbojpeg.dll` 
  - OpenCV的 `opencv_world***.dll`

4. 从之前安装的OpenToonz的安装目录中复制 `srv` 文件夹到与 `OpenToonz.exe` 相同目录下
  - 如果没有 `srv` 文件夹， OpenToonz 也可以正常使用，但是会缺少一些媒体文件格式的支持，比如`mov`格式。
  - 关于这点稍后会做说明。

### 创建Stuff文件夹
如果之前已经安装过OpenToonz，注册表已经创建好了的话，可以跳过这个步骤。

1. 复制 `$opentoonz/stuff` 的内容到一个指定文件夹。

### 创建注册表
1. 打开注册表编辑器，创建下列键名，并将键值设为`$opentoonz/stuff`的绝对路径。
  - HKEY_LOCAL_MACHINE\SOFTWARE\OpenToonz\OpenToonz\TOONZROOT

### 运行
`OpenToonz.exe` 现在可以运行了。 
恭喜!

## 调试
1. 在解决方案管理器的解决方案**OpenToonz**文件夹内找到项目**OpenToonz**。
2. 右键选择设为启动项目。

## 创建 `srv` 文件夹所需文件

OpenToonz 使用 QuickTime SDK 提供的 MOV 及相关文件格式。由于 QuickTime SDK 仅提供 32 位版本，所以 OpenToonz 的 64 位和 32 位版本都会使用来自 QuickTime SDK 的 32 位文件 `t32bitsrv.exe`。因此，以下说明同时适用于 OpenToonz 的 32 位和 64 位版本。

### Qt
- https://www.qt.io/download-open-source/
- 从这里下载32位版本的Qt [Qt Online Installer for Windows](http://download.qt.io/official_releases/online_installers/qt-unified-windows-x86-online.exe).

### QuickTime SDK
1. 登录Apple开发者账号后下载 `QuickTime 7.3 SDK for Windows.zip` from the following url.
  - https://developer.apple.com/downloads/?q=quicktime
2. QuickTime SDK安装完成后, 复制 `C:\Program Files (x86)\QuickTime SDK` 到 `$opentoonz/thirdparty/quicktime/QT73SDK`

### 使用CMake 构建32位版本的VS2019工程
- 替换以下内容，其余步骤与构建64位版本时相同
  - `$opentoonz/toonz/build` to `$opentoonz/flare/build32`
  - `Visual Studio 16 2019 x64` to `Visual Studio 16 2019 Win32`
- 指定 `QT_PATH` 为32位Qt的Build目录

### 构建 32位版本程序
1. 打开 `$opentoonz/flare/build32/OpenToonz.sln`

### 配置 `srv` 文件夹（MOV视频支持）
- 如果是64位程序, 复制下列文件到 `srv` 文件夹：
  - `$opentoonz/flare/build32/Release`
    - t32bitsrv.exe
    - image.dll
    - tnzbase.dll
    - tnzcore.dll
    - tnzext.dll
    - toonzlib.dll
  - 32位版本的Qt：
    - 使用 `windeployqt.exe` 运行`t32bitsrv.exe`，自动复制相关文件。
    - 另外需要手动复制 Qt5Gui.dll
  - `$opentoonz/thirdparty/glut/3.7.6/lib/glut32.dll`

## 创建翻译文件
Qt 翻译文件首先使用源码生成.ts文件, 然后使用这些.tx文件生成 .qm 文件。这些文件可以在Visual Studio中创建，只要 `translation_` 项目以及 `Build translation_??? only` (`translation_???`のみをビルド」) 被启用。
这些文件不会在默认的 `Build Project Solution` 中生成。