# ビルド手順（Windows）

Visual Studio 2013とQt 5.6でビルドできることを確認しています。

## 必要なソフトウェアの導入

### Visual Studio Express 2013 for Windows Desktop
- https://www.microsoft.com/ja-jp/download/details.aspx?id=44914
- Express版はターゲットプラットフォームごとにバージョンが分かれています。「for Windows」ではなく「for Windows Desktop」を使用します
- Community版やProfessional版などでも構いません

### CMake
- https://cmake.org/download/
- Visual Studio用のプロジェクトファイルの生成に使用します

## ソースコードの取得
- 本リポジトリをcloneします
- 以下の説明中の`$opentoonz`は、本リポジトリのrootを表します
- Visual Studio 2013はBOMの無いUTF-8のソースコードを正しく認識できず、改行コードがLFで、1行コメントの末尾が日本語の場合に、改行が無視されて次の行もコメントとして扱われる問題があるため、Gitに下記の設定をして改行コードをCRLFに変換すると良いでしょう
  - `git config core.safecrlf true`

## 必要なライブラリのインストール
サイズの大きいライブラリはこのリポジトリには含めていないので、別途インストールする必要があります。

### Qt
- http://download.qt.io/official_releases/qt/5.6/5.6.0/
- クロスプラットフォームのGUIフレームワークです
- 現在はQt 5.6には対応していません
- 上記のURLから以下のファイルをダウンロードして適当なフォルダにインストールします
  - qt-opensource-windows-x86-msvc2013_64-5.6.0.exe

### boost
- http://www.boost.org/users/history/version_1_55_0.html
- 上記のURLからboost_1_55_0.zipをダウンロードして解凍し、boost_1_55_0を`$opentoonz/thirdparty/boost`にコピーします
- Visual Studio 2013用の下記のパッチを当てます
  - https://svn.boost.org/trac/boost/attachment/ticket/9369/vc12_fix_has_member_function_callable_with.patch

## ビルド

### CMakeでVisual Studioのプロジェクトを生成する
1. CMakeを立ち上げる
2. Where is the source codeに`$opentoonz/toonz/sources`を指定する
3. Where to build the binariesに`$opentoonz/toonz/build`を指定する
  - 他の場所でも構いません
  - チェックアウトしたフォルダ内に作成する場合は、buildから開始するフォルダ名にするとgitから無視されます
  - ビルド先を変更した場合は、以下の説明を適宜読み替えてください
4. Configureをクリックして、Visual Studio 12 2013 Win64を選択します
5. Qtのインストール先がデフォルトではない場合、`Specify QT_PATH properly`というエラーが表示されるので、`QT_DIR`にQt5をインストールしたフォルダ内の`msvc2013_64`のパスを指定します
6. Generateをクリック
  - CMakeLists.txtに変更があった場合は、ビルド時に自動的に処理が走るので、以降はCMakeを直接使用する必要はありません

## ライブラリの設定
下記のファイルをコピーします
  - `$opentoonz/thirdparty/LibJPEG/jpeg-9/jconfig.vc` → `$opentoonz/thirdparty/LibJPEG/jpeg-9/jconfig.h`
  - `$opentoonz/thirdparty/tiff-4.0.3/libtiff/tif_config.vc.h` → `$opentoonz/thirdparty/tiff-4.0.3/libtiff/tif_config.h`
  - `$opentoonz/thirdparty/tiff-4.0.3/libtiff/tiffconf.vc.h` → `$opentoonz/thirdparty/tiff-4.0.3/libtiff/tiffconf.h`
  - `$opentoonz/thirdparty/libpng-1.6.21/scripts/pnglibconf.h.prebuilt` → `$opentoonz/thirdparty/libpng-1.6.21/pnglibconf.h`

## ビルド
1. `$opentoonz/toonz/build/OpenToonz.sln`を開いてRelease構成を選択してビルドします
2. `$opentoonz/toonz/build/Release`にファイルが生成されます

