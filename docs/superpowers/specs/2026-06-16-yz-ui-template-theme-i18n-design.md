# yz-ui-template Theme + i18n Port Design

## Goal

Port OpenToonz's theme switching (8 QSS themes + ThemeManager + SvgIconEngine) and language switching (QTranslator + QSettings persistence + restart) into the template.

## Scope

### Theme System
- Copy 8 QSS theme files from `stuff/config/qss/` to `template/resources/themes/`
- Copy icon SVGs from `stuff/config/qss/Default/imgs/` to `template/resources/icons/`
- Extract `ThemeManager` + `SvgIconEngine` from `gutil.h/cpp` into dedicated `thememanager.h/cpp`
- Strip toonz-specific icon metadata, keep QSS parsing + icon recoloring
- Add Qt resource file (.qrc) for themes and icons
- Add theme switching menu in TopBar, persist choice in QSettings
- Apply theme at startup via `qApp->setStyleSheet()`

### Language System
- Create 2 translation files: Chinese (zh_CN) + Japanese (ja_JP) as demo
- Load QTranslator at startup based on QSettings preference
- Add language submenu in TopBar
- Require restart for language change (matching OpenToonz behavior)

### Files Changed
- NEW: `template/resources/resources.qrc`
- NEW: `template/resources/themes/<8 dirs>/`
- NEW: `template/resources/icons/{black,white}/`
- NEW: `template/src/framework/thememanager.h/.cpp`
- NEW: `template/translations/chinese/template.ts`
- NEW: `template/translations/japanese/template.ts`
- MODIFY: `template/src/framework/appcontext.h/.cpp` — add theme/language state + signals
- MODIFY: `template/src/framework/menubar.h/.cpp` — add theme/language submenus
- MODIFY: `template/src/main.cpp` — load stylesheet + translator at startup
- MODIFY: `template/src/CMakeLists.txt` — add .qrc + thememanager
