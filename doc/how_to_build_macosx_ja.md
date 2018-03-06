# MacOSX での開発環境構築

## 必要なソフトウェア

- git
- brew
- Xcode
- cmake
  - バージョン 3.2.2 で動作確認済みです。
- Qt
  - http://download.qt.io/official_releases/qt/5.9/5.9.2/
    - qt-opensource-mac-x64-5.9.2.dmg
- boost
  - http://www.boost.org/users/history/version_1_55_0.html

## ビルド手順

### brew で必要なパッケージをインストール

```
$ brew install glew lz4 libjpeg libpng lzo pkg-config
```

### リポジトリを clone

```
$ git clone https://github.com/opentoonz/opentoonz
```

### stuff ディレクトリの設置 (任意)

`/Applications/OpenToonz/OpenToonz_1.2_stuff` というディレクトリが存在していない場合は以下のコマンド等でリポジトリのひな形を設置する必要があります。

```
$ sudo cp -r opentoonz/stuff /Applications/OpenToonz/OpenToonz_1.2_stuff
```

### thirdparty 下の tiff をビルド

```
$ cd opentoonz/thirdparty/tiff-4.0.3
$ ./configure && make
```

### thirdpaty 下に boost を設置

以下のコマンドは `~/Downsloads` に `boost_1_55_0.tar.bz2` がダウンロードされていることを想定しています。

```
$ cd ../boost
$ mv ~/Downloads/boost_1_55_0.tar.bz2 .
$ tar xjvf boost_1_55_0.tar.bz2
```

### 本体のビルド

```
$ cd ../../toonz
$ mkdir build
$ cd build
$ cmake ../sources
$ make
```

ビルドが長いので気長に待ちます。

### 完成

```
$ open ./toonz/OpenToonz_1.2.app
```