## 実行
### 実行可能ファイルなどの配置
1. `$oepntoonz/toonz/build/Release`の中身を適当なフォルダにコピーします
2. `OpenToonz_1.0.exe`のパスを引数にしてQtに付属の`windeployqt.exe`を実行します
  - 必要なQtのライブラリなどが`OpenToonz_1.0.exe`と同じフォルダに集められます
3. 下記のファイルを`OpenToonz_1.0.exe`と同じフォルダにコピーします
  - `$opentoonz/thirdparty/glut/3.7.6/lib/glut64.dll`
  - `$opentoonz/thirdparty/glew/glew-1.9.0/bin/64bit/glew32.dll`
4. バイナリ版のOpenToonzのインストール先にある`srv`フォルダを`OpenToonz_1.0.exe`と同じフォルダにコピーします
  - `srv`が無くてもOpenToonzは動作しますが、mov形式などに対応できません
  - `srv`内のファイルの生成方法は後述します

### Stuffフォルダの作成
既にバイナリ版のOpenToonzをインストールしている場合、この手順とレジストリキーの作成と同様の処理が行われているため、これらの手順は不要です。

1. `$opentoonz/stuff`を適当なフォルダにコピーします

### レジストリキーの作成
1. レジストリエディタで下記のキーを作成し、Stuffフォルダの作成でコピーしたstuffフォルダのパスを記載します
  - HKEY_LOCAL_MACHINE\SOFTWARE\OpenToonz\OpenToonz\1.0\TOONZROOT

### 実行
OpenToonz_1.0.exeを実行して動作すれば成功です。おめでとうございます。

## srvフォルダ内のファイルの生成
OpenToonzはQuickTime SDKを用いてmov形式などへ対応しています。QuickTime SDKは32ビット版しかないため、`t32bitsrv.exe`という32ビット版の実行可能ファイルにQuickTime SDKを組み込み、64ビット版のOpenToonzは`t32bitsrv.exe`を経由してQuickTime SDKの機能を使用しています。以下の手順では`t32bitsrv.exe`などと合わせて、32ビット版のOpenToonzも生成されます。

### Qt
- http://download.qt.io/official_releases/qt/5.6/5.6.0/
- 上記のURLから以下のファイルをダウンロードして適当なフォルダにインストールします
  - qt-opensource-windows-x86-msvc2013-5.6.0.exe

### QuickTime SDK
1. Appleの開発者登録をして下記のURLから`QuickTime 7.3 SDK for Windows.zip`をダウンロードします
  - https://developer.apple.com/downloads/?q=quicktime
2. QuickTime SDKをインストールして、`C:\Program Files (x86)\QuickTime SDK`の中身を`thirdparty/quicktime/QT73SDK`の中にコピーします

### CMakeでVisual Studioの32ビット版のプロジェクトを生成する
- 64ビット版と同様の手順で、次のようにフォルダ名とターゲットを読み替えます
  - `$opentoonz/toonz/build` → `$opentoonz/toonz/build32`
  - Visual Studio 12 2013 Win64 → Visual Studio 12 2013
- `QT_DIR`には32ビット版のQtのパスを指定します

### 32ビット版のビルド
1. `$opentoonz/toonz/build32/OpenToonz.sln`を開いてビルドします

### srvフォルダの配置
- 64ビット版のsrvフォルダの中に下記のファイルをコピーします
  - `$opentoonz/toonz/build32/Release`から
    - t32bitsrv.exe
    - image.dll
    - tnzcore.dll
  - Qtの32ビット版のインストール先から
    - Qt5Core.dll
    - Qt5Network.dll
  - `$opentoonz/thirdparty/glut/3.7.6/lib/glut32.dll`

## 翻訳ファイルの生成
Qtの翻訳ファイルは、ソースコードから.tsファイルを生成して、.tsファイルに対して翻訳作業を行い、.tsファイルから.qmファイルを生成します。Visual Studioソリューション中の`translation_`から始まるプロジェクトに対して「`translation_???`のみをビルド」を実行すると、.tsファイルと.qmファイルの生成が行われます。これらのプロジェクトはソリューションのビルドではビルドされないようになっています。
