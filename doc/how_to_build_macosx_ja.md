# MacOSX での開発環境構築

## 必要なソフトウェア

- git
- brew
- Xcode
- cmake (3.2.2以降)
- Qt (5.9.2以降)
- boost　(1.55.0以降)

## ビルド手順

### Xcode をインストール

### Homebrew をインストール

1. ターミナルウィンドウを起動
2. 下記を実行します：
```
$ /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install.sh)"
```

### brew で必要なパッケージをインストール

```
$ brew install glew lz4 libjpeg libpng lzo pkg-config libusb cmake git-lfs libmypaint qt boost
$ git lfs install
```

### リポジトリを clone

```
$ git clone https://github.com/opentoonz/opentoonz
$ cd opentoonz
$ git lfs pull
$ cd thirdparty/lzo
$ cp -r 2.03/include/lzo driver
$ cd ../tiff-4.0.3
$ ./configure && make
```

boost をhomebrewではなく https://boost.org からダウンロードした場合は以下を実行します。以下のコマンドは `~/Downsloads` に `boost_1_72_0.tar.bz2` がダウンロードされていることを想定しています。
```
$ cd ../boost
$ mv ~/Downloads/boost_1_72_0.tar.bz2 .
$ tar xjvf boost_1_72_0.tar.bz2
```

### 本体のビルド

1. buildディレクトリの作成
```
$ cd ../../toonz
$ mkdir build
$ cd build
```

2. ビルド
コマンドラインの場合は下記を実行します。
```
$ cmake ../sources -DQT_PATH='/usr/local/opt/qt/lib'
$ make
```
- Qt をHomebrewでなく http://download.qt.io/official_releases/qt/ からダウンロードして `/Users/ユーザ名/Qt` にインストールしている場合、`QT_PATH`の値は `~/Qt/5.12.2/clang_64/lib` または `~/Qt/5.12.2/clang_32/lib` のようになります。

ビルドが長いので気長に待ちます。

Xcodeを用いる場合は下記を実行します。
```
$ sudo xcode-select -s /Applications/Xcode.app/Contents/Developer
$ cmake -G Xcode ../sources -B. -DQT_PATH='/usr/local/opt/qt/lib' -DWITH_TRANSLATION=OFF
```
- オプション `-DWITH_TRANSLATION=OFF` はXcode12以降で必要です。
- Xcodeでプロジェクト `/opentoonz/toonz/build/OpenToonz.xcodeproj` を開き、ビルドします。

### stuff ディレクトリの設置 (任意)

`/Applications/OpenToonz/OpenToonz_stuff` というディレクトリが存在していない場合は以下のコマンド等でリポジトリのひな形を設置する必要があります。

```
$ sudo cp -r opentoonz/stuff /Applications/OpenToonz/OpenToonz_stuff
$ sudo chmod -R 777 /Applications/OpenToonz
```

### アプリケーションの実行

```
$ open ./toonz/OpenToonz.app
```

- Xcode でビルドしている場合、アプリケーションは　`.toonz/build/Debug/OpenToonz.app` にあります。