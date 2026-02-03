#pragma once

#ifndef ASSISTANTLINE_INCLUDED
#define ASSISTANTLINE_INCLUDED


// TnzTools includes
#include <tools/assistant.h>
#include <tools/assistants/guidelineline.h>


// TnzCore includes
#include <tgl.h>


//*****************************************************************************************
//    TAssistantLine definition
//*****************************************************************************************

class TAssistantLine final : public TAssistant {
  Q_DECLARE_TR_FUNCTIONS(TAssistantVanishingPoint)
public:
  const TStringId m_idRestricktA;
  const TStringId m_idRestricktB;
  const TStringId m_idParallel;
  const TStringId m_idGrid;
  const TStringId m_idPerspective;

protected:
  TAssistantPoint &m_a;
  TAssistantPoint &m_b;
  TAssistantPoint &m_grid0;
  TAssistantPoint &m_grid1;

public:
  TAssistantLine(TMetaObject &object);

  static QString getLocalName();

  void updateTranslation() const override;

  inline bool getRestrictA() const
    { return data()[m_idRestricktA].getBool(); }
  inline bool getRestrictB() const
    { return data()[m_idRestricktB].getBool(); }
  inline bool getParallel() const
    { return data()[m_idParallel].getBool(); }
  inline bool getGrid() const
    { return data()[m_idGrid].getBool(); }
  inline bool getPerspective() const
    { return data()[m_idPerspective].getBool(); }

  void onDataChanged(const TVariant &value) override;

private:
  void fixGrid(const TPointD &prevA, const TPointD &prevB);

public:
  void onMovePoint(TAssistantPoint &point, const TPointD &position) override;

  void getGuidelines(
    const TPointD &position,
    const TAffine &toTool,
    const TPixelD &color,
    TGuidelineList &outGuidelines ) const override;

  static void drawRuler(
    const TPointD &a,
    const TPointD &b,
    const TPointD &grid0,
    const TPointD &grid1,
    const TPointD *perspectiveBase,
    double alpha );

  static void drawLine(
    const TAffine &matrix,
    const TAffine &matrixInv,
    double pixelSize,
    const TPointD &a,
    const TPointD &b,
    bool restrictA,
    bool restrictB,
    double alpha );

  static void drawGrid(
    const TPointD &a,
    const TPointD &b,
    const TPointD &grid0,
    const TPointD &grid1,
    bool restrictA,
    bool restrictB,
    bool perspective,
    double alpha );

  void draw(TToolViewer *viewer, bool enabled) const override;
};


#endif
