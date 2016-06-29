#pragma once

#ifndef SCRIPTBINDING_OUTLINE_VECTORIZER_H
#define SCRIPTBINDING_OUTLINE_VECTORIZER_H

#include "toonz/scriptbinding.h"

class ToonzScene;
class TXshSimpleLevel;

namespace TScriptBinding {

class DVAPI OutlineVectorizer final : public Wrapper {
  Q_OBJECT
  NewOutlineConfiguration *m_parameters;

public:
  OutlineVectorizer();
  ~OutlineVectorizer();

  Q_INVOKABLE QScriptValue toString();
  WRAPPER_STD_METHODS(OutlineVectorizer)

  Q_INVOKABLE QScriptValue vectorize(QScriptValue img_or_level);

  Q_PROPERTY(int accuracy READ getAccuracy WRITE setAccuracy)
  int getAccuracy() const;
  void setAccuracy(int v);

  Q_PROPERTY(int despeckling READ getDespeckling WRITE setDespeckling)
  int getDespeckling() const;
  void setDespeckling(int v);

  Q_PROPERTY(bool preservePaintedAreas READ getPreservePaintedAreas WRITE
                 setPreservePaintedAreas)
  bool getPreservePaintedAreas() const;
  void setPreservePaintedAreas(bool v);

  Q_PROPERTY(
      double cornerAdherence READ getCornerAdherence WRITE setCornerAdherence)
  double getCornerAdherence() const;
  void setCornerAdherence(double v);

  Q_PROPERTY(double cornerAngle READ getCornerAngle WRITE setCornerAngle)
  double getCornerAngle() const;
  void setCornerAngle(double v);

  Q_PROPERTY(double cornerCurveRadius READ getCornerCurveRadius WRITE
                 setCornerCurveRadius)
  double getCornerCurveRadius() const;
  void setCornerCurveRadius(double v);

  Q_PROPERTY(int maxColors READ getMaxColors WRITE setMaxColors)
  int getMaxColors() const;
  void setMaxColors(int v);

  Q_PROPERTY(QString transparentColor READ getTransparentColor WRITE
                 setTransparentColor)
  QString getTransparentColor() const;
  void setTransparentColor(const QString &colorName);

  Q_PROPERTY(int toneThreshold READ getToneThreshold WRITE setToneThreshold)
  int getToneThreshold() const;
  void setToneThreshold(int v);
};

}  // namespace TScriptBinding

Q_DECLARE_METATYPE(TScriptBinding::OutlineVectorizer *)

#endif
