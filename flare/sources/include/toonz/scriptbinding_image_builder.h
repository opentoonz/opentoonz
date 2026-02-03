#pragma once

#ifndef SCRIPTBINDING_IMAGE_BUILDER_H
#define SCRIPTBINDING_IMAGE_BUILDER_H

#include "toonz/scriptbinding.h"

class ToonzScene;
class TXshSimpleLevel;

namespace TScriptBinding {

class DVAPI Transform final : public Wrapper {
  Q_OBJECT
  TAffine m_affine;

public:
  Transform();
  Transform(const TAffine &aff);

  ~Transform();

  WRAPPER_STD_METHODS(Transform)
  Q_INVOKABLE QScriptValue toString();

  Q_INVOKABLE QScriptValue translate(double x, double y);
  Q_INVOKABLE QScriptValue rotate(double degrees);
  Q_INVOKABLE QScriptValue scale(double s);
  Q_INVOKABLE QScriptValue scale(double sx, double sy);

  const TAffine &getAffine() const { return m_affine; }
};

class DVAPI ImageBuilder final : public Wrapper {
  Q_OBJECT
  TImageP m_img;
  int m_width, m_height;

  QString add(const TImageP &img, const TAffine &aff);

public:
  ImageBuilder();
  ~ImageBuilder();

  WRAPPER_STD_METHODS(ImageBuilder)
  Q_INVOKABLE QScriptValue toString();

  Q_PROPERTY(QScriptValue image READ getImage)
  QScriptValue getImage();

  Q_INVOKABLE QScriptValue add(QScriptValue img);
  Q_INVOKABLE QScriptValue add(QScriptValue img, QScriptValue transformation);

  Q_INVOKABLE void clear();
  Q_INVOKABLE QScriptValue fill(const QString &colorName);
};

}  // namespace TScriptBinding

Q_DECLARE_METATYPE(TScriptBinding::Transform *)
Q_DECLARE_METATYPE(TScriptBinding::ImageBuilder *)

#endif
