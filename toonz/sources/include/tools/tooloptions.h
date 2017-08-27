#pragma once

#ifndef TOOLOPTIONS_H
#define TOOLOPTIONS_H

// TnzQt includes
#include "toonzqt/checkbox.h"

// TnzLib includes
#include "toonz/tstageobject.h"

// TnzCore includes
#include "tcommon.h"
#include "tproperty.h"

// Qt includes
#include <QFrame>
#include <QAction>
#include <QList>
#include <QToolBar>
#include <QMap>

// STD includes
#include <map>

#undef DVAPI
#undef DVVAR
#ifdef TNZTOOLS_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//=============================================================================

//    Forward declarations

class TTool;
class ToolOptionToolBar;
class TPropertyGroup;
class TPaletteHandle;
class TFrameHandle;
class TObjectHandle;
class TXsheetHandle;
class ToolHandle;
class SelectionScaleField;
class SelectionRotationField;
class SelectionMoveField;
class ToolOptionSlider;
class ToolOptionIntSlider;
class ThickChangeField;
class ToolOptionCombo;
class ToolOptionCheckbox;
class PegbarChannelField;
class ToolOptionPairSlider;
class ToolOptionControl;
class ToolOptionPopupButton;
class TXshLevelHandle;
class NoScaleField;
class PegbarCenterField;
class RGBLabel;
class MeasuredValueField;
class PaletteController;

class QLabel;
class QPushButton;
class QPropertyAnimation;
class QFrame;
class QHBoxLayout;
class QComboBox;
class QStackedWidget;

//=============================================================================

// Preprocessor definitions

#define TOOL_OPTIONS_LEFT_MARGIN 5

//=============================================================================

//***********************************************************************************************
//    ToolOptionToolBar  declaration
//***********************************************************************************************

class ToolOptionToolBar final : public QToolBar {
public:
  ToolOptionToolBar(QWidget *parent = 0);

  void addSpacing(int width);
};

//***********************************************************************************************
//    ToolOptionsBox  declaration
//***********************************************************************************************

class ToolOptionsBox : public QFrame {
  Q_OBJECT

protected:
  QMap<std::string, ToolOptionControl *>
      m_controls;  //!< property name -> ToolOptionControl
  QMap<std::string, QLabel *> m_labels;

  QHBoxLayout *m_layout;

public:
  ToolOptionsBox(QWidget *parent);
  ~ToolOptionsBox();

  virtual void
  updateStatus();  //!< Invokes updateStatus() on all registered controls
  virtual void onStageObjectChange() {}

  QHBoxLayout *hLayout() { return m_layout; }
  void addControl(ToolOptionControl *control);

  ToolOptionControl *control(const std::string &controlName) const;

  QLabel *addLabel(QString name);
  void addLabel(std::string propName, QLabel *label);
  void addSeparator();
};

//***********************************************************************************************
//    ToolOptionControlBuilder  declaration
//***********************************************************************************************

class ToolOptionControlBuilder final : public TProperty::Visitor {
  ToolOptionsBox *m_panel;
  TTool *m_tool;
  TPaletteHandle *m_pltHandle;
  ToolHandle *m_toolHandle;

  int m_singleValueWidgetType;
  int m_enumWidgetType;

public:
  ToolOptionControlBuilder(ToolOptionsBox *panel, TTool *tool,
                           TPaletteHandle *pltHandle,
                           ToolHandle *toolHandle = 0);

  enum SingleValueWidgetType { SLIDER = 0, FIELD };
  void setSingleValueWidgetType(int type) { m_singleValueWidgetType = type; }

  enum EnumWidgetType { COMBOBOX = 0, POPUPBUTTON };
  void setEnumWidgetType(int type) { m_enumWidgetType = type; }

private:
  QHBoxLayout *hLayout() { return m_panel->hLayout(); }
  QLabel *addLabel(TProperty *p);

  void visit(TDoubleProperty *p);
  void visit(TDoublePairProperty *p);
  void visit(TIntPairProperty *p);
  void visit(TIntProperty *p);
  void visit(TBoolProperty *p);
  void visit(TStringProperty *p);
  void visit(TEnumProperty *p);
  void visit(TStyleIndexProperty *p);
  void visit(TPointerProperty *p);
};

//***********************************************************************************************
//    GenericToolOptionsBox  declaration
//***********************************************************************************************

class GenericToolOptionsBox : public ToolOptionsBox {
public:
  GenericToolOptionsBox(QWidget *parent, TTool *tool, TPaletteHandle *pltHandle,
                        int propertyGroupIdx = 0, ToolHandle *toolHandle = 0);
};

//=============================================================================
//
// ArrowToolOptionsBox
//
//=============================================================================

class ArrowToolOptionsBox final : public ToolOptionsBox {
  Q_OBJECT

