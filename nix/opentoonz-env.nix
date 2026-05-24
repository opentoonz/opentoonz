{
  pkgs,
  lib ? pkgs.lib,
  src ? ../.,
  qtMajor ? 5,
}:

let
  qtMajorString = toString qtMajor;
  inherit (pkgs) stdenv;
  qt =
    if qtMajor == 5 then
      pkgs.qt5
    else if qtMajor == 6 then
      pkgs.qt6
    else
      throw "Unsupported OpenToonz Qt major: ${qtMajorString}";

  qtBase = qt.qtbase;
  qtSvg = qt.qtsvg;
  qtMultimedia = qt.qtmultimedia;
  defaultBuildDirName =
    if qtMajor == 5 then "nix-relwithdebinfo" else "nix-qt6-relwithdebinfo";

  qtInputs =
    if qtMajor == 5 then
      (with qt; [
        qtbase
        qtsvg
        qtscript
        qttools
        qtmultimedia
        qtserialport
      ])
      ++ lib.optionals stdenv.isLinux [ qt.qtwayland ]
    else
      (with qt; [
        qtbase
        qtsvg
        qttools
        qtmultimedia
        qtserialport
        qt5compat
        qtdeclarative
      ])
      ++ lib.optionals stdenv.isLinux [ qt.qtwayland ];

  nativeInputs = [
    pkgs.autoconf
    pkgs.automake
    pkgs.ccache
    pkgs.clang-tools
    pkgs.cmake
    pkgs.git
    pkgs.git-lfs
    pkgs.gnumake
    pkgs.libtool
    pkgs.ninja
    pkgs.pkg-config
  ];

  libraryInputs = [
    pkgs.boost
    pkgs.ffmpeg
    pkgs.freetype
    pkgs.glew
    pkgs.json_c
    pkgs.libjpeg_turbo
    pkgs.libmypaint
    pkgs.libpng
    pkgs.libusb1
    pkgs.lz4
    pkgs.lzo
    pkgs.opencv
    pkgs.openblas
    pkgs.superlu
    pkgs.xz
    pkgs.zlib
  ] ++ lib.optionals stdenv.isLinux [
    pkgs.freeglut
    pkgs.libGL
    pkgs.libGLU
    pkgs.mesa
    pkgs.xorg.libX11
    pkgs.xorg.libXext
    pkgs.xorg.libXi
    pkgs.xorg.libXmu
    pkgs.xorg.libxcb
  ] ++ lib.optionals stdenv.isDarwin [
    pkgs.apple-sdk_15
    pkgs.libiconvReal
  ];

  buildInputs = qtInputs ++ libraryInputs;
  devOutputs = map lib.getDev (qtInputs ++ libraryInputs);
  libOutputs = map lib.getLib (qtInputs ++ libraryInputs);

  cmakePrefixPath = lib.makeSearchPath "lib/cmake" devOutputs;
  pkgConfigPath = lib.concatStringsSep ":" [
    (lib.makeSearchPath "lib/pkgconfig" devOutputs)
    (lib.makeSearchPath "share/pkgconfig" devOutputs)
  ];
  libraryPath = lib.makeLibraryPath libOutputs;

  qtBaseDev = lib.getDev qtBase;
  qtBaseBin = lib.getBin qtBase;
  qtSvgBin = lib.getBin qtSvg;
  qtMultimediaBin = lib.getBin qtMultimedia;
  qtPluginPrefix = qtBase.qtPluginPrefix;
  lzoDev = lib.getDev pkgs.lzo;
  lzoLib = lib.getLib pkgs.lzo;
  superluDev = lib.getDev pkgs.superlu;
  superluLib = lib.getLib pkgs.superlu;

  dylibExt = if stdenv.isDarwin then "dylib" else "so";

  # OpenToonz's current CMake appends /superlu on macOS after FindSuperLU.
  superluIncludeDir =
    if stdenv.isDarwin then "${superluDev}/include" else "${superluDev}/include/superlu";

  compiler =
    if stdenv.isDarwin then
      {
        cc = "/usr/bin/cc";
        cxx = "/usr/bin/c++";
      }
    else
      {
        cc = "${stdenv.cc}/bin/cc";
        cxx = "${stdenv.cc}/bin/c++";
      };

  darwinCompilerSetup = lib.optionalString stdenv.isDarwin ''
    export CC="${compiler.cc}"
    export CXX="${compiler.cxx}"
    unset NIX_CFLAGS_COMPILE NIX_LDFLAGS NIX_LDFLAGS_BEFORE
    unset NIX_ENFORCE_PURITY
    unset NIX_CC NIX_BINTOOLS
    unset DEVELOPER_DIR DEVELOPER_DIR_FOR_BUILD
    unset SDKROOT_FOR_BUILD
    unset NIX_APPLE_SDK_VERSION NIX_APPLE_SDK_VERSION_FOR_BUILD
    unset NIX_CC_WRAPPER_TARGET_HOST_arm64_apple_darwin
    unset NIX_CC_WRAPPER_TARGET_HOST_aarch64_apple_darwin
    unset NIX_CC_WRAPPER_TARGET_HOST_x86_64_apple_darwin
    unset NIX_BINTOOLS_WRAPPER_TARGET_HOST_arm64_apple_darwin
    unset NIX_BINTOOLS_WRAPPER_TARGET_HOST_aarch64_apple_darwin
    unset NIX_BINTOOLS_WRAPPER_TARGET_HOST_x86_64_apple_darwin
    unset LIBRARY_PATH CPATH C_INCLUDE_PATH CPLUS_INCLUDE_PATH
    if [ -z "''${OPENTOONZ_MACOS_SDKROOT:-}" ]; then
      for sdk in \
        /Library/Developer/CommandLineTools/SDKs/MacOSX15.sdk \
        /Library/Developer/CommandLineTools/SDKs/MacOSX15.4.sdk \
        /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX15.sdk \
        /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX15.4.sdk
      do
        if [ -d "$sdk/System/Library/Frameworks/AGL.framework" ]; then
          export OPENTOONZ_MACOS_SDKROOT="$sdk"
          break
        fi
      done
    fi
    if [ -n "''${OPENTOONZ_MACOS_SDKROOT:-}" ]; then
      export SDKROOT="$OPENTOONZ_MACOS_SDKROOT"
      export OPENTOONZ_MACOS_FRAMEWORK_PATH="$OPENTOONZ_MACOS_SDKROOT/System/Library/Frameworks"
      export CMAKE_FRAMEWORK_PATH="$OPENTOONZ_MACOS_FRAMEWORK_PATH''${CMAKE_FRAMEWORK_PATH:+:$CMAKE_FRAMEWORK_PATH}"
    fi
  '';

  darwinEnvVars = lib.optionalAttrs stdenv.isDarwin {
    OPENTOONZ_DARWIN_LIBICONV_LIBRARY = "${lib.getLib pkgs.libiconv}/lib/libiconv.2.${dylibExt}";
    OPENTOONZ_GNU_LIBICONV_LIBRARY = "${lib.getLib pkgs.libiconvReal}/lib/libiconv.2.${dylibExt}";
  };

  envVars = {
    CC = compiler.cc;
    CXX = compiler.cxx;
    OPENTOONZ_QT_MAJOR = qtMajorString;
    OPENTOONZ_QT_PATH = "${qtBaseDev}/lib";
    OPENTOONZ_QT_DIR = "${qtBaseDev}/lib/cmake/Qt${qtMajorString}";
    OPENTOONZ_QT5_DIR = lib.optionalString (qtMajor == 5) "${qtBaseDev}/lib/cmake/Qt5";
    OPENTOONZ_QT6_DIR = lib.optionalString (qtMajor == 6) "${qtBaseDev}/lib/cmake/Qt6";
    OPENTOONZ_QT_PLUGIN_DIRS = lib.concatStringsSep ":" [
      "${qtBaseBin}/${qtPluginPrefix}"
      "${qtSvgBin}/${qtPluginPrefix}"
      "${qtMultimediaBin}/${qtPluginPrefix}"
    ];
    OPENTOONZ_BOOST_ROOT = "${lib.getDev pkgs.boost}";
    OPENTOONZ_CMAKE_C_COMPILER = compiler.cc;
    OPENTOONZ_CMAKE_CXX_COMPILER = compiler.cxx;
    OPENTOONZ_LZO_INCLUDE_DIR = "${lzoDev}/include";
    OPENTOONZ_LZO_LIBRARY = "${lzoLib}/lib/liblzo2.${dylibExt}";
    OPENTOONZ_SUPERLU_INCLUDE_DIR = superluIncludeDir;
    OPENTOONZ_SUPERLU_LIBRARY = "${superluLib}/lib/libsuperlu.${dylibExt}";
  } // darwinEnvVars;

  cmakeFlags = [
    "-G"
    "Ninja"
    "-DCMAKE_BUILD_TYPE=RelWithDebInfo"
    "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON"
    "-DWITH_TRANSLATION=OFF"
    "-DWITH_SYSTEM_LZO=ON"
    "-DWITH_SYSTEM_SUPERLU=ON"
    "-DOPENTOONZ_QT_MAJOR=${qtMajorString}"
    "-DQT_PATH=${envVars.OPENTOONZ_QT_PATH}"
    "-DQt${qtMajorString}_DIR=${envVars.OPENTOONZ_QT_DIR}"
    "-DBOOST_ROOT=${envVars.OPENTOONZ_BOOST_ROOT}"
    "-DBoost_NO_BOOST_CMAKE=ON"
    "-DLZO_INCLUDE_DIR=${envVars.OPENTOONZ_LZO_INCLUDE_DIR}"
    "-DLZO_LIBRARY=${envVars.OPENTOONZ_LZO_LIBRARY}"
    "-DSUPERLU_INCLUDE_DIR=${envVars.OPENTOONZ_SUPERLU_INCLUDE_DIR}"
    "-DSUPERLU_LIBRARY=${envVars.OPENTOONZ_SUPERLU_LIBRARY}"
  ];

  baseDerivation = {
    pname = "opentoonz";
    version = "dev";
    inherit src;

    nativeBuildInputs = nativeInputs ++ [ qt.wrapQtAppsHook ];
    inherit buildInputs;

    CMAKE_PREFIX_PATH = cmakePrefixPath;
    PKG_CONFIG_PATH = pkgConfigPath;
  } // envVars;
