#ifndef MYPAINTBRUSHSETTINGS_H
#define MYPAINTBRUSHSETTINGS_H

/* libmypaint - The MyPaint Brush Library
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
#include <mypaint-glib-compat.h>
#include <mypaint-brush-settings-gen.h>

G_BEGIN_DECLS

typedef struct {
    const gchar *cname;
    const gchar *name; // FIXME: needs to be accessed through gettext
    gboolean constant;
    float min;
    float def; // default
    float max;
    const gchar *tooltip; // FIXME: needs to be accessed through gettext
} MyPaintBrushSettingInfo;

const MyPaintBrushSettingInfo *
mypaint_brush_setting_info(MyPaintBrushSetting id);

const gchar *
mypaint_brush_setting_info_get_name(const MyPaintBrushSettingInfo *self);
const gchar *
mypaint_brush_setting_info_get_tooltip(const MyPaintBrushSettingInfo *self);
MyPaintBrushSetting
mypaint_brush_setting_from_cname(const char *cname);

typedef struct {
    const gchar *cname;
    float hard_min;
    float soft_min;
    float normal;
    float soft_max;
    float hard_max;
    const gchar *name; // FIXME: needs to be accessed through gettext
    const gchar *tooltip; // FIXME: needs to be accessed through gettext
} MyPaintBrushInputInfo;

const MyPaintBrushInputInfo *
mypaint_brush_input_info(MyPaintBrushInput id);

const gchar *
mypaint_brush_input_info_get_name(const MyPaintBrushInputInfo *self);

const gchar *
mypaint_brush_input_info_get_tooltip(const MyPaintBrushInputInfo *self);

MyPaintBrushInput
mypaint_brush_input_from_cname(const char *cname);

G_END_DECLS

#endif // MYPAINTBRUSHSETTINGS_H
