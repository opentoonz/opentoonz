#!/bin/bash
brew update
brew install glew lz4 lzo libusb libmypaint
brew tap tcr/tcr
brew install clang-format
# obtain qt5.6 from the previous version of the formula
curl -O https://raw.githubusercontent.com/Homebrew/homebrew-core/fdfc724dd532345f5c6cdf47dc43e99654e6a5fd/Formula/qt5.rb
brew install ./qt5.rb
