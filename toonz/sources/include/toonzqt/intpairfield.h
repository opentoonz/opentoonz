#pragma once

#ifndef INTPAIRFIELD_H
#define INTPAIRFIELD_H

#include "toonzqt/intfield.h"
#include "tcommon.h"

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
class QLabel;

//=============================================================================

namespace DVGui {

//=============================================================================
/*! \brief The IntPairField class provides to view an object to manage a pair
                                         of int parameter.

                Inherits \b QWidget.

                The object is composed of an horizontal layout \b QHBoxLayout
   which contains
                two pair [label, text field] and a slider with two grab, one for
   each int
                value to manage. [label, text field] are a \b QLabel and a \b
   IntLineEdit.
                Objects are inserted in the following order: a label and
   respective text field,
                the slider and the other pair [lebel,text field].
                Object size width is not fixed, while height is equal to
   DVGui::WidgetHeight, 20;
                labels width depend from its text length, text fields has fixed
   size, while
                slider width change in according with widget size.

                Objects contained in this widget are connected, so if you change
   one value the
                other automatically change, if it is necessary. You can set
   current value,
                getValues(), minimum and max value, getRange(), using
   setValues(), setRange().

                To know when one of two int parameter value change class
   provides a signal,
                valuesChanged(); class emit signal when a grab slider position
   change or when
                editing text field, left or right is finished and current value
   is changed.
                Editing text field finished may occur if focus is lost or enter
   key is pressed.
                See SLOT: onLeftEditingFinished() and onRightEditingFinished().

                \b Example:
                \code
                        IntPairField* intPairFieldExample = new
   IntPairField(this);
                        intPairFieldExample->setLeftText(tr("Left Value:"));
                        intPairFieldExample->setRightText(tr("Rigth Value:"));
                        intPairFieldExample->setRange(0,10);
                        intPairFieldExample->setValues(std::make_pair(3,8));
                \endcode
                \b Result:
                        \image html DoublePairField.jpg
*/
class DVAPI IntPairField : public QWidget {
  Q_OBJECT

  QPixmap m_handleLeftPixmap, m_handleRightPixmap, m_handleLeftGrayPixmap,
      m_handleRightGrayPixmap;
  Q_PROPERTY(QPixmap HandleLeftPixmap READ getHandleLeftPixmap WRITE
                 setHandleLeftPixmap);
  Q_PROPERTY(QPixmap HandleRightPixmap READ getHandleRightPixmap WRITE
                 setHandleRightPixmap);
  Q_PROPERTY(QPixmap HandleLeftGrayPixmap READ getHandleLeftGrayPixmap WRITE
                 setHandleLeftGrayPixmap);
  Q_PROPERTY(QPixmap HandleRightGrayPixmap READ getHandleRightGrayPixmap WRITE
                 setHandleRightGrayPixmap);

  IntLineEdit *m_leftLineEdit;
  IntLineEdit *m_rightLineEdit;

  QColor m_lightLineColor; /*-- スライダ溝の明るい色（白） --*/
  QColor m_darkLineColor; /*-- スライダ溝の暗い色（128,128,128） --*/
  QColor m_middleLineColor;
  QColor m_lightLineEdgeColor;
  Q_PROPERTY(
      QColor LightLineColor READ getLightLineColor WRITE setLightLineColor);
  Q_PROPERTY(QColor DarkLineColor READ getDarkLineColor WRITE setDarkLineColor);
  Q_PROPERTY(
      QColor MiddleLineColor READ getMiddleLineColor WRITE setMiddleLineColor);
  Q_PROPERTY(QColor LightLineEdgeColor READ getLightLineEdgeColor WRITE
                 setLightLineEdgeColor);

  QLabel *m_leftLabel, *m_rightLabel;

  std::pair<int, int> m_values;
  int m_minValue, m_maxValue;
  int m_grabOffset, m_grabIndex;
  int m_leftMargin, m_rightMargin;