  TPropertyGroup *m_pg;
  bool m_splined;
  TTool *m_tool;
  TFrameHandle *m_frameHandle;
  TObjectHandle *m_objHandle;
  TXsheetHandle *m_xshHandle;

  QStackedWidget *m_mainStackedWidget;

  // General
  ToolOptionCombo *m_chooseActiveAxisCombo;
  ToolOptionCombo *m_pickCombo;
  // enable to choose the target pegbar from the combobox
  QComboBox *m_currentStageObjectCombo;

  // Position
  PegbarChannelField *m_motionPathPosField;
  PegbarChannelField *m_ewPosField;
  PegbarChannelField *m_nsPosField;
  QLabel *m_ewPosLabel;
  QLabel *m_nsPosLabel;
  PegbarChannelField *m_zField;
  NoScaleField *m_noScaleZField;
  ToolOptionCheckbox *m_lockEWPosCheckbox;
  ToolOptionCheckbox *m_lockNSPosCheckbox;

  // SO = Stacked Order
  QLabel *m_soLabel;
  PegbarChannelField *m_soField;

  // Rotation
  PegbarChannelField *m_rotationField;

  // Scale
  PegbarChannelField *m_globalScaleField;
  PegbarChannelField *m_scaleHField;
  PegbarChannelField *m_scaleVField;
  ToolOptionCheckbox *m_lockScaleHCheckbox;
  ToolOptionCheckbox *m_lockScaleVCheckbox;
  ToolOptionCombo *m_maintainCombo;

  // Shear
  PegbarChannelField *m_shearHField;
  PegbarChannelField *m_shearVField;
  ToolOptionCheckbox *m_lockShearHCheckbox;
  ToolOptionCheckbox *m_lockShearVCheckbox;

  // Center Position
  PegbarCenterField *m_ewCenterField;
  PegbarCenterField *m_nsCenterField;
  ToolOptionCheckbox *m_lockEWCenterCheckbox;
  ToolOptionCheckbox *m_lockNSCenterCheckbox;

  ToolOptionCheckbox *m_globalKey;

public:
  ArrowToolOptionsBox(QWidget *parent, TTool *tool, TPropertyGroup *pg,
                      TFrameHandle *frameHandle, TObjectHandle *objHandle,
                      TXsheetHandle *xshHandle, ToolHandle *toolHandle);

  void updateStatus();
  void onStageObjectChange();

protected:
  void showEvent(QShowEvent *);
  void hideEvent(QShowEvent *);

  void setSplined(bool on);
  bool isCurrentObjectSplined() const;

protected slots:
  void onFrameSwitched() { updateStatus(); }
  // update the object list in combobox
  void updateStageObjectComboItems();
  // syncronize the current item in the combobox to the selected stage object
  void syncCurrentStageObjectComboItem();
  // change the current stage object when user changes it via combobox by hand
  void onCurrentStageObjectComboActivated(int index);
};

//=============================================================================
//
// SelectionToolOptionsBox
//
//=============================================================================

class IconViewField final : public QWidget {
  Q_OBJECT

public:
  enum IconType {
    Icon_ScalePeg = 0,
    Icon_Rotation,
    Icon_Position,
    Icon_Thickness,
    Icon_Amount
  };

private:
  IconType m_iconType;

protected:
  QPixmap m_pm[Icon_Amount];
  Q_PROPERTY(
      QPixmap ScalePegPixmap READ getScalePegPixmap WRITE setScalePegPixmap);
  Q_PROPERTY(
      QPixmap RotationPixmap READ getRotationPixmap WRITE setRotationPixmap);
  Q_PROPERTY(
      QPixmap PositionPixmap READ getPositionPixmap WRITE setPositionPixmap);
  Q_PROPERTY(
      QPixmap ThicknessPixmap READ getThicknessPixmap WRITE setThicknessPixmap);

public:
  IconViewField(QWidget *parent = 0, IconType iconType = Icon_ScalePeg);

  QPixmap getScalePegPixmap() const { return m_pm[Icon_ScalePeg]; }
  void setScalePegPixmap(const QPixmap &pixmap) {
    m_pm[Icon_ScalePeg] = pixmap;
  }
  QPixmap getRotationPixmap() const { return m_pm[Icon_Rotation]; }
  void setRotationPixmap(const QPixmap &pixmap) {
    m_pm[Icon_Rotation] = pixmap;
  }
  QPixmap getPositionPixmap() const { return m_pm[Icon_Position]; }
  void setPositionPixmap(const QPixmap &pixmap) {
    m_pm[Icon_Position] = pixmap;
  }
  QPixmap getThicknessPixmap() const { return m_pm[Icon_Thickness]; }
  void setThicknessPixmap(const QPixmap &pixmap) {
    m_pm[Icon_Thickness] = pixmap;
  }

protected:
  void paintEvent(QPaintEvent *e);
};

