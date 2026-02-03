#pragma once

#ifndef SCRIPTBINDING_RASTERIZER_H
#define SCRIPTBINDING_RASTERIZER_H

#include "toonz/scriptbinding.h"

class ToonzScene;
class TXshSimpleLevel;

namespace TScriptBinding {

class DVAPI Rasterizer final : public Wrapper {
  Q_OBJECT
  bool m_colorMapped;
  int m_xres, m_yres;
  double m_dpi;
  bool m_antialiasing;

public:
  Rasterizer();
  ~Rasterizer();

  Q_INVOKABLE QScriptValue toString();
  WRAPPER_STD_METHODS(Rasterizer)

  Q_INVOKABLE QScriptValue rasterize(QScriptValue img);

  Q_PROPERTY(bool colorMapped READ getColorMapped WRITE setColorMapped)
  bool getColorMapped() const;
  void setColorMapped(bool v);

  Q_PROPERTY(int xres READ getXRes WRITE setXRes)
  int getXRes() const;
  void setXRes(int v);

  Q_PROPERTY(int yres READ getYRes WRITE setYRes)
  int getYRes() const;
  void setYRes(int v);

  Q_PROPERTY(double dpi READ getDpi WRITE setDpi)
  double getDpi() const;
  void setDpi(double v);

  Q_PROPERTY(bool antialiasing READ getAntialiasing WRITE setAntialiasing)
  bool getAntialiasing() const { return m_antialiasing; }
  void setAntialiasing(bool v) { m_antialiasing = v; }
};

}  // namespace TScriptBinding

Q_DECLARE_METATYPE(TScriptBinding::Rasterizer *)

#endif
