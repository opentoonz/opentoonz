#pragma once

#ifndef INTFIELD_H
#define INTFIELD_H

#include "tcommon.h"
#include "toonzqt/lineedit.h"

#include <QToolBar>

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
class QSlider;
class QIntValidator;

//=============================================================================

namespace DVGui {

//=============================================================================
/*! \brief The RollerField class provides an object to change an integer value.

                Inherits \b QWidget.
*/
class DVAPI RollerField final : public QWidget {
  Q_OBJECT

  double m_value;
  double m_minValue;
  double m_maxValue;

  double m_step;

  int m_xPos;

public:
  RollerField(QWidget *parent = 0);

  ~RollerField() {}

  void setValue(double value);
  double getValue() const;

  void setRange(double minValue, double maxValue);
  void getRange(double &minValue, double &maxValue);

  void setStep(double _step) { m_step = _step; }

protected:
  void paintEvent(QPaintEvent *e) override;
  void mousePressEvent(QMouseEvent *) override;
  void mouseMoveEvent(QMouseEvent *) override;
  void mouseReleaseEvent(QMouseEvent *) override;

  void addValue(bool isDragging);
  void removeValue(bool isDragging);

signals:
  void valueChanged(bool isDragging);
};

//=============================================================================
/*! \brief The IntLineEdit class provides an object to manage an integer line
   edit.

                Inherits \b LineEdit, set an integer validator \b QIntValidator.

                You can pass to constructor current value, minimum and max value
   field
                or set this value using setValue(), setRange() .
*/
class DVAPI IntLineEdit : public LineEdit {
  Q_OBJECT

  QIntValidator *m_validator;
  //! The number of digits showed int the line edit.
  //! If digits is less than 1 the line edit show the natural number without
  //! prepend zeros.
  int m_showedDigits;
  int m_xMouse;
  bool m_mouseDragEditing = false;
  bool m_isTyping         = false;

public:
  IntLineEdit(QWidget *parent = 0, int value = 1,
              int minValue     = (-(std::numeric_limits<int>::max)()),
              int maxValue     = ((std::numeric_limits<int>::max)()),
              int showedDigits = 0);
  ~IntLineEdit() {}

  /*! Set text in field to \b value. */
  void setValue(int value);
  /*! Return an integer with text field value. */
  int getValue();

  /*! Set the range of field from \b minValue to \b maxValue;
                  set validator value. */
  void setRange(int minValue, int maxValue);
  /*! Set to \b minValue minimum range value.
                  \sa setRange() */
  void setBottomRange(int minValue);
  /*! Set to \b maxValue maximum range value.
                  \sa getRange() */
  void setTopRange(int maxValue);

  /*! Set \b minValue an \b maxValue to current range; to current
                  validator minimum and maximum value. */
  void getRange(int &minValue, int &maxValue);

  void setLineEditBackgroundColor(QColor color);

protected:
  /*! If focus is lost and current text value is out of range emit signal
                  \b editingFinished.*/
  void focusOutEvent(QFocusEvent *) override;

  // for dragging the mouse to set the value
  void mousePressEvent(QMouseEvent *) override;
  void mouseMoveEvent(QMouseEvent *) override;
  void mouseReleaseEvent(QMouseEvent *) override;
};

//=============================================================================
/*! \brief The IntField class provides to view an object to manage an integer
                parameter.

                Inherits \b QWidget.

                The class is composed of an horizontal layout which contains two
object,
                \li a vertical layout with a text field \b IntLineEdit and a
roller
                \li an horizontal slider QSlider.
                This objects are connected, so if you change one the other
automatically change.
                You can set current value getValue(), minimum and max value
using setValue(),
                setRange() or setValues(); by default is value = 0, minimum
value = 0
                and maximum value = 100.
\n	Maximum height object is fixed to \b DVGui::WidgetHeight, width depend
                on parent width.

                To know when integer parameter value change class provides a
signal, valueChanged();
                class emit signal when slider value change or when editing text
field is
                finished and current value is changed. Editing text field
finished may occur
                if focus is lost or enter key is pressed. See SLOT:
onSliderChanged(int value),
                onEditingFinished().
*/
class DVAPI IntField : public QWidget {
  Q_OBJECT

  RollerField *m_roller;
  IntLineEdit *m_lineEdit;
  QSlider *m_slider;
  bool m_isMaxRangeLimited;

public:
  IntField(QWidget *parent = 0, bool isMaxRangeLimited = true,
           bool isRollerHide = true);
  ~IntField() {}

  /*! Set to \b minValue and \b maxValue slider and text field range.
                  \sa getRange() */
  void setRange(int minValue, int maxValue);

  /*! Set \b minValue and \b maxValue to IntField range.
                  \sa setRange() */
  void getRange(int &minValue, int &maxValue);

  /*! Set to \b value current value IntField.
                  \sa getValue() */
  void setValue(int value);
  /*! Return current value.
                  \sa setValue() */
  int getValue();

  /*! This is provided for convenience.
                  Set to \b value current value IntField; set to \b minValue and
     \b maxValue
                  slider and text field range.
                  \sa getValue() */
  void setValues(int value, int minValue, int maxValue);

  /*! If \b enable is false set slider disable and hide it. */
  void enableSlider(bool enable);
  bool sliderIsEnabled();

  /*! If \b enable is false set roller disable and hide it. */
  void enableRoller(bool enable);
  bool rollerIsEnabled();

  void setLineEditBackgroundColor(QColor color);

protected slots:
  /*! Set to value the text field. If text field value is different from \b
     value
                  emit signal valueChanged(). */
  void onSliderChanged(int value);
  void onSliderReleased() { emit valueChanged(false); }

  /*! Set slider and roller value to current value in text field.
  \n	This protected slot is called when text editing is finished.
  \n	If slider value is different from text field value emit signal
  valueChanged(). */
  void onEditingFinished();
  /*! Set text field and slider to roller current value.
  \n	If slider and text field value are different from roller value
                  emit signal valueChanged(). */
  void onRollerValueChanged(bool isDragging);

signals:
  /*! This is a signal emitted when IntField, slider or text field, value
     change;
                  if slider position change or text field editing is finished.
                  \sa onEditingFinished() and onSliderChanged(int value). */
  void valueChanged(bool isDragging);

  /*! This signal is emitted only when the user edit the value by hand */
  void valueEditedByHand();
};

//-----------------------------------------------------------------------------
}  // namespace DVGui
//-----------------------------------------------------------------------------

#endif  // INTFIELD_H
