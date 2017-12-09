#pragma once

#ifndef DOUBLEFIELD_H
#define DOUBLEFIELD_H

#include "tcommon.h"
#include "toonzqt/intfield.h"

#undef DVAPI
#undef DVVAR
#ifdef TOONZQT_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

// forward declaration
class QDoubleValidator;
class QSlider;
class TMeasuredValue;

//=============================================================================

namespace DVGui {

//=============================================================================
/*! \brief The DoubleValueLineEdit is abstract class.
                Inherits \b LineEdit.
*/
class DVAPI DoubleValueLineEdit : public LineEdit {
  Q_OBJECT

  int m_xMouse;

protected:
  bool m_mouseDragEditing = false;
  bool m_isTyping         = false;

public:
  DoubleValueLineEdit(QWidget *parent = 0) : LineEdit(parent) {}
  ~DoubleValueLineEdit() {}

  virtual void setValue(double value) = 0;
  virtual double getValue()           = 0;

  virtual void setRange(double minValue, double maxValue)   = 0;
  virtual void getRange(double &minValue, double &maxValue) = 0;

  virtual int getDecimals() { return 2; }

protected:
  /*! If focus is lost and current value is out of range emit signal \b
   * lostFocus. */
  void focusOutEvent(QFocusEvent *) override;

  // these are only used for mouse dragging
  // to set the value of the field.
  void mousePressEvent(QMouseEvent *) override;
  void mouseMoveEvent(QMouseEvent *) override;
  void mouseReleaseEvent(QMouseEvent *) override;

signals:
  /*! To emit when value change. */
  void valueChanged();
};

//=============================================================================
/*! \brief The DoubleValueField class provides to view an object to manage a
                                         generic double parameter, with two
cipher after decimal point.

                Inherits \b QWidget.

                The class is composed of an horizontal layout which contains two
object,
                \li a vertical layout with a text field \b PrimaryDoubleLineEdit
and a roller
                \li an horizontal slider QSlider.
                This objects are connected, so if you change one the other
automatically change.
                It's possible to set current value getValue(), minimum and max
value, getRange(),
                using setValue(), setRange() or setValues(); by default value =
0, minimum
                value = -100.0 and maximum value = 100.0.
\n	Maximum height object is fixed to \b DVGui::WidgetHeight, while width
depend
                on parent width.

                To know when double parameter value change class provides a
signal, valueChanged();
                class emit signal when slider value change or when editing text
field is
                finished and current value is changed. Editing text field
finished may occur
                if focus is lost or enter key is pressed. See SLOT:
onSliderChanged(int value),
                onEditingFinished().
*/
class DVAPI DoubleValueField : public QWidget {
  Q_OBJECT

protected:
  RollerField *m_roller;
  DoubleValueLineEdit *m_lineEdit;
  QSlider *m_slider;
  QWidget *m_spaceWidget;

public:
  DoubleValueField(QWidget *parent, DoubleValueLineEdit *lineEdit);
  ~DoubleValueField() {}

  void getRange(double &minValue, double &maxValue);
  void setRange(double minValue = 0, double maxValue = 0);

  void setValue(double value);
  double getValue();

  /*! This is provided for convenience.
                  Set to \b value current value DoubleField; set to \b minValue
     and \b maxValue
                  slider and text field range.
                  \sa getValue() */
  void setValues(double value, double minValue = 0, double maxValue = 0);

  /*! If \b enable is false set slider disable and hide it. */
  void enableSlider(bool enable);
  bool isSliderEnabled();

  /*! If \b enable is false set roller disable and hide it. */
  void enableRoller(bool enable);
  bool isRollerEnabled();

protected slots:
  /*! Set to value the text field and roller. If text field value is different
     from \b value
                  emit signal valueChanged(int value). */
  void onSliderChanged(int value);
  void onSliderReleased() { emit valueChanged(false); }

  /*! Set slider and roller value to current value in text field.
                  This protected slot is called when text editing is finished.
                  If slider value is different from text field value emit signal
     valueChanged(). */
  void onLineEditValueChanged();
  /*! Set text field and slider to roller current value.
  \n	If slider and text field value are different from roller value
                  emit signal valueChanged(). */
  void onRollerValueChanged(bool isDragging);

signals:
  /*!	This signal is emitted when DoubleFiel, slider, roller or text field,
     value change;
                  if slider position change or text field editing is finished.
                  \sa onEditingFinished() and onSliderChanged(int value). */
  void valueChanged(bool);

