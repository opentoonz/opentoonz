cmake_minimum_required(VERSION 2.8.12)

project(mypaint LANGUAGES C)

find_package(json-c CONFIG REQUIRED)

include_directories(${JSON-C_INCLUDE_DIR})

set(sources
  brushmodes.c
  fifo.c
  helpers.c
  mypaint-brush-settings.c
  mypaint-brush.c
  mypaint-fixed-tiled-surface.c
  mypaint-mapping.c
  mypaint-matrix.c
  mypaint-rectangle.c
  mypaint-surface.c
  mypaint-symmetry.c
  mypaint-tiled-surface.c
  mypaint.c
  operationqueue.c
  rng-double.c
  tilemap.c
  utils.c
)
set(headers
  mypaint-config.h
  mypaint-glib-compat.h
  mypaint-mapping.h
  mypaint-matrix.h
  mypaint-symmetry.h
  config.h
  mypaint.h
)

add_library(libmypaint STATIC ${sources} ${headers})
