# Setting up the development environment for BSD operating systems such as NetBSD, FreeBSD, Dragonfly BSD, or OpenBSD

## Required software

Building OpenToonz from source requires the following dependencies:
- Git
- GCC or Clang
- CMake (3.4.1 or newer).
- Qt5 (5.9 or newer)
- Boost (1.55 or newer)
- LibPNG
- SuperLU
- Lzo2
- FreeType
- LibMyPaint (1.3 or newer)
- Jpeg-Turbo (1.4 or newer)
- OpenCV 3.2 or newer

## OpenBSD example
```
# pkg_add git cmake pkgconf boost qt5 lz4 usb lzo2 png jpeg glew freeglut freetype json-c mypaint opencv
```
For programs such as SuperLU or Jpeg-Turbo, you might need to compile from source.

Notes:
* It's possible we also need `libgsl2` (Called gsl in BSD systems or maybe `libopenblas-dev` (called just blas in BSD.))
* We may also need `libegl1-mesa-dev libgles2-mesa-dev libglib2.0-dev liblzma-dev` Install the BSD equivilency to those GNU/Linux packages. They might not be available as precompiled packages, so compiling from source might be required.
* If your BSD operating system doesn't have the up to date packages such as gsl, you may install them by compiling from source.


Continue the instructions as stated in [the linux guide](https://github.com/opentoonz/opentoonz/blob/master/doc/how_to_build_linux.md) to finish the compilation.
