
# Setting Up the Development Environment on MacOSX

## Necessary Software

- git
- brew
- Xcode
- cmake
  - Version 3.2.2 confirmed to work.
- Qt
  - http://download.qt.io/official_releases/qt/5.6/5.6.0/
    - qt-opensource-mac-x64-clang-5.6.0.dmg
- boost
  - http://www.boost.org/users/history/version_1_55_0.html (or later, though only 1.55.0 is supported)

## Building on MacOSX

### 0. Install Qt

### 1. Install Dependent Packages

With homebrew, you can install them with following command.

```
$ brew install glew lz4 libjpeg libpng lzo pkg-config libusb
```

Or, you should build and install them manually.


### 2. Clone the Repository

```
$ git clone https://github.com/opentoonz/opentoonz
```

### (Optional) Create the stuff Directory

If the directory `/Applications/OpenToonz/OpenToonz_1.0_stuff` does not exist, enter the following command:

```
$ sudo cp -r opentoonz/stuff /Applications/OpenToonz/OpenToonz_1.0_stuff
```

### 3. Build tiff in thirdparty

```
$ cd opentoonz/thirdparty/tiff-4.0.3
$ ./configure && make
```

### 4. Put Boost library into thirdpaty directory
The following assumes `boost_1_55_0.tar.bz2` was downloaded to `~/Downloads`.

```
$ cd ../boost
$ mv ~/Downloads/boost_1_55_0.tar.bz2 .
$ tar xjvf boost_1_55_0.tar.bz2
```

### 5. Build Everything Together

```
$ cd ../../toonz
$ mkdir build
$ cd build
  CMAKE_PREFIX_PATH=~/Qt5.6.0/5.6/clang_64 cmake ../sources
$ make
```

Please be patient as the install will take a while.

### After Building

```
$ open ./toonz/OpenToonz_1.0.app
```