  bool m_isMaxRangeLimited;

  bool m_isLinear;

public:
  IntPairField(QWidget *parent = 0, bool isMaxRangeLimited = true);
  ~IntPairField() {}

  /*! Set current values to \b values. */
  void setValues(const std::pair<int, int> &values);
  /*!	Return a pair containing current values. */
  std::pair<int, int> getValues() const { return m_values; }

  /*! Set left label string to \b QString \b text. Recompute left margin adding
                  label width. */
  void setLeftText(const QString &text);
  /*! Set right label string to \b QString \b text. Recompute right margin
     adding
                  label width. */
  void setRightText(const QString &text);

  void setLabelsEnabled(bool enable);

  /*! Set range of \b IntPairField to \b minValue, \b maxValue. */
  void setRange(int minValue, int maxValue);
  /*! Set \b minValue and \b maxValue to IntPairField range. */
  void getRange(int &minValue, int &maxValue);

  QPixmap getHandleLeftPixmap() const { return m_handleLeftPixmap; }
  void setHandleLeftPixmap(const QPixmap &pixmap) {
    m_handleLeftPixmap = pixmap;
  }
  QPixmap getHandleRightPixmap() const { return m_handleRightPixmap; }
  void setHandleRightPixmap(const QPixmap &pixmap) {
    m_handleRightPixmap = pixmap;
  }
  QPixmap getHandleLeftGrayPixmap() const { return m_handleLeftGrayPixmap; }
  void setHandleLeftGrayPixmap(const QPixmap &pixmap) {
    m_handleLeftGrayPixmap = pixmap;
  }
  QPixmap getHandleRightGrayPixmap() const { return m_handleRightGrayPixmap; }
  void setHandleRightGrayPixmap(const QPixmap &pixmap) {
    m_handleRightGrayPixmap = pixmap;
  }

  void setLightLineColor(const QColor &color) { m_lightLineColor = color; }
  QColor getLightLineColor() const { return m_lightLineColor; }
  void setDarkLineColor(const QColor &color) { m_darkLineColor = color; }
  QColor getDarkLineColor() const { return m_darkLineColor; }
  void setMiddleLineColor(const QColor &color) { m_middleLineColor = color; }
  QColor getMiddleLineColor() const { return m_middleLineColor; }
  void setLightLineEdgeColor(const QColor &color) {
    m_lightLineEdgeColor = color;
  }
  QColor getLightLineEdgeColor() const { return m_lightLineEdgeColor; }

protected:
  /*! Return value corresponding \b x position. */
  int pos2value(int x) const;
  /*! Return x position corresponding \b value. */
  int value2pos(int v) const;

  /*! Set current value to \b value.
                  Set left or right value, or both, to value depending on
     current slider
                  grab edited and \b value. */
  void setValue(int v);

  void paintEvent(QPaintEvent *) override;

  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;

  void setLinearSlider(bool linear) { m_isLinear = linear; }
protected slots:
  /*! Set current left value to value in left text field; if necessary, if left
                  value is greater than right, change also current right value.
  \n	This protected slot is called when text editing is finished.
  \n	Emit valuesChanged().
  \n	If current left value is equal to left text field value, return and do
  nothing. */
  void onLeftEditingFinished();

  /*! Set current right value to value in right text field; if necessary, if
  right
                  value is lower than left, change also current left value.
  \n	This protected slot is called when text editing is finished.
  \n	Emit valuesChanged().
  \n	If current right value is equal to right text field value return and
  do nothing. */
  void onRightEditingFinished();

signals:
  /*!	This signal is emitted when change one of two IntField value;
                  if one slider grab position change or if one text field
     editing is finished. */
  void valuesChanged(bool isDragging);
};

//-----------------------------------------------------------------------------
}  // namespace DVGui
//-----------------------------------------------------------------------------

#endif  // INTPAIRFIELD_H
