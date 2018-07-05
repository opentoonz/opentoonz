# Developing Stylesheets
Stylesheets are written with [LESS](http://lesscss.org/), which is a dynamic preprocessor stylesheet language that can be compiled into Qt stylesheets.

ℹ️ [LESS Functions](http://lesscss.org/functions/)

## Recommended Setup
Although any LESS compiler will work fine, here is a recommended setup.

- Install [Visual Studio Code](https://code.visualstudio.com/) by Microsoft.
- Add the [Easy LESS](https://marketplace.visualstudio.com/items?itemName=mrcrowl.easy-less) extension from the marketplace which will be used as the compiler.
- In VSCode, navigate to your OpenToonz stuff folder and open `config/qss/Default/less`.

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

## How To Compile
Easy LESS uses a compile on save feature, so the theme files must be saved to generate an output.

```
Default.less
themes/Blue.less
themes/Dark.less
themes/Light.less
```

## How They Work
The stylesheets are designed into a component, wire-frame and palette structure similar to web design that exploits the cascade of the LESS language to generate multiple theme colors from a single layout. This method was used to prevent duplication.

### Cascade Hierarchy 
The include pathway is important as `components` need to be included before `layouts`. The cascade order is defined in the `main.less` file.

``` LESS
// Base
@import 'base/colors';
@import 'base/mixins';

// Components
@import 'components/all';

// Layouts
@import 'layouts/all';
```

Then import the `main.less` file into the theme file which will add values to every variable in `Default.less`.

## Variable List
There are many variables, for a full list look at `Default.less`, below is a list of the core variables.

`@bg` is the main background color for the interface, almost all other color variables use it as their base color and instead use color functions to either lighten or darken the value.

`@text-color` is the main text color for the interface.

`@accent` is used for accenting, such as borders, separators, boxes, wrappers, think of it as secondary.

`@hl-bg-color` changes the color for highlighted items and focused text fields, think of it as tertiary.

# Adding New Features
This is an example of how to add new features into the layout files.

``` LESS
#NewWindow {
  background-color: @bg;
  border: 1 solid @accent;
  
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

To call components, use the LESS extend function.

``` LESS
#NewWindow {
  &:extend(#ComponentName all);
}
```

ℹ️ [LESS Extend Docs](http://lesscss.org/features/#extend-feature)

# Creating New Themes
It's possible to create custom themes.

## How To
- Navigate to your OpenToonz stuff directory, and create a new folder in `config/qss`.
- Create a new text file in the new folder, give it the same name as the folder and save it with the `.less` extension.
- Open the LESS file and add the following line at the top.
- `// out: filename.qss`
- Which will control where Easy LESS outputs the compiled file on save.
- Next, choose which theme you want to base from, in this example the base will be Default.
- Add this line under the Easy LESS line but above everything else.
- `@import '../Default/less/Default.less'`
- Now the theme file is a clone of Default, so variables can be overridden.

``` LESS
//out: filename.qss

@import '../Default/less/Default.less';

@bg: red;
@text-color: white;
@hl-bg-color: yellow;

// etc
```

# Notes
⚠️ When developing, always develop with the Default theme, then decide later if any changes need porting to alternate themes, in most cases it won't be necessary.

⚠️ Layout files must never have hard-coded color values, color values **must always** be variable. Each function area of OpenToonz is split into a separate file.

### Directory Information
**Base:** Contains generic color palettes, legacy code, mixins and functions. It's fine to place junk code here.

**Components:** Anything re-usable is here, such as multiple tab structures, button styles, icon layouts, frames, wrappers and folder trees.

**Layouts:** The core wire-frame, every window, widget and control is designed here.

**Themes:** Alternate theme colors that inherit the Default theme, it is only necessary to override variable values unique to the theme.