#pragma once

#ifndef ASSISTANTVANISHINGPOINT_INCLUDED
#define ASSISTANTVANISHINGPOINT_INCLUDED


// TnzTools includes
#include <tools/assistant.h>
#include <tools/assistants/guidelineline.h>

// TnzCore includes
#include <tgl.h>


//*****************************************************************************************
//    TAssistantVanishingPoint definition
//*****************************************************************************************

class TAssistantVanishingPoint final : public TAssistant {
  Q_DECLARE_TR_FUNCTIONS(TAssistantVanishingPoint)
public:
  const TStringId m_idPassThrough;
  const TStringId m_idGrid;
  const TStringId m_idPerspective;

protected:
  TAssistantPoint &m_center;
  TAssistantPoint &m_a0;
  TAssistantPoint &m_a1;
  TAssistantPoint &m_b0;
  TAssistantPoint &m_b1;
  TAssistantPoint &m_grid0;
  TAssistantPoint &m_grid1;

public:
  TAssistantVanishingPoint(TMetaObject &object);

  static QString getLocalName();

  void updateTranslation() const override;

  inline bool getPassThrough() const
    { return data()[m_idPassThrough].getBool(); }
  inline bool getGrid() const
    { return data()[m_idGrid].getBool(); }
  inline bool getPerspective() const
    { return data()[m_idPerspective].getBool(); }

  void onDataChanged(const TVariant &value) override;

private:
  void fixCenter();
  void fixSidePoint(TAssistantPoint &p0, TAssistantPoint &p1, TPointD previousP0);
  void fixSidePoint(TAssistantPoint &p0, TAssistantPoint &p1);
  void fixGrid1(const TPointD &previousCenter, const TPointD &previousGrid0);

public:
  void onFixPoints() override;
  void onMovePoint(TAssistantPoint &point, const TPointD &position) override;

  void getGuidelines(
    const TPointD &position,
    const TAffine &toTool,
    const TPixelD &color,
    TGuidelineList &outGuidelines ) const override;

  static void drawSimpleGrid(
    const TPointD &center,
    const TPointD &grid0,
    const TPointD &grid1,
    double alpha );
  
  static void drawPerspectiveGrid(
    const TPointD &center,
    const TPointD &grid0,
    const TPointD &grid1,
    double alpha );
  
  void draw(TToolViewer *viewer, bool enabled) const override;
  void drawEdit(TToolViewer *viewer) const override;
};


#endif
