#pragma once

#ifndef TOOL_OPTIONS_CONTROLS_INCLUDED
#define TOOL_OPTIONS_CONTROLS_INCLUDED

// TnzCore includes
#include "tproperty.h"

// TnzBase includes
#include "tunit.h"
#include "tdoubleparamrelayproperty.h"

// ToonzQt includes
#include "toonzqt/doublepairfield.h"
#include "toonzqt/intpairfield.h"
#include "toonzqt/intfield.h"
#include "toonzqt/styleindexlineedit.h"
#include "toonzqt/checkbox.h"
#include "toonzqt/doublefield.h"
#include "toonzqt/popupbutton.h"

// TnzLib includes
#include "toonz/txsheet.h"
#include "toonz/tstageobject.h"
#include "toonz/stageobjectutil.h"

// STD includes
#include <string>

// Qt includes
#include <QComboBox>
#include <QToolButton>
#include <QTimer>
#include <QLabel>

#undef DVAPI
#undef DVVAR
#ifdef TNZTOOLS_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

class TTool;
class TFrameHandle;
class TObjectHandle;
class TXsheetHandle;
class SelectionTool;
class ToolHandle;

//***********************************************************************************
//    ToolOptionControl  declaration
//***********************************************************************************

//! ToolOptionControl is the base class for tool property toolbar controls.
/*!
  This class implements the basic methods the allow toolbar controls to
  interact with the tool properties.
*/
class ToolOptionControl : public TProperty::Listener {
protected:
  std::string m_propertyName;
  TTool *m_tool;
  ToolHandle *m_toolHandle;

public:
  ToolOptionControl(TTool *tool, std::string propertyName,
                    ToolHandle *toolHandle = 0);

  const std::string &propertyName() const { return m_propertyName; }

  void onPropertyChanged() override { updateStatus(); }
  void notifyTool();
  // return true if the control is belonging to the visible viewer
  bool isInVisibleViewer(QWidget *widget);

  virtual void updateStatus() = 0;
};

//***********************************************************************************
//    ToolOptionControl derivative  declarations
//***********************************************************************************

class ToolOptionCheckbox final : public DVGui::CheckBox,
                                 public ToolOptionControl {
  Q_OBJECT

protected:
  TBoolProperty *m_property;

public:
  ToolOptionCheckbox(TTool *tool, TBoolProperty *property,
                     ToolHandle *toolHandle = 0, QWidget *parent = 0);
  void updateStatus() override;
public slots:
  void doClick(bool);

protected:
  void nextCheckState() override;
};

//-----------------------------------------------------------------------------

class ToolOptionSlider final : public DVGui::DoubleField,
                               public ToolOptionControl {
  Q_OBJECT

protected:
  TDoubleProperty *m_property;

public:
  ToolOptionSlider(TTool *tool, TDoubleProperty *property,
                   ToolHandle *toolHandle = 0);
  void updateStatus() override;

protected slots:

  void onValueChanged(bool isDragging);
  void increase();
  void decrease();
};

//-----------------------------------------------------------------------------

class ToolOptionPairSlider final : public DVGui::DoublePairField,
                                   public ToolOptionControl {
  Q_OBJECT

protected:
  TDoublePairProperty *m_property;

public:
  ToolOptionPairSlider(TTool *tool, TDoublePairProperty *property,
                       const QString &leftName, const QString &rightName,
                       ToolHandle *toolHandle = 0);
  void updateStatus() override;

protected slots:

  void onValuesChanged(bool isDragging);
  void increaseMaxValue();
  void decreaseMaxValue();
  void increaseMinValue();
  void decreaseMinValue();
};

//-----------------------------------------------------------------------------

class ToolOptionIntPairSlider final : public DVGui::IntPairField,
                                      public ToolOptionControl {
  Q_OBJECT

protected:
  TIntPairProperty *m_property;

public:
  ToolOptionIntPairSlider(TTool *tool, TIntPairProperty *property,
                          const QString &leftName, const QString &rightName,
                          ToolHandle *toolHandle = 0);
  void updateStatus() override;

protected slots:

  void onValuesChanged(bool isDragging);
  void increaseMaxValue();
  void decreaseMaxValue();
  void increaseMinValue();
  void decreaseMinValue();
};

//-----------------------------------------------------------------------------

class ToolOptionIntSlider final : public DVGui::IntField,
                                  public ToolOptionControl {
  Q_OBJECT

protected:
  TIntProperty *m_property;

public:
  ToolOptionIntSlider(TTool *tool, TIntProperty *property,
                      ToolHandle *toolHandle = 0);
  void updateStatus() override;

protected slots:

  void onValueChanged(bool isDragging);
  void increase();
  void decrease();
};

//-----------------------------------------------------------------------------

class ToolOptionCombo final : public QComboBox, public ToolOptionControl {
  Q_OBJECT

protected:
  TEnumProperty *m_property;

public:
  ToolOptionCombo(TTool *tool, TEnumProperty *property,
                  ToolHandle *toolHandle = 0);
  void loadEntries();
  void updateStatus() override;

public slots:

  void onActivated(int);
  void doShowPopup();
  void doOnActivated(int);
};

