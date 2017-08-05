#ifndef MYPAINTBRUSH_H
#define MYPAINTBRUSH_H

/* libmypaint - The MyPaint Brush Library
 * Copyright (C) 2008 Martin Renold <martinxyz@gmx.ch>
 * Copyright (C) 2012 Jon Nordby <jononor@gmail.com>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <mypaint-config.h>
#include <mypaint-surface.h>
#include <mypaint-brush-settings.h>

G_BEGIN_DECLS

typedef struct MyPaintBrush MyPaintBrush;

MyPaintBrush *
mypaint_brush_new(void);

void
mypaint_brush_unref(MyPaintBrush *self);
void
mypaint_brush_ref(MyPaintBrush *self);

void
mypaint_brush_reset(MyPaintBrush *self);

void
mypaint_brush_new_stroke(MyPaintBrush *self);

int
mypaint_brush_stroke_to(MyPaintBrush *self, MyPaintSurface *surface, float x, float y,
                        float pressure, float xtilt, float ytilt, double dtime);

void
mypaint_brush_set_base_value(MyPaintBrush *self, MyPaintBrushSetting id, float value);

float
mypaint_brush_get_base_value(MyPaintBrush *self, MyPaintBrushSetting id);

gboolean
mypaint_brush_is_constant(MyPaintBrush *self, MyPaintBrushSetting id);

int
mypaint_brush_get_inputs_used_n(MyPaintBrush *self, MyPaintBrushSetting id);

void
mypaint_brush_set_mapping_n(MyPaintBrush *self, MyPaintBrushSetting id, MyPaintBrushInput input, int n);

int
mypaint_brush_get_mapping_n(MyPaintBrush *self, MyPaintBrushSetting id, MyPaintBrushInput input);

void
mypaint_brush_set_mapping_point(MyPaintBrush *self, MyPaintBrushSetting id, MyPaintBrushInput input, int index, float x, float y);

void
mypaint_brush_get_mapping_point(MyPaintBrush *self, MyPaintBrushSetting id, MyPaintBrushInput input, int index, float *x, float *y);

float
mypaint_brush_get_state(MyPaintBrush *self, MyPaintBrushState i);

void
mypaint_brush_set_state(MyPaintBrush *self, MyPaintBrushState i, float value);

double
mypaint_brush_get_total_stroke_painting_time(MyPaintBrush *self);

void
mypaint_brush_set_print_inputs(MyPaintBrush *self, gboolean enabled);

void
mypaint_brush_from_defaults(MyPaintBrush *self);


gboolean
mypaint_brush_from_string(MyPaintBrush *self, const char *string);


G_END_DECLS

#endif // MYPAINTBRUSH_H
