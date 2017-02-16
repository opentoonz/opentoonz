#!/bin/bash
brew update
brew install qt55 glew lz4 lzo libusb
brew tap tcr/tcr
# Use older version of clang-format 
brew tap homebrew/versions
brew search clang-format
brew install homebrew/versions/clang-format38
# brew install clang-format
