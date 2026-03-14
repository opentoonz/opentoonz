# Developing Stylesheets
Stylesheets are written with [LESS](http://lesscss.org/), which is a dynamic preprocessor stylesheet language that can be compiled into Qt stylesheets.

ℹ️ [LESS Functions](http://lesscss.org/functions/)

## Recommended Setup

### Windows
Although any LESS compiler will work fine, here is a recommended setup.

1. Install [Visual Studio Code](https://code.visualstudio.com/) by Microsoft.
2. Add the [Easy LESS](https://marketplace.visualstudio.com/items?itemName=mrcrowl.easy-less) extension (by **mrcrowl**) from the marketplace which will be used as the compiler (on save).
3. In VSCode, navigate to your OpenToonz stuff folder and open `config/qss/Default/less`.

A `settings.json` file is already included to ensure developers work to the same standards located in `.vscode`. If the file must be created manually then the following should apply.

``` json
  "editor.tabSize": 2,
  "less.compile": {
  "compress":  true,
  "sourceMap": false,
  "out":       false
}
```

ℹ️ [How to Change Settings in Visual Studio Code](https://code.visualstudio.com/docs/getstarted/settings).

### Linux

On Linux you can use the recommended VSCode setup or use a command-line compiler `lessc`.

Ubuntu:

    $ apt install node-less

Fedora:

    $ dnf install nodejs-less

## How To Compile

### Windows

The Easy LESS extension uses a **compile on save** feature, so the theme files must be saved to generate an output.

```
less/themes/default/default-theme.less
less/themes/others/default-green-theme.less
less/themes/others/blue-theme.less
less/themes/others/dark-theme.less
less/themes/others/clay-theme.less
less/themes/others/neutral-theme.less
less/themes/others/light-theme.less
less/themes/others/synthwave-theme.less
```

### Linux
From opentoonz source directory root execute the following commands:
```
$ lessc -x less/themes/default/default-theme.less stuff/config/qss/Default/Default.qss
$ lessc -x less/themes/others/default-green-theme.less stuff/config/qss/Default-Green/Default-Green.qss
$ lessc -x less/themes/others/blue-theme.less stuff/config/qss/Blue/Blue.qss
$ lessc -x less/themes/others/dark-theme.less stuff/config/qss/Dark/Dark.qss
$ lessc -x less/themes/others/clay-theme.less stuff/config/qss/Clay/Clay.qss
$ lessc -x less/themes/others/neutral-theme.less stuff/config/qss/Neutral/Neutral.qss
$ lessc -x less/themes/others/light-theme.less stuff/config/qss/Light/Light.qss
$ lessc -x less/themes/others/synthwave-theme.less stuff/config/qss/Synthwave/Synthwave.qss
```

## How They Work
The stylesheets use LESS to separate the layout structure from theme colors. A shared base skeleton defines the component structure without hard-coded colors, while individual theme files provide color variables.

Each theme is compiled with the same base layout to generate the final QSS files for different color schemes.

### Cascade Hierarchy 
The include pathway is important as certain parts need to be included before others so they can be overridden later. The important chain is as follows as defined in `_main.less`:

``` LESS
// Base
@import 'base/_styles.less';

// Layouts
@import 'layouts/_mainwindow.less'; // load mainwindow first!
@import 'layouts/_user-controls.less';

// Others (at this point any order is fine)
```

The `_mainwindows.less` file contains classes for ScrollBars, Tabs, and general window styles that other panels may override, so it is important this is included early.

## Variables
The themes use a variable-driven system where a set of base variables define the interface colors and properties. Many variables may be derived from others such as using LESS color functions like `darken()`. This allows a theme to change the entire interface appearance by overriding only a few base variables.

Example:

``` LESS
@bg-color: black;

@panel-bg-color: darken(@bg-color, 5%);
@border-color: darken(@bg-color, 15%);
```

If a theme overrides `@bg`, all derived colors automatically update. This keeps themes consistent and reduces the number of values that must be manually maintained.

For a full list of available color variables and properties, view `themes/default/default-theme.less`.

# Adding New Features
This is an example of how to add new features into the layout files.

``` LESS
#NewWindow {
  background-color: @bg-color;
  border: 1 solid @accent-color;
  
  & QPushButton#OK {
    background-color: @button-bg-color;
    border-color: @button-border-color;
    color: @button-text-color;
    padding: 3;
    &:hover {
      border-color: @hl-bg-color;
    }
  }
}
```

To reuse styles from another class you can reference another class as a mixin:

``` LESS
#NewWindow {
  .some-style(); // import properties
}
```

ℹ️ This is more stable than using `extend`, even if it produces a larger `.qss` output.

# Creating New Themes
It's possible to create custom themes.

## How To
1. Make sure you have a LESS compiler setup and working before proceeding.
2. Navigate to `stuff/config/qss/Default/less/themes/others`.
3. You can either duplicate an existing theme and rename it (pay attention to the header section to rename the `out` path) or create a new empty `.less` theme file.

If creating an empty file, make sure to include the required sections:

``` LESS
// out: "../../../../Theme-Name/Theme-Name.qss"

//! The above line is for EasyLESS (VSCode extension) compile output location

// -----------------------------------------------------------------------------
// THEME NAME
// -----------------------------------------------------------------------------

// Theme to inherit/override (this inherits colors/properties)
@import '../default/default-theme.less';

// Image path relative to where this *.qss is output
@img-url: '../Default/imgs/white';

// SETTINGS --------------------------------------------------------------------

@ui-contrast: 1.0; // multiplier

// COLORS ----------------------------------------------------------------------

@bg-color: green;
```

You only need to override most base variables, like `@bg-color`, if another variable referenced it using a LESS color function the updated color is automatically propgated.

A full list of possible color variables and properties can be found by viewing the default theme file at `themes/default/default-theme.less`.

# Development Guidelines
⚠️ Always develop using the **Default** theme first.

Most layout changes will automatically propgate to other themes. Only adjust other themes if a variable override is required.

⚠️ Avoid hard-coded colors in layout files.

Always use variables so themes remain compatible.

### Folder Responsibilities

**Base**
Reusable UI structures such as buttons, tabs, icon layouts, etc.

**Layouts**
Window-level layout definitions for widgets, panels and containers.

**Themes**
Color definitions that override base variables.

**Themes:** Alternate theme colors that inherit the Default theme, it is only necessary to override variable values unique to the theme.