//-----------------------------------------------------------------------------

class ToolOptionPopupButton final : public PopupButton,
                                    public ToolOptionControl {
  Q_OBJECT

protected:
  TEnumProperty *m_property;

public:
  ToolOptionPopupButton(TTool *tool, TEnumProperty *property);
  void updateStatus() override;
  TEnumProperty *getProperty() { return m_property; }

public slots:

  void onActivated(int);
  void doShowPopup();
  void doSetCurrentIndex(int);
  void doOnActivated(int);
};

//-----------------------------------------------------------------------------

class ToolOptionTextField final : public DVGui::LineEdit,
                                  public ToolOptionControl {
  Q_OBJECT

protected:
  TStringProperty *m_property;

public:
  ToolOptionTextField(TTool *tool, TStringProperty *property);
  void updateStatus() override;

public slots:

  void onValueChanged();
};

//-----------------------------------------------------------------------------

class StyleIndexFieldAndChip final : public DVGui::StyleIndexLineEdit,
                                     public ToolOptionControl {
  Q_OBJECT

protected:
  TStyleIndexProperty *m_property;
  TPaletteHandle *m_pltHandle;

public:
  StyleIndexFieldAndChip(TTool *tool, TStyleIndexProperty *property,
                         TPaletteHandle *pltHandle, ToolHandle *toolHandle = 0);
  void updateStatus() override;

public slots:

  void onValueChanged(const QString &);
  void updateColor();
};

//-----------------------------------------------------------------------------

//! The ToolOptionMeasuredDoubleField class implements toolbar controls for
//! double properties that need to be displayed with a measure.
/*!
  This option control is useful to display function editor curves in the
  toolbar;
  in particular, it deals with the following tasks:

  \li Setting the preference-based keyframe interpolation type
  \li Editing with global keyframes (ie affecting multiple parameters other than
  the edited one)
  \li Undo/Redo of user interactions.
*/
class ToolOptionParamRelayField final : public DVGui::MeasuredDoubleLineEdit,
                                        public ToolOptionControl {
  Q_OBJECT

  TDoubleParamP m_param;  //!< Cached property param
  TMeasure *m_measure;    //!< Cached property param measure

protected:
  TDoubleParamRelayProperty
      *m_property;  //!< The TDoubleParam relaying property

  TBoolProperty *m_globalKey;     //!< The property enforcing global keys
  TPropertyGroup *m_globalGroup;  //!< The property group whose properties
                                  //!< are affected by m_globalKey
public:
  ToolOptionParamRelayField(TTool *tool, TDoubleParamRelayProperty *property,
                            int decimals = 2);

  void setGlobalKey(TBoolProperty *globalKey, TPropertyGroup *globalGroup);

  void updateStatus() override;

protected slots:

  virtual void onValueChanged();
};

//=============================================================================
//
// Widget specifici di ArrowTool (derivati da ToolOptionControl)
//
//=============================================================================

class DVAPI MeasuredValueField : public DVGui::LineEdit {
  Q_OBJECT

  TMeasuredValue *m_value;
  bool m_modified;
  double m_errorHighlighting;
  QTimer m_errorHighlightingTimer;
  int m_xMouse = -1;
  int m_precision;
  bool m_mouseEdit    = false;
  bool m_labelClicked = false;
  double m_originalValue;
  bool m_isTyping = false;

protected:
  bool m_isGlobalKeyframe;

  // these are used for mouse dragging to edit a value
  void mousePressEvent(QMouseEvent *) override;
  void mouseMoveEvent(QMouseEvent *) override;
  void mouseReleaseEvent(QMouseEvent *) override;
  void focusOutEvent(QFocusEvent *) override;

public:
  MeasuredValueField(QWidget *parent, QString name = "numfield");
  ~MeasuredValueField();

  void setMeasure(std::string name);

  void enableGlobalKeyframe(bool isGlobalKeyframe) {
    m_isGlobalKeyframe = isGlobalKeyframe;
  }

  TMeasuredValue *getMeasuredValue() const { return m_value; }

  void setValue(double v);
  double getValue() const;

  void setPrecision(int precision);
  int getPrecision() { return m_precision; }

protected slots:

  void commit();
  void onTextChanged(const QString &);
  void errorHighlightingTick();

  // clicking on the label connected to a field
  // can be used to drag and change the value
  void receiveMouseMove(QMouseEvent *event);
  void receiveMousePress(QMouseEvent *event);
  void receiveMouseRelease(QMouseEvent *event);

signals:

  void measuredValueChanged(TMeasuredValue *value, bool addToUndo = true);
};
//-----------------------------------------------------------------------------

