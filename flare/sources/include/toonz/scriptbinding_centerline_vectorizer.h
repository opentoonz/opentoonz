#pragma once

#ifndef SCRIPTBINDING_CENTERLINE_VECTORIZER_H
#define SCRIPTBINDING_CENTERLINE_VECTORIZER_H

#include "toonz/scriptbinding.h"

class ToonzScene;
class TXshSimpleLevel;

namespace TScriptBinding {

class DVAPI CenterlineVectorizer final : public Wrapper {
  Q_OBJECT
  CenterlineConfiguration *m_parameters;

public:
  CenterlineVectorizer();
  ~CenterlineVectorizer();

  Q_INVOKABLE QScriptValue toString();
  WRAPPER_STD_METHODS(CenterlineVectorizer)

  Q_INVOKABLE QScriptValue vectorize(QScriptValue img_or_level);

  Q_PROPERTY(int threshold READ getThreshold WRITE setThreshold)
  int getThreshold() const;
  void setThreshold(int v);

  Q_PROPERTY(int accuracy READ getAccuracy WRITE setAccuracy)
  int getAccuracy() const;
  void setAccuracy(int v);

  Q_PROPERTY(int despeckling READ getDespeckling WRITE setDespeckling)
  int getDespeckling() const;
  void setDespeckling(int v);

  Q_PROPERTY(double maxThickness READ getMaxThickness WRITE setMaxThickness)
  double getMaxThickness() const;
  void setMaxThickness(double v);

  Q_PROPERTY(double thicknessCalibration READ getThicknessCalibration WRITE
                 setThicknessCalibration)
  double getThicknessCalibration() const;
  void setThicknessCalibration(double v);

  Q_PROPERTY(bool preservePaintedAreas READ getPreservePaintedAreas WRITE
                 setPreservePaintedAreas)
  bool getPreservePaintedAreas() const;
  void setPreservePaintedAreas(bool v);

  Q_PROPERTY(bool addBorder READ getAddBorder WRITE setAddBorder)
  bool getAddBorder() const;
  void setAddBorder(bool v);

  Q_PROPERTY(bool eir READ getEir WRITE setEir)
  bool getEir() const;
  void setEir(bool v);

private:
  QScriptValue vectorizeImage(const TImageP &src1, TPalette *palette);
};

}  // namespace TScriptBinding

Q_DECLARE_METATYPE(TScriptBinding::CenterlineVectorizer *)

#endif
