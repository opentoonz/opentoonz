# How to contribute

This document describes some points about contribution process for OpenToonz.

## Pull-requests

OpenToonz organization loves any kinds of your contributions, such as fixing typos and code refactroing.
If you fixed or added something useful to the OpenToonz, please send pull-requests to us.
We review the request, then we accept it, or add comments for rework, or decline it.

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

If you found bugs, please report details about them using [issues](https://github.com/opentoonz/opentoonz/issues).
Then we will try to reproduce the bugs and fix them.
Unfortunately, sometimes bugs can be only reproduced in your your environment,
so we cannot reproduce them. We believe you can fix the bug and send us the fix.

## Features

If you had an idea about a new feature, please implement it and send a pull-request to us.
Even if you cannot implement the feature, you can open a topic in [issues](https://github.com/opentoonz/opentoonz/issues).
It enables us to discuss about implementaions of the feature there.

## Translations

Translation source (`.ts`) files for OpenToonz GUI are located in `toonz/sources/translations`.
If you create new `.ts` files for your language or polish existing ones,
please send us those modifications as pull-requests. 
[Qt Linguist](http://doc.qt.io/qt-5.6/linguist-translators.html) is usefull for translating them.

Please send us Qt message (`.qm`) files with `.ts` files if you can make the following modifications.

OpenToonz uses `.qm` files generated from `.ts` files.
You can generate `.qm` files by using [Qt Linguist](http://doc.qt.io/qt-5.6/linguist-translators.html).
Please locate generated `.qm` files in `stuff/config/loc`.
It enables OpenToonz installer to install them into the `stuff` directory.
