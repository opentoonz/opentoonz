#pragma once

#ifndef SCRIPTBINDING_IMAGE_H
#define SCRIPTBINDING_IMAGE_H

#include "toonz/scriptbinding.h"

namespace TScriptBinding {

class DVAPI Image final : public Wrapper {
  Q_OBJECT
  TImageP m_img;

public:
  Image();
  Image(const TImageP img);
  Image(TImage *img);

  ~Image();

  WRAPPER_STD_METHODS(Image)
  Q_INVOKABLE QScriptValue toString();

  Q_PROPERTY(QString type READ getType)
  QString getType() const;

  const TImageP &getImg() const { return m_img; }
  void setImg(const TImageP &img) { m_img = img; }

  Q_PROPERTY(int width READ getWidth)
  Q_PROPERTY(int height READ getHeight)
  Q_PROPERTY(double dpi READ getDpi)
  int getWidth();
  int getHeight();
  double getDpi();

  Q_INVOKABLE QScriptValue load(const QScriptValue &fp);
  Q_INVOKABLE QScriptValue save(const QScriptValue &fp);
};

QScriptValue checkImage(QScriptContext *context, const QScriptValue &value,
                        Image *&img);

}  // namespace TScriptBinding

Q_DECLARE_METATYPE(TScriptBinding::Image *)

#endif
