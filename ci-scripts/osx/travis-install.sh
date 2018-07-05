#!/bin/bash
brew update
brew install glew lz4 lzo libusb libmypaint
brew tap tcr/tcr
brew install clang-format
# from Homebrew 1.6.0 the old formula for obtaining Qt5.9.2 becomes invalid.
# so we start to use the latest version of Qt. (#1910)
brew install qt