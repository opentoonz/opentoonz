-------
PURPOSE
-------

Astral was created to provide modularity for development but also minimalism in
design for end users.

--------------
HOW TO COMPILE
--------------

Any Less compiler that supports @import should do the trick.

Which Files Do I Compile?

  * /themes/Astral_001_Default.less (outputs the default theme)

----------
DEVELOPING
----------

Always develop the default theme first as all other themes inherit and override
from it.

* Alternate theme variants work by @importing the default theme or another
  theme that @imports [main.less].

* When working with alternate themes derived from the default theme, only use
  variables that need adjusting for that particular theme.

-------------------
CASCADE EXPLANATION
-------------------

Everything except the theme files should be imported into the [main.less] file.
This is the 'engine' file.

[main.less] should then be @imported into the default theme file at the top.

Alternate themes then @import the default theme file to inherit and create a
simple override scenario.

Order:

  > base
  > components
  > layouts
  > mods
  [main.less]

  [main.less] > [Astral_001_Default.less] > [other_themes.less] > [etc.less]

---------------------
DIRECTORY EXPLANATION
---------------------

[base]
------
Contains common blocks and variables used across multiple themes such as mixins.

[components]
------------
Contains extendable modules (placeholders) for use in the /layouts/ files, such
as buttons, icons, boxes, etc.

* Components should always be extended to, especially since they are used so
  extensively. They won't work otherwise, because we import components at the
  top of the cascade which allows us to override them lower down in the
  /layouts/, /themes/ or /mods/ files.

* Add a location comment to where you extend it so others can find it if
  necessary.

Example:

  [/components/example.less]
  --------------------------
  .componentName {
      background-color: red;
      color: blue;
  }
  
  [/layouts/example.less]
  -----------------------
  .selector {
    &:extend(.componentName all); // components/name.less
    color: red; // override etc
  }

[layouts]
---------
Contains the wire-framing of each window or widget.

  * Never "hard-code" color values into a layout file, always use a variable.

[mods]
------
Contains extendable override modules. These apply small adjustments. For example
we created a mod to adjust the QScrollBar within specific windows only. The mod
was re-used multiple times.

* Use mods the same way as a component.

* Mods must be imported last in the cascade.

* Mods are set as reference only, so if something exists in a mod file but isn't
  used in a layout file it won't be output in the QSS file.

[themes]
--------
Contains the variables needed to output a theme. For a list of all variables
its a good idea to look at the default theme file.