//-----------------------------------------------------------------------------

class SelectionToolOptionsBox final : public ToolOptionsBox,
                                      public TProperty::Listener {
  Q_OBJECT

  TTool *m_tool;

  ToolOptionCheckbox *m_setSaveboxCheckbox;
  bool m_isVectorSelction;
  QLabel *m_scaleXLabel;
  SelectionScaleField *m_scaleXField;
  QLabel *m_scaleYLabel;
  SelectionScaleField *m_scaleYField;
  DVGui::CheckBox *m_scaleLink;
  SelectionRotationField *m_rotationField;
  QLabel *m_moveXLabel;
  SelectionMoveField *m_moveXField;
  QLabel *m_moveYLabel;
  SelectionMoveField *m_moveYField;
  ThickChangeField *m_thickChangeField;

  ToolOptionPopupButton *m_capStyle;
  ToolOptionPopupButton *m_joinStyle;
  ToolOptionIntSlider *m_miterField;

public:
  SelectionToolOptionsBox(QWidget *parent, TTool *tool,
                          TPaletteHandle *pltHandle, ToolHandle *toolHandle);

  void updateStatus();
  void onPropertyChanged();

protected slots:
  void onScaleXValueChanged();
  void onScaleYValueChanged();
  void onSetSaveboxCheckboxChanged(bool);
};

//=============================================================================
//
// GeometricToolOptionsBox
//
//=============================================================================

class GeometricToolOptionsBox final : public ToolOptionsBox {
  Q_OBJECT

  int m_targetType;

  QLabel *m_poligonSideLabel, *m_hardnessLabel;
  ToolOptionSlider *m_hardnessField;
  ToolOptionIntSlider *m_poligonSideField;
  ToolOptionCombo *m_shapeField;
  ToolOptionCheckbox *m_pencilMode;
  ToolOptionIntSlider *m_miterField;
  ToolOptionCheckbox *m_snapCheckbox;
  ToolOptionCombo *m_snapSensitivityCombo;
  TTool *m_tool;

public:
  GeometricToolOptionsBox(QWidget *parent, TTool *tool,
                          TPaletteHandle *pltHandle, ToolHandle *toolHandle);

  void updateStatus();

protected slots:
  void onShapeValueChanged();
  void onPencilModeToggled(bool);
  void onJoinStyleChanged(int);
};

//=============================================================================
//
// TypeToolOptionsBox
//
//=============================================================================

class TypeToolOptionsBox final : public ToolOptionsBox {
  Q_OBJECT

  TTool *m_tool;

public:
  TypeToolOptionsBox(QWidget *parent, TTool *tool, TPaletteHandle *pltHandle,
                     ToolHandle *toolHandle);

  void updateStatus();

protected slots:
  void onFieldChanged();
};

//=============================================================================
//
// PaintbrushToolOptionsBox
//
//=============================================================================

class PaintbrushToolOptionsBox final : public ToolOptionsBox {
  Q_OBJECT

  ToolOptionCombo *m_colorMode;
  ToolOptionCheckbox *m_selectiveMode;

public:
  PaintbrushToolOptionsBox(QWidget *parent, TTool *tool,
                           TPaletteHandle *pltHandle, ToolHandle *toolHandle);

  void updateStatus();

protected slots:
  void onColorModeChanged();
};

//=============================================================================
//
// FillToolOptionsBox
//
//=============================================================================

class FillToolOptionsBox final : public ToolOptionsBox {
  Q_OBJECT

  int m_targetType;
  QLabel *m_fillDepthLabel;
  ToolOptionCombo *m_colorMode, *m_toolType;
  ToolOptionCheckbox *m_selectiveMode, *m_segmentMode, *m_onionMode,
      *m_multiFrameMode;
  ToolOptionPairSlider *m_fillDepthField;

public:
  FillToolOptionsBox(QWidget *parent, TTool *tool, TPaletteHandle *pltHandle,
                     ToolHandle *toolHandle);

  void updateStatus();

protected slots:
  void onColorModeChanged();
  void onToolTypeChanged();
  void onOnionModeToggled(bool);
  void onMultiFrameModeToggled(bool);
};

//=============================================================================
//
// BrushToolOptionsBox
//
//=============================================================================

class BrushToolOptionsBox final : public ToolOptionsBox {
  Q_OBJECT

  TTool *m_tool;

  ToolOptionCheckbox *m_pencilMode;
  QLabel *m_hardnessLabel;
  ToolOptionSlider *m_hardnessField;
  ToolOptionPopupButton *m_joinStyleCombo;
  ToolOptionIntSlider *m_miterField;
  ToolOptionCombo *m_presetCombo;
  ToolOptionCheckbox *m_snapCheckbox;
  ToolOptionCombo *m_snapSensitivityCombo;
  QPushButton *m_addPresetButton;
  QPushButton *m_removePresetButton;

private:
  class PresetNamePopup;
  PresetNamePopup *m_presetNamePopup;
  void filterControls();

public:
  BrushToolOptionsBox(QWidget *parent, TTool *tool, TPaletteHandle *pltHandle,
                      ToolHandle *toolHandle);

