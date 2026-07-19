# Theme Development

## Built-in Themes (8)

| Theme | QSS File | Style |
|-------|----------|-------|
| Dark | `themes/Dark/Dark.qss` | Dark background (#303030), white icons |
| Light | `themes/Light/Light.qss` | Light background (#DBDBDB), black icons |
| Default | `themes/Default/Default.qss` | Medium-dark (#484848), white icons |
| Blue | `themes/Blue/Blue.qss` | Blue-tinted dark (#43464d) |
| Clay | `themes/Clay/Clay.qss` | Warm gray (#514d4c) |
| Neutral | `themes/Neutral/Neutral.qss` | Neutral gray (#808080), black icons |
| Synthwave | `themes/Synthwave/Synthwave.qss` | Cyberpunk dark (#2b313a) |
| Default-Green | `themes/Default-Green/Default-Green.qss` | Green variant of Default |

## QSS Architecture

Each `.qss` file is a standard Qt stylesheet with a custom `:TOONZCOLORS` block:

```css
:TOONZCOLORS {
  -bg-color: #303030;
  -icon-base-opacity: 0.8;
  -icon-disabled-opacity: 0.2;
  -icon-base-color: #cecece;
  -icon-active-color: white;
  -icon-selected-color: white;
  -icon-on-color: white;
  -icon-close-color: white;
  -icon-preview-color: black;
  -icon-vcheck-color: white;
  -icon-lock-color: white;
  -icon-keyframe-color: white;
  -icon-keyframe-modified-color: white;
}

QWidget {
  background-color: #303030;
  color: #cecece;
}
/* ... ~3300 lines of widget-specific styling ... */
```

## How Themes Work

### Layer 1: QSS Application

When a theme is selected, `AppContext::applyTheme()` calls:

```cpp
qApp->setStyleSheet(stylesheet);  // apply QSS globally
ThemeManager::getInstance()
    .parseCustomPropertiesFromStylesheet(stylesheet);  // parse :TOONZCOLORS
```

### Layer 2: ThemeManager

Parses the `:TOONZCOLORS { ... }` block and extracts:
- Icon opacities (base, disabled)
- Icon colors (base, active, on, selected, close, preview, lock, etc.)

These colors are used by `SvgIconEngine` to recolor SVG icons dynamically.

### Layer 3: Icon Path Rewriting

QSS files reference icons via relative paths like:
```css
image: url('../Default/imgs/white/checkmark/checkmark.svg');
```

`AppContext::loadStylesheet()` rewrites these to Qt resource paths:
```cpp
content.replace("../Default/imgs/black/", ":/icons/black/");
content.replace("../Default/imgs/white/", ":/icons/white/");
```

## Creating a New Theme

1. Copy an existing theme directory:
   ```bash
   cp -r resources/themes/Dark resources/themes/MyTheme
   ```

2. Rename the `.qss` file to match:
   ```bash
   mv resources/themes/MyTheme/Dark.qss resources/themes/MyTheme/MyTheme.qss
   ```

3. Edit `:TOONZCOLORS` and QSS rules as needed.

4. Register in `resources/resources.qrc`:
   ```xml
   <file alias="MyTheme.qss">themes/MyTheme/MyTheme.qss</file>
   ```

5. Rebuild. The theme is auto-discovered at runtime via `QDir(":/themes")`.

## SvgIconEngine

Renders SVG icons with dynamic theming:

```cpp
// Create a theme-colored icon
QIcon icon = createQIcon("lock");       // looks up "lock.svg" in /icons/
QIcon icon = createQIcon("lock", true); // for menu items (smaller size)
```

The engine:
1. Loads SVG content
2. Colorizes black pixels with `ThemeManager`'s active/base color
3. Adjusts opacity based on mode (Normal/Disabled/Active/Selected)
4. Caches rendered pixmaps in `QPixmapCache`

## ThemeManager API

```cpp
ThemeManager& tm = ThemeManager::getInstance();
tm.initialize();                         // preload icon metadata from :/icons
tm.parseCustomPropertiesFromStylesheet(qss);  // parse :TOONZCOLORS

// Color access
tm.getIconBaseColor();      // normal state
tm.getIconActiveColor();    // hover
tm.getIconOnColor();        // toggled on
tm.getIconSelectedColor();  // selected

// Opacity
tm.getIconBaseOpacity();     // default 0.8
tm.getIconDisabledOpacity(); // default 0.2
```

## Switching Themes at Runtime

```cpp
// From menu, toolbar, or code:
AppContext::instance()->setCurrentTheme("Dark");
// This calls:
// 1. saveSettings() to QSettings
// 2. applyTheme() → qApp->setStyleSheet() + ThemeManager::parse
// 3. emit themeChanged()
```