  /*! This signal is emitted only when users edited the value by hand
*/
  void valueEditedByHand();
};

//=============================================================================
/*! \brief The DoubleLineEdit class provides to view an object to manage a
   double field.

                Inherits \b LineEdit, set a double validator \b
   QDoubleValidator.

                You can pass to constructor current value, getValue(), minimum
   and max value
                field, getRange(), and number of decimals, getDecimals() or you
   can set this
                value using setValue(), setRange() and setDecimals().
*/
class DVAPI DoubleLineEdit final : public DoubleValueLineEdit {
  Q_OBJECT

  QDoubleValidator *m_validator;

public:
  DoubleLineEdit(QWidget *parent = 0, double value = 1);
  ~DoubleLineEdit() {}

  /*! Set text field value to double value \b value. */
  void setValue(double value) override;
  /*! Return a double with field value. */
  double getValue() override;

  /*! Set the range of field from \b minValue to \b maxValue;
                  set validator value. */
  void setRange(double minValue, double maxValue) override;
  /*! Set \b minValue an \b maxValue to current range; to current
                  validator minimum and maximum value. */
  void getRange(double &minValue, double &maxValue) override;

  /*! Set length of field value decimal part to \b decimals. */
  void setDecimals(int decimals);
  /*! Return length of field value decimal part. */
  int getDecimals() override;
};

//=============================================================================
/*! \brief The DoubleField class provides a DoubleValueField with a
   DoubleLineEdit.

                Inherits \b DoubleValueField.
*/

class DVAPI DoubleField : public DoubleValueField {
public:
  DoubleField(QWidget *parent = 0, bool isRollerHide = true, int decimals = 2);
  ~DoubleField() {}
};

//=============================================================================
/*! \brief The MeasuredDoubleLineEdit class provides to view an object to manage
            a double lineEdit with its misure.
*/
class DVAPI MeasuredDoubleLineEdit : public DoubleValueLineEdit {
  Q_OBJECT

  TMeasuredValue *m_value;
  double m_minValue;
  double m_maxValue;
  bool m_modified;
  double m_errorHighlighting;
  int m_errorHighlightingTimerId;
  int m_decimals;
  int m_xMouse;
  bool m_labelClicked = false;

public:
  MeasuredDoubleLineEdit(QWidget *parent = 0);
  ~MeasuredDoubleLineEdit();

  void setValue(double value) override;
  double getValue() override;

  void setRange(double minValue, double maxValue) override;
  void getRange(double &minValue, double &maxValue) override;

  void setMeasure(std::string measureName);

  void setDecimals(int decimals);
  int getDecimals() override;

  // called after setText()
  void postSetText() { onEditingFinished(); }

private:
  void valueToText();

protected:
  void timerEvent(QTimerEvent *e) override;
  // these are only used for mouse dragging
  // to set the value of the field.
  void mousePressEvent(QMouseEvent *) override;
  void mouseMoveEvent(QMouseEvent *) override;
  void mouseReleaseEvent(QMouseEvent *) override;

protected slots:

  void onEditingFinished();
  void onTextChanged(const QString &);

  // This allows the field to be set by dragging on the label
  void receiveMouseMove(QMouseEvent *event);
  void receiveMousePress(QMouseEvent *event);
  void receiveMouseRelease(QMouseEvent *event);
};

//=============================================================================
/*! \brief The MeasuredDoubleField class provides a DoubleValueField with a
   ValueLineEdit.

                Inherits \b PrimaryDoubleField.
*/

class DVAPI MeasuredDoubleField final : public DoubleValueField {
  Q_OBJECT

public:
  MeasuredDoubleField(QWidget *parent = 0, bool isRollerHide = true);
  ~MeasuredDoubleField() {}

  void setMeasure(std::string measureName);

  void setDecimals(int decimals);
};

//-----------------------------------------------------------------------------
}  // namespace DVGui
//-----------------------------------------------------------------------------

#endif  // DOUBLEFIELD_H
