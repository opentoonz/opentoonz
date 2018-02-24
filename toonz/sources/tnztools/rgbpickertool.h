#pragma once

#ifndef RGBPICKERTOOL_H
#define RGBPICKERTOOL_H

#include "tools/tool.h"
#include "tproperty.h"
#include "toonz/strokegenerator.h"
#include "tools/tooloptions.h"

#include <QCoreApplication>

class RGBPickerTool final : public TTool {
  Q_DECLARE_TR_FUNCTIONS(RGBPickerTool)

  bool m_firstTime;
  int m_currentStyleId;
  TPixel32 m_oldValue, m_currentValue;

  TRectD m_selectingRect;
  TRectD m_drawingRect;
  TPropertyGroup m_prop;
  TEnumProperty m_pickType;

  TBoolProperty m_passivePick;
  std::vector<RGBPickerToolOptionsBox *> m_toolOptionsBox;

  // Aggiunte per disegnare il lazzo a la polyline
  StrokeGenerator m_drawingTrack;
  StrokeGenerator m_workingTrack;
  TPointD m_firstDrawingPos, m_firstWorkingPos;
  TPointD m_mousePosition;
  double m_thick;
  TStroke *m_stroke;
  TStroke *m_firstStroke;
  std::vector<TPointD> m_drawingPolyline;
  std::vector<TPointD> m_workingPolyline;
  bool m_makePick;

  TPointD m_mousePixelPosition;

public:
  RGBPickerTool();

  /*-- ToolOptionBox上にPassiveに拾った色を表示するため --*/
  void setToolOptionsBox(RGBPickerToolOptionsBox *toolOptionsBox);

  ToolType getToolType() const override { return TTool::LevelReadTool; }

  void updateTranslation() override;

  // Used to notify and set the currentColor outside the draw() methods:
  // using special style there was a conflict between the draw() methods of the
  // tool
  // and the genaration of the icon inside the style editor (makeIcon()) which
  // use
  // another glContext

  void onImageChanged() override;

  void draw() override;

  void leftButtonDown(const TPointD &pos, const TMouseEvent &e) override;

  void leftButtonDrag(const TPointD &pos, const TMouseEvent &e) override;

  void leftButtonUp(const TPointD &pos, const TMouseEvent &) override;

  void leftButtonDoubleClick(const TPointD &pos, const TMouseEvent &e) override;

  void mouseMove(const TPointD &pos, const TMouseEvent &e) override;

  void pick();

  void pickRect();

  void pickStroke();

  bool onPropertyChanged(std::string propertyName) override;

  void onActivate() override;

  TPropertyGroup *getProperties(int targetType) override;

  int getCursorId() const override;

  void doPolylineFreehandPick();

  void passivePick();

  //! Viene aggiunto \b pos a \b m_track e disegnato il primo pezzetto del
  //! lazzo. Viene inizializzato \b m_firstPos
  void startFreehand(const TPointD &drawingPos, const TPointD &workingPos);

  //! Viene aggiunto \b pos a \b m_track e disegnato un altro pezzetto del
  //! lazzo.
  void freehandDrag(const TPointD &drawingPos, const TPointD &workingPos);

  //! Viene chiuso il lazzo (si aggiunge l'ultimo punto ad m_track) e viene
  //! creato lo stroke rappresentante il lazzo.
  void closeFreehand();

  //! Viene aggiunto un punto al vettore m_polyline.
  void addPointPolyline(const TPointD &drawingPos, const TPointD &workingPos);

  //! Agginge l'ultimo pos a \b m_polyline e chiude la spezzata (aggiunge \b
  //! m_polyline.front() alla fine del vettore)
  void closePolyline(const TPointD &drawingPos, const TPointD &workingPos);

  /*--- RGBPickerToolをFlipbookで有効にする ---*/
  void showFlipPickedColor(const TPixel32 &pix);
};

#endif