in
{
  inherit cmakeFlags;

  devShell = pkgs.mkShell (
    {
      packages = nativeInputs ++ buildInputs ++ [
        pkgs.nixfmt
      ];
    } // envVars // {
      shellHook = ''
        export OPENTOONZ_NIX=1
        ${darwinCompilerSetup}
        export CC="${envVars.CC}"
        export CXX="${envVars.CXX}"
        export OPENTOONZ_BUILD_DIR="''${OPENTOONZ_BUILD_DIR:-$PWD/toonz/build/${defaultBuildDirName}}"
        export CMAKE_PREFIX_PATH="${cmakePrefixPath}''${CMAKE_PREFIX_PATH:+:$CMAKE_PREFIX_PATH}"
        export PKG_CONFIG_PATH="${pkgConfigPath}''${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
        export LD_LIBRARY_PATH="${libraryPath}''${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
        export DYLD_FALLBACK_LIBRARY_PATH="${libraryPath}''${DYLD_FALLBACK_LIBRARY_PATH:+:$DYLD_FALLBACK_LIBRARY_PATH}"
        export MISE_DISABLE_VERSION_CHECK=1
        export MISE_DISABLE_TOOLS=1
      '';
    }
  );

  package = stdenv.mkDerivation (
    baseDerivation
    // {
      configurePhase = ''
        runHook preConfigure
        ${darwinCompilerSetup}
        bash scripts/nix/prepare-tiff.sh
        cmake -S toonz/sources -B build ${lib.escapeShellArgs cmakeFlags} -DCMAKE_INSTALL_PREFIX="$out"
        runHook postConfigure
      '';

      buildPhase = ''
        runHook preBuild
        cmake --build build --parallel "$NIX_BUILD_CORES"
        runHook postBuild
      '';

      installPhase =
        if stdenv.isDarwin then
          ''
            runHook preInstall
            mkdir -p "$out/Applications"
            cp -R build/toonz/OpenToonz.app "$out/Applications/"
            cp -R stuff "$out/Applications/OpenToonz.app/portablestuff"
            runHook postInstall
          ''
        else
          ''
            runHook preInstall
            cmake --install build
            runHook postInstall
          '';
    }
  );

  configureCheck = stdenv.mkDerivation (
    baseDerivation
    // {
      pname = "opentoonz-configure-check";
      dontBuild = true;

      configurePhase = ''
        runHook preConfigure
        ${darwinCompilerSetup}
        bash scripts/nix/prepare-tiff.sh
        cmake -S toonz/sources -B build ${lib.escapeShellArgs cmakeFlags} -DCMAKE_INSTALL_PREFIX="$out"
        runHook postConfigure
      '';

      installPhase = ''
        runHook preInstall
        touch "$out"
        runHook postInstall
      '';
    }
  );
}
