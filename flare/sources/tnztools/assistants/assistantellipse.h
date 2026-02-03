#pragma once

#ifndef ASSISTANTELLIPSE_INCLUDED
#define ASSISTANTELLIPSE_INCLUDED


// TnzTools includes
#include <tools/assistant.h>
#include <tools/assistants/guidelineline.h>
#include <tools/assistants/guidelineellipse.h>

// TnzCore includes
#include <tgl.h>

// std includes
#include <limits>


//*****************************************************************************************
//    TAssistantEllipse definition
//*****************************************************************************************

class TAssistantEllipse final : public TAssistant {
  Q_DECLARE_TR_FUNCTIONS(TAssistantEllipse)
public:
  const TStringId m_idCircle;
  const TStringId m_idRestrictA;
  const TStringId m_idRestrictB;
  const TStringId m_idRepeat;
  const TStringId m_idGrid;
  const TStringId m_idPerspective;
  const TStringId m_idPerspectiveDepth;

protected:
  TAssistantPoint &m_center;
  TAssistantPoint &m_a;
  TAssistantPoint &m_b;
  TAssistantPoint &m_grid0;
  TAssistantPoint &m_grid1;

public:
  TAssistantEllipse(TMetaObject &object);

  static QString getLocalName();

  void updateTranslation() const override;

  inline bool getCircle() const
    { return data()[m_idCircle].getBool(); }
  inline bool getRestrictA() const
    { return data()[m_idRestrictA].getBool(); }
  inline bool getRestrictB() const
    { return data()[m_idRestrictB].getBool(); }
  inline bool getRepeat() const
    { return data()[m_idRepeat].getBool(); }
  inline bool getGrid() const
    { return data()[m_idGrid].getBool(); }
  inline bool getPerspective() const
    { return data()[m_idPerspective].getBool(); }
  inline bool getPerspectiveDepth() const
    { return data()[m_idPerspectiveDepth].getBool(); }

  void onDataChanged(const TVariant &value) override;

private:
  void fixBAndGrid(
    TPointD prevCenter,
    TPointD prevA,
    TPointD prevB );

public:
  void onMovePoint(TAssistantPoint &point, const TPointD &position) override;

  TAffine calcEllipseMatrix() const;

  void getGuidelines(
    const TPointD &position,
    const TAffine &toTool,
    const TPixelD &color,
    TGuidelineList &outGuidelines ) const override;

  static void drawEllipseRanges(
    const TAngleRangeSet &ranges,
    const TAffine &ellipseMatrix,
    const TAffine &screenMatrixInv,
    double pixelSize,
    double alpha );

  static inline void drawEllipse(
    const TAffine &ellipseMatrix,
    const TAffine &screenMatrixInv,
    double pixelSize,
    double alpha )
      { drawEllipseRanges(TAngleRangeSet(true), ellipseMatrix, screenMatrixInv, pixelSize, alpha); }

  static void drawRuler(
    const TAffine &ellipseMatrix,
    const TPointD &grid0,
    const TPointD &grid1,
    bool perspective,
    double alpha );

  static void drawConcentricGrid(
    const TAffine &ellipseMatrix,
    const TPointD &grid0,
    const TPointD &grid1,
    bool perspective,
    double alpha );

  static void drawParallelGrid(
    const TAffine &ellipseMatrix,
    const TPointD &grid0,
    const TPointD &grid1,
    const TPointD *bound0,
    const TPointD *bound1,
    bool perspective,
    bool perspectiveDepth,
    bool repeat,
    double alpha );

private:
  void draw(
    const TAffine &ellipseMatrix,
    const TAffine &screenMatrixInv,
    double ox,
    double pixelSize,
    bool enabled ) const;

public:
  void draw(TToolViewer *viewer, bool enabled) const override;
};


#endif
