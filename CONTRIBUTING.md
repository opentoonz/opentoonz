# How to contribute

This document describes some points about the contribution process for OpenToonz.

## Pull-requests

The OpenToonz organization loves any kind of contributions, such as fixing typos and code refactoring.
If you fixed or added something useful to OpenToonz, please send pull-requests to us.
We will first review the request, then we will accept it, add comments for rework, or decline it.

### Workflow

0. `fork` OpenToonz to your GitHub account from `opentoonz/opentoonz`.
  - (use the `fork` button at the https://github.com/opentoonz/opentoonz)
0. `clone` the repository.
  - `git clone git@github.com:your-github-account/opentoonz.git`
  - `git remote add upstream https://github.com/opentoonz/opentoonz.git`, additionally.
0. modify the codes.
  - `git checkout -b your-branch-name`
    - `your-branch-name` is a name of your modifications, for example,
      `fix/fatal-bugs`, `feature/new-useful-gui` and so on.
  - fix codes, then test them.
  - `git commit` them with good commit messages.
0. `pull` the latest changes form the `master` branch of the upstream.
  - `git pull upstream master` or `git pull --rebase upstream master`.
  - apply [clang-format](http://clang.llvm.org/docs/ClangFormat.html) with `toonz/sources/.clang-format`.
    - `cd toonz/sources`
    - `./beautification.sh` or `beautification.bat`.
  - `git commit` them.
  - `git push origin your-branch-name`.
0. make a pull request.

## Bugs

If you find bugs, please report details about them using [issues](https://github.com/opentoonz/opentoonz/issues).
Please include information needed to reproduce the bug, including the operating system 
and information directly relating to the issue. Links to screen captures of what is 
observed on screen or video of specific steps to produce the problem are very helpful.  
Then we will try to reproduce the bugs and fix them.
Unfortunately, bugs can sometimes only be reproduced in your own environment, 
so we cannot reproduce them. 
If you believe you can fix the bug, please submit a pull request.

## Features

If you had an idea about a new feature, please implement it and send a pull request to us.
If you cannot implement the feature, please open a topic on the [Google Group Page](https://groups.google.com/forum/#!forum/opentoonz_en).
It enables us to discuss implementations of the feature there.
Feature requests posted on GitHub without an active developer or funding will be closed 
to keep the issue tracker from becoming cluttered with unfulfilled feature requests.

## Translations

Translation source (`.ts`) files for OpenToonz GUI are located in `toonz/sources/translations`.
If you create new `.ts` files for your language or update an existing one,
please send us those modifications as pull-requests.
[Qt Linguist](http://doc.qt.io/qt-5.6/linguist-translators.html) is useful for translating them.

Please send us Qt message (`.qm`) files with `.ts` files if you can make the following modifications.

OpenToonz uses `.qm` files generated from `.ts` files.
You can generate `.qm` files by using [Qt Linguist](http://doc.qt.io/qt-5.6/linguist-translators.html).
Please locate generated `.qm` files in `stuff/config/loc`.
It enables the OpenToonz installer to install them into the `stuff` directory.
