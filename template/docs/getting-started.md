# Getting Started

## Prerequisites

- CMake 3.16+
- Qt 6.x (Widgets, Core, Gui, Svg)
- C++17 compiler (MSVC 2019+, GCC 10+, Clang 12+)

## Build

```bash
git clone https://github.com/daoxingtianxia/yz-ui-template.git
cd yz-ui-template
cmake -B build -DCMAKE_PREFIX_PATH=/path/to/qt6
cmake --build build
```

## Run

```bash
# Directly from build
./build/src/yz-ui-template

# Or create a standalone runtime
mkdir runtime
cp build/src/yz-ui-template.exe runtime/
windeployqt --dir runtime/ build/src/yz-ui-template.exe
cp -r translations/ runtime/
```

## Directory Structure

```
yz-ui-template/
├── CMakeLists.txt          # Top-level: Qt6 find, C++17
├── README.md
├── docs/                   # Developer guides
├── src/
│   ├── CMakeLists.txt      # Source build: executable + sources
│   ├── main.cpp            # Entry point: QApplication, theme, translator
│   ├── framework/          # Core UI framework (15 files)
│   │   ├── docklayout.h/.cpp       # DockLayout + Region tree engine
│   │   ├── dockwidget.h/.cpp       # DockWidget base (DockSeparator, DockPlaceholder)
│   │   ├── tdockwindows.h/.cpp     # TMainWindow + TDockWidget wrappers
│   │   ├── pane.h/.cpp             # TPanel + TPanelTitleBar + TPanelFactory
│   │   ├── mainwindow.h/.cpp       # Room + MainWindow + toolbars
│   │   ├── menubar.h/.cpp          # TopBar + RoomTabWidget + StackedMenuBar
│   │   ├── menubarcommand.h/.cpp   # CommandManager + DVAction + MenuItemHandler
│   │   ├── floatingpanelcommand.h/.cpp  # OpenFloatingPanel auto-discovery
│   │   ├── appcontext.h/.cpp       # AppContext: settings, theme, language
│   │   └── thememanager.h/.cpp     # ThemeManager + SvgIconEngine
│   └── panels/             # Demo panels (5 types)
│       ├── register.h/.cpp        # Factory + OpenFloatingPanel registration
│       ├── logpanel.h/.cpp
│       ├── propertypanel.h/.cpp
│       ├── canvaspanel.h/.cpp
│       ├── commandpalette.h/.cpp
│       └── welcomepanel.h/.cpp
├── resources/
│   ├── resources.qrc       # Themes + icons
│   ├── translations.qrc
│   ├── themes/             # 8 QSS theme files
│   └── icons/              # SVG icons (black/ + white/)
└── translations/
    ├── chinese/template.ts
    └── japanese/template.ts
```

## First Run

On first launch, three default Rooms are created:
- **Edit** — Canvas center + PropertyInspector right + CommandPalette bottom
- **Debug** — Log center + PropertyInspector right
- **Settings** — Welcome panel

Click the tabs to switch rooms. Drag panel title bars to rearrange. Close panels via X button, re-open from View menu.

Config files are created at `C:\Users\<user>\AppData\Roaming\YZ\YZ UI Template\`.
