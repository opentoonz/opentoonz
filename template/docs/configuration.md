# Configuration Files

All config is stored as XML at `AppData/Roaming/YZ/YZ UI Template/`.

## settings.xml

Written by `AppContext::saveSettings()`, read by `loadSettings()`.

```xml
<?xml version="1.0" encoding="UTF-8"?>
<settings>
  <group name="MainWindow">
    <value key="geometry">@ByteArray(...)</value>
    <value key="lastRoom">0</value>
  </group>
  <group name="Preferences">
    <value key="Theme">Dark</value>
    <value key="Language">English</value>
  </group>
</settings>
```

### Reading/Writing Settings

```cpp
AppContext* ctx = AppContext::instance();

// Read
QVariant theme = ctx->setting("Preferences", "Theme", "Dark");
int lastRoom = ctx->setting("MainWindow", "lastRoom", 0).toInt();

// Write
ctx->setSetting("Preferences", "Theme", "Dark");
ctx->setSetting("MainWindow", "geometry", saveGeometry());

// Persist to disk
ctx->saveSettings();
```

## rooms/<Name>.xml

Written by `Room::save()`, read by `Room::load()`.

```xml
<?xml version="1.0" encoding="UTF-8"?>
<room name="Edit">
  <panels>
    <panel index="0" name="Canvas" x="0" y="0" w="800" h="500"/>
    <panel index="1" name="PropertyInspector" x="800" y="0" w="300" h="500"/>
    <panel index="2" name="CommandPalette" x="0" y="500" w="1100" h="200"/>
  </panels>
  <hierarchy>(0,(1,2))</hierarchy>
</room>
```

- `name` — TPanelFactory type string
- `x, y, w, h` — geometry in the room
- `hierarchy` — DockLayout tree string (Region nesting)

## popups.xml

Written by `TPanel::~TPanel()`, read by `TPanel::restoreFloatingPanelState()`.

```xml
<?xml version="1.0" encoding="UTF-8"?>
<popups>
  <popup type="LogPanel">
    <geometry>AAAA/////...</geometry>  <!-- QWidget::saveGeometry() base64 -->
  </popup>
  <popup type="Canvas">
    <geometry>AAAA/////...</geometry>
  </popup>
</popups>
```

## shortcuts.xml

Written by `CommandManager::setShortcut()`, read by `CommandManager::loadShortcuts()`.

```xml
<?xml version="1.0" encoding="UTF-8"?>
<shortcuts>
  <shortcut id="MI_SaveLayout" value="Ctrl+S"/>
  <shortcut id="MI_Quit" value="Ctrl+Q"/>
  <shortcut id="MI_FullScreenWindow" value="Ctrl+`"/>
</shortcuts>
```

## Path Resolution

All paths use `QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)`:

| OS | Path |
|----|------|
| Windows | `C:\Users\<user>\AppData\Roaming\YZ\YZ UI Template\` |
| macOS | `~/Library/Application Support/YZ/YZ UI Template/` |
| Linux | `~/.local/share/YZ/YZ UI Template/` |

## Adding New Config Files

Use the `XmlSettings` pattern:

1. Create XML read/write with `QXmlStreamReader`/`QXmlStreamWriter`
2. Store values in `QMap<QString, QVariant>` at runtime
3. Call save on changes
4. See `AppContext::loadSettings()`/`saveSettings()` for reference
