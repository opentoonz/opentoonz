#pragma once

#ifndef PICK_RGB_UTILS_H
#define PICK_RGB_UTILS_H

#include "tcommon.h"

#include <QRect>
#include <QRgb>

#undef DVAPI
#undef DVVAR
#ifdef TOONZQT_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//-------------------------------------------------------------------------------

//  Forward declarations

class QWidget;
class QOpenGLWidget;

//-------------------------------------------------------------------------------

//! Returns the average color displayed in a widget across the specified rect.
/*! This function reads the widget's frame buffer content on given rect, and
returns
    a mean of its pixels. The widget's background color is assumed outside the
    widget bounds.

\warning Use this function only on plain widgets - in case your widget inherits
         QOpenGLWidget, invoke the overloaded function accepting a
QOpenGLWidget* type
         instead.
*/
QRgb DVAPI pickRGB(QWidget *widget, const QRect &rect);
inline QRgb pickRGB(QWidget *widget, const QPoint &pos) {
  return pickRGB(widget, QRect(pos.x(), pos.y(), 1, 1));
}

//! Returns the average color displayed in a QOpenGLWidget instance across the
//! specified rect.
/*! This function reads the widget's front frame buffer on given rect, and
returns
    a mean of its pixels.

\warning Pixel colors are undefined outside the widget's geometry.
*/
QRgb DVAPI pickRGB(QOpenGLWidget *widget, const QRect &rect);
inline QRgb pickRGB(QOpenGLWidget *widget, const QPoint &pos) {
  return pickRGB(widget, QRect(pos.x(), pos.y(), 1, 1));
}

//! Returns the average color displayed on screen across the specified rect.
/*! This function reads the screen's buffer on given rect, and returns
    a mean of its pixels.

\warning In general, grabbing an area outside the screen is not safe.
         This depends on the underlying window system.
*/
QRgb DVAPI pickScreenRGB(const QRect &rect);
inline QRgb pickScreenRGB(const QPoint &pos) {
  return pickScreenRGB(QRect(pos.x(), pos.y(), 1, 1));
}

#endif  // PICK_RGB_UTILS_H