  void updateStatus();

protected slots:

  void onPencilModeToggled(bool);
  void onAddPreset();
  void onRemovePreset();
};

//=============================================================================
//
// EraserToolOptionsBox
//
//=============================================================================

class EraserToolOptionsBox final : public ToolOptionsBox {
  Q_OBJECT

  ToolOptionCheckbox *m_pencilMode, *m_invertMode, *m_multiFrameMode;
  ToolOptionCombo *m_toolType, *m_colorMode;
  QLabel *m_hardnessLabel;
  ToolOptionSlider *m_hardnessField;

public:
  EraserToolOptionsBox(QWidget *parent, TTool *tool, TPaletteHandle *pltHandle,
                       ToolHandle *toolHandle);

  void updateStatus();

protected slots:
  void onPencilModeToggled(bool);
  void onToolTypeChanged();
  void onColorModeChanged();
};

//=============================================================================
//
// RulerToolOptionsBox
//
//=============================================================================

class RulerToolOptionsBox final : public ToolOptionsBox {
  Q_OBJECT

  MeasuredValueField *m_Xfld;
  MeasuredValueField *m_Yfld;
  MeasuredValueField *m_Wfld;
  MeasuredValueField *m_Hfld;
  MeasuredValueField *m_Afld;
  MeasuredValueField *m_Lfld;

  QLabel *m_XpixelFld;
  QLabel *m_YpixelFld;
  QLabel *m_WpixelFld;
  QLabel *m_HpixelFld;

  TTool *m_tool;

public:
  RulerToolOptionsBox(QWidget *parent, TTool *tool);

  void updateValues(bool isRasterLevelEditing, double X, double Y, double W,
                    double H, double A, double L, int Xpix = 0, int Ypix = 0,
                    int Wpix = 0, int Hpix = 0);

  void resetValues();
};

//=============================================================================
//
// TapeToolOptionsBox
//
//=============================================================================

class TapeToolOptionsBox final : public ToolOptionsBox {
  Q_OBJECT

  ToolOptionCheckbox *m_smoothMode, *m_joinStrokesMode;
  ToolOptionCombo *m_toolMode, *m_typeMode;
  QLabel *m_autocloseLabel;
  ToolOptionSlider *m_autocloseField;

public:
  TapeToolOptionsBox(QWidget *parent, TTool *tool, TPaletteHandle *pltHandle,
                     ToolHandle *toolHandle);

  void updateStatus();

protected slots:
  void onToolTypeChanged();
  void onToolModeChanged();
  void onJoinStrokesModeChanged();
};

//=============================================================================
//
// RGBPickerToolOptionsBox
//
//=============================================================================

class RGBPickerToolOptionsBox final : public ToolOptionsBox {
  Q_OBJECT
  ToolOptionCheckbox *m_realTimePickMode;
  // label with background color
  RGBLabel *m_currentRGBLabel;

public:
  RGBPickerToolOptionsBox(QWidget *parent, TTool *tool,
                          TPaletteHandle *pltHandle, ToolHandle *toolHandle,
                          PaletteController *paletteController);
  void updateStatus();
protected slots:
  void updateRealTimePickLabel(const QColor &);
};

//=============================================================================
//
// StylePickerToolOptionsBox
//
//=============================================================================

class StylePickerToolOptionsBox final : public ToolOptionsBox {
  Q_OBJECT
  ToolOptionCheckbox *m_realTimePickMode;

  QLabel *m_currentStyleLabel;

public:
  StylePickerToolOptionsBox(QWidget *parent, TTool *tool,
                            TPaletteHandle *pltHandle, ToolHandle *toolHandle,
                            PaletteController *paletteController);
  void updateStatus();
protected slots:
  void updateRealTimePickLabel(const int, const int, const int);
};

//-----------------------------------------------------------------------------

class DVAPI ToolOptions final : public QFrame {
  Q_OBJECT

  int m_width, m_height;
  std::map<TTool *, ToolOptionsBox *> m_panels;
  QWidget *m_panel;

public:
  ToolOptions();
  ~ToolOptions();

  QWidget *getPanel() const { return m_panel; }

protected:
  void showEvent(QShowEvent *);
  void hideEvent(QShowEvent *);

public slots:

  void onToolSwitched();
  void onToolChanged();
  void onStageObjectChange();

signals:
  // used in ComboViewer to handle Tab focus
  void newPanelCreated();

  //  void toolOptionChange();
};

#endif  // PANE_H