class PegbarChannelField final : public MeasuredValueField,
                                 public ToolOptionControl {
  Q_OBJECT

  const enum TStageObject::Channel m_actionId;
  TFrameHandle *m_frameHandle;
  TObjectHandle *m_objHandle;
  TXsheetHandle *m_xshHandle;
  enum ScaleType { eNone = 0, eAR = 1, eMass = 2 } m_scaleType;
  TStageObjectValues m_before;
  bool m_firstMouseDrag = false;

public:
  PegbarChannelField(TTool *tool, enum TStageObject::Channel actionId,
                     QString name, TFrameHandle *frameHandle,
                     TObjectHandle *objHandle, TXsheetHandle *xshHandle,
                     QWidget *parent = 0);

  ~PegbarChannelField() {}

  void updateStatus() override;

public slots:

  void onScaleTypeChanged(int type);

protected slots:
  // add to undo is only false if mouse dragging to change the value
  // on mouse release, add to undo is true
  void onChange(TMeasuredValue *fld, bool addToUndo = true);
};

//-----------------------------------------------------------------------------

class DVAPI PegbarCenterField final : public MeasuredValueField,
                                      public ToolOptionControl {
  Q_OBJECT

  int m_index;  // 0 = x, 1 = y
  TObjectHandle *m_objHandle;
  TXsheetHandle *m_xshHandle;
  TPointD m_oldCenter;
  bool m_firstMouseDrag = false;

public:
  PegbarCenterField(TTool *tool, int index, QString name,
                    TObjectHandle *objHandle, TXsheetHandle *xshHandle,
                    QWidget *parent = 0);

  ~PegbarCenterField() {}

  void updateStatus() override;

protected slots:
  // add to undo is only false if mouse dragging to change the value
  // on mouse release, add to undo is true
  void onChange(TMeasuredValue *fld, bool addToUndo = true);
};

//-----------------------------------------------------------------------------

class NoScaleField final : public MeasuredValueField, public ToolOptionControl {
  Q_OBJECT

public:
  NoScaleField(TTool *tool, QString name);
  ~NoScaleField() {}

  void updateStatus() override;

protected slots:
  // add to undo is only false if mouse dragging to change the value
  // on mouse release, add to undo is true
  void onChange(TMeasuredValue *fld, bool addToUndo = true);
};

//-----------------------------------------------------------------------------

class PropertyMenuButton final : public QToolButton, public ToolOptionControl {
  Q_OBJECT

  QList<TBoolProperty *> m_properties;

public:
  PropertyMenuButton(
      QWidget *parent = 0, TTool *tool = 0,
      QList<TBoolProperty *> properties = QList<TBoolProperty *>(),
      QIcon icon = QIcon(), QString tooltip = QString());
  ~PropertyMenuButton() {}

  void updateStatus() override;

protected slots:

  void onActionTriggered(QAction *);

signals:

  void onPropertyChanged(QString name);
};

//-----------------------------------------------------------------------------

class SelectionScaleField final : public MeasuredValueField {
  Q_OBJECT

  int m_id;
  SelectionTool *m_tool;
  double m_originalValue;

public:
  SelectionScaleField(SelectionTool *tool, int actionId, QString name);

  ~SelectionScaleField() {}

  void updateStatus();
  bool applyChange(bool addToUndo = true);

protected slots:
  // add to undo is only false if mouse dragging to change the value
  // on mouse release, add to undo is true
  void onChange(TMeasuredValue *fld, bool addToUndo = true);

signals:
  void valueChange(bool addToUndo);
};

//-----------------------------------------------------------------------------

class SelectionRotationField final : public MeasuredValueField {
  Q_OBJECT

  SelectionTool *m_tool;

public:
  SelectionRotationField(SelectionTool *tool, QString name);

  ~SelectionRotationField() {}

  void updateStatus();

protected slots:
  // add to undo is only false if mouse dragging to change the value
  // on mouse release, add to undo is true
  void onChange(TMeasuredValue *fld, bool addToUndo = true);
};

//-----------------------------------------------------------------------------

class SelectionMoveField final : public MeasuredValueField {
  Q_OBJECT

  int m_id;
  SelectionTool *m_tool;

public:
  SelectionMoveField(SelectionTool *tool, int id, QString name);

  ~SelectionMoveField() {}

  void updateStatus();

protected slots:
  // add to undo is only false if mouse dragging to change the value
  // on mouse release, add to undo is true
  void onChange(TMeasuredValue *fld, bool addToUndo = true);
};

//-----------------------------------------------------------------------------

class ThickChangeField final : public MeasuredValueField {
  Q_OBJECT

  SelectionTool *m_tool;

public:
  ThickChangeField(SelectionTool *tool, QString name);

  ~ThickChangeField() {}

  void updateStatus();

protected slots:
  void onChange(TMeasuredValue *fld, bool addToUndo = true);
};

//-----------------------------------------------------------------------------

// The ClickableLabel class is used to allow click and dragging
// on a label to change the value of a linked field
class ClickableLabel : public QLabel {
  Q_OBJECT

protected:
  void mousePressEvent(QMouseEvent *) override;
  void mouseMoveEvent(QMouseEvent *) override;
  void mouseReleaseEvent(QMouseEvent *) override;

public:
  ClickableLabel(const QString &text, QWidget *parent = Q_NULLPTR,
                 Qt::WindowFlags f = Qt::WindowFlags());
  ~ClickableLabel();

signals:
  void onMousePress(QMouseEvent *event);
  void onMouseMove(QMouseEvent *event);
  void onMouseRelease(QMouseEvent *event);
};

//-----------------------------------------------------------------------------

#endif
