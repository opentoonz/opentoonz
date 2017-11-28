#!/bin/bash
brew update
brew install glew lz4 lzo libusb libmypaint
brew tap tcr/tcr
brew install clang-format
# obtain qt5.9.2 from the previous version of the formula
curl -O https://raw.githubusercontent.com/Homebrew/homebrew-core/9bc1bdd8d26747cffd7a18c31975f86cd0a97bc3/Formula/qt.rb
brew install ./qt.rb