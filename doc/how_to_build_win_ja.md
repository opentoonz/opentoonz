# ビルド手順（Windows）

Visual Studio 2015 と Qt 5.9 でビルドできることを確認しています。

## 必要なソフトウェアの導入

### Visual Studio Express 2015 for Windows Desktop
- https://www.visualstudio.com/ja-jp/products/visual-studio-express-vs.aspx
- Express 版はターゲットプラットフォームごとにバージョンが分かれています。 `for Windows` ではなく `for Windows Desktop` を使用します
- Community 版や Professional 版などでも構いません

### CMake
- https://cmake.org/download/
- Visual Studio 用のプロジェクトファイルの生成に使用します

## ソースコードの取得
- 本リポジトリを `git clone` します
- 以下の説明中の `$opentoonz` は、本リポジトリの root を表します
- Visual Studio は BOM の無い UTF-8 のソースコードを正しく認識できず、改行コードが LF で、1行コメントの末尾が日本語の場合に、改行が無視されて次の行もコメントとして扱われる問題があるため、Git に下記の設定をして改行コードを CRLF に変換すると良いでしょう
  - `git config core.safecrlf true`

## 必要なライブラリのインストール
サイズの大きいライブラリはこのリポジトリには含めていないので、別途インストールする必要があります。

### `lib` と `dll`
- `lib` と `dll` ファイルは [Git Large File Storage](https://git-lfs.github.com/) で管理しています。
- `lfs` のクライアントを導入して、上記 `git clone` のあとに `git lfs pull` を実行してください。

### Qt
- https://www.qt.io/download-open-source/
- クロスプラットフォームの GUI フレームワークです
- 上記の URL から以下のファイルをダウンロードして Qt 5.9 (64 ビット版) を適当なフォルダにインストールします
  - [Qt Online Installer for Windows](http://download.qt.io/official_releases/online_installers/qt-unified-windows-x86-online.exe)

### boost
- http://www.boost.org/users/history/version_1_61_0.html
- 上記の URL から boost_1_61_0.zip をダウンロードして解凍し、 boost_1_61_0 を `$opentoonz/thirdparty/boost` にコピーします

## ビルド

### CMake で Visual Studio のプロジェクトを生成する
1. CMake を立ち上げる
2. Where is the source code に `$opentoonz/toonz/sources` を指定する
3. Where to build the binaries に `$opentoonz/toonz/build` を指定する
  - 他の場所でも構いません
  - チェックアウトしたフォルダ内に作成する場合は、buildから開始するフォルダ名にするとgitから無視されます
  - ビルド先を変更した場合は、以下の説明を適宜読み替えてください
4. Configure をクリックして、 Visual Studio 14 2015 Win64 を選択します
5. Qt のインストール先がデフォルトではない場合、 `Specify QT_PATH properly` というエラーが表示されるので、 `QT_PATH` に Qt5 をインストールしたパスを指定します
6. Generate をクリック
  - CMakeLists.txt に変更があった場合は、ビルド時に自動的に処理が走るので、以降は CMake を直接使用する必要はありません

## ライブラリの設定
下記のファイルをコピーします
  - `$opentoonz/thirdparty/LibJPEG/jpeg-9/jconfig.vc` → `$opentoonz/thirdparty/LibJPEG/jpeg-9/jconfig.h`
  - `$opentoonz/thirdparty/tiff-4.0.3/libtiff/tif_config.vc.h` → `$opentoonz/thirdparty/tiff-4.0.3/libtiff/tif_config.h`
  - `$opentoonz/thirdparty/tiff-4.0.3/libtiff/tiffconf.vc.h` → `$opentoonz/thirdparty/tiff-4.0.3/libtiff/tiffconf.h`
  - `$opentoonz/thirdparty/libpng-1.6.21/scripts/pnglibconf.h.prebuilt` → `$opentoonz/thirdparty/libpng-1.6.21/pnglibconf.h`

## ビルド
1. `$opentoonz/toonz/build/OpenToonz.sln` を開いて Release 構成を選択してビルドします
2. `$opentoonz/toonz/build/Release` にファイルが生成されます

## 実行
### 実行可能ファイルなどの配置
1. `$oepntoonz/toonz/build/Release` の中身を適当なフォルダにコピーします
2. `OpenToonz_1.1.exe` のパスを引数にして Qt に付属の `windeployqt.exe` を実行します
  - 必要な Qt のライブラリなどが `OpenToonz_1.1.exe` と同じフォルダに集められます
3. 下記のファイルを `OpenToonz_1.1.exe` と同じフォルダにコピーします
  - `$opentoonz/thirdparty/glut/3.7.6/lib/glut64.dll`
  - `$opentoonz/thirdparty/glew/glew-1.9.0/bin/64bit/glew32.dll`
4. バイナリ版の OpenToonz のインストール先にある `srv` フォルダを `OpenToonz_1.1.exe` と同じフォルダにコピーします
  - `srv` が無くても OpenToonz は動作しますが、 mov 形式などに対応できません
  - `srv` 内のファイルの生成方法は後述します

### Stuffフォルダの作成
既にバイナリ版の OpenToonz をインストールしている場合、この手順とレジストリキーの作成と同様の処理が行われているため、これらの手順は不要です。

1. `$opentoonz/stuff` を適当なフォルダにコピーします

### レジストリキーの作成
1. レジストリエディタで下記のキーを作成し、 Stuff フォルダの作成でコピーした stuff フォルダのパスを記載します
  - HKEY_LOCAL_MACHINE\SOFTWARE\OpenToonz\OpenToonz\1.1\TOONZROOT

### 実行
OpenToonz_1.1.exe を実行して動作すれば成功です。おめでとうございます。

## `srv` フォルダ内のファイルの生成
OpenToonz は QuickTime SDK を用いて mov 形式などへ対応しています。 QuickTime SDK は 32 ビット版しかないため、 `t32bitsrv.exe` という 32 ビット版の実行可能ファイルにQuickTime SDKを組み込み、64ビット版の OpenToonz は `t32bitsrv.exe` を経由して QuickTime SDK の機能を使用しています。以下の手順では `t32bitsrv.exe` などと合わせて、 32 ビット版の OpenToonz も生成されます。

### Qt
- https://www.qt.io/download-open-source/
- 64 ビット版と同じインストーラーで Qt 5.9 (32 ビット版) を適当なフォルダにインストールします

### QuickTime SDK
1. Apple の開発者登録をして下記のURLから `QuickTime 7.3 SDK for Windows.zip` をダウンロードします
  - https://developer.apple.com/downloads/?q=quicktime
2. QuickTime SDK をインストールして、 `C:\Program Files (x86)\QuickTime SDK` の中身を `thirdparty/quicktime/QT73SDK` の中にコピーします

### CMake で Visual Studio の 32 ビット版のプロジェクトを生成する
- 64 ビット版と同様の手順で、次のようにフォルダ名とターゲットを読み替えます
  - `$opentoonz/toonz/build` → `$opentoonz/toonz/build32`
  - Visual Studio 14 2015 Win64 → Visual Studio 14 2015
- `QT_PATH` には 32 ビット版の Qt のパスを指定します

### 32 ビット版のビルド
1. `$opentoonz/toonz/build32/OpenToonz.sln`を開いてビルドします

### `srv` フォルダの配置
- 64 ビット版の `srv` フォルダの中に下記のファイルをコピーします
  - `$opentoonz/toonz/build32/Release` から
    - t32bitsrv.exe
    - image.dll
    - tnzcore.dll
  - Qt の 32ビット版のインストール先から
    - Qt5Core.dll
    - Qt5Network.dll
  - `$opentoonz/thirdparty/glut/3.7.6/lib/glut32.dll`

## 翻訳ファイルの生成
Qt の翻訳ファイルは、ソースコードから `.ts` ファイルを生成して、 `.ts` ファイルに対して翻訳作業を行い、 `.ts` ファイルから `.qm` ファイルを生成します。Visual Studioソリューション中の`translation_`から始まるプロジェクトに対して「 `translation_???` のみをビルド」を実行すると、 `.ts` ファイルと `.qm` ファイルの生成が行われます。これらのプロジェクトはソリューションのビルドではビルドされないようになっています。
