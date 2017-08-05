#ifndef MYPAINTFIXEDTILEDSURFACE_H
#define MYPAINTFIXEDTILEDSURFACE_H

#include <mypaint-config.h>
#include <mypaint-glib-compat.h>
#include <mypaint-tiled-surface.h>

G_BEGIN_DECLS

/**
 * MyPaintFixedTiledSurface:
 *
 * Simple #MyPaintTiledSurface subclass that implements a fixed sized #MyPaintSurface.
 * Only intended for testing and trivial use-cases, and to serve as an example of
 * how to implement a tiled surface subclass.
 */
typedef struct MyPaintFixedTiledSurface MyPaintFixedTiledSurface;

MyPaintFixedTiledSurface *
mypaint_fixed_tiled_surface_new(int width, int height);

int
mypaint_fixed_tiled_surface_get_width(MyPaintFixedTiledSurface *self);

int
mypaint_fixed_tiled_surface_get_height(MyPaintFixedTiledSurface *self);


MyPaintSurface *
mypaint_fixed_tiled_surface_interface(MyPaintFixedTiledSurface *self);

G_END_DECLS

#endif // MYPAINTFIXEDTILEDSURFACE_H
