#pragma once

#ifndef TONECURVEFIELD_H
#define TONECURVEFIELD_H

#include "tcommon.h"
#include "tstroke.h"
#include "toonzqt/histogram.h"
#include <QWidget>

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
class TStroke;
class QPainterPath;
class QStackedWidget;
class FxHistogramRender;

//=============================================================================
namespace DVGui {

// forward declaration
class DoublePairField;
class CheckBox;
class DoubleLineEdit;

//=============================================================================
// ChennelCurveEditor
//-----------------------------------------------------------------------------

class DVAPI ChennelCurveEditor final : public QWidget {
  Q_OBJECT

  HistogramView *m_histogramView;
  ChannelBar *m_verticalChannelBar;

  QList<QPointF> m_points;
  int m_currentControlPointIndex;

  Qt::MouseButton m_mouseButton;

  int m_curveHeight;

  int m_LeftRightMargin;
  int m_TopMargin;
  int m_BottomMargin;

  bool m_isLinear;

  QPointF m_preMousePos;

  bool m_isEnlarged;

public:
  ChennelCurveEditor(QWidget *parent = 0, HistogramView *histogramView = 0);

  void setPoints(QList<TPointD> points);
  QList<TPointD> getPoints();

  int getCurrentControlPointIndex() { return m_currentControlPointIndex; };
  void setCurrentControlPointIndex(int index) {
    m_currentControlPointIndex = index;
  };

  bool eventFilter(QObject *object, QEvent *event) override;

  void setFirstLastXPosition(std::pair<double, double> values, bool isDragging);

  void setLinear(bool isLinear);
  void moveCurrentControlPoint(QPointF delta);

  void setEnlarged(bool isEnlarged);
  void setLabelRange(ChannelBar::Range range);

protected:
  QPointF viewToStrokePoint(const QPointF &p);
  int getClosestPointIndex(const QPointF &pos, double &minDistance2) const;

  bool isCentralControlPoint(const int index) const { return index % 3 == 0; }
  bool isLeftControlPoint(const int index) const { return index % 3 == 2; }
  bool isRightControlPoint(const int index) const { return index % 3 == 1; }

  void setPoint(int index, const QPointF point);
  void movePoint(int index, const QPointF delta);
  QPointF checkPoint(const QPointF p);

  QPointF getVisibleHandlePos(int index) const;

  void moveCentralControlPoint(int index, QPointF delta);
  //	bool eraseControlPointWhileMove(int index, const QPointF delta);

  void addControlPoint(double percent);

  void removeControlPoint(int index);
  void removeCurrentControlPoint();

  void selectNextControlPoint();
  void selectPreviousControlPoint();

  QPainterPath getPainterPath();

  void paintEvent(QPaintEvent *) override;
  void mouseMoveEvent(QMouseEvent *) override;
  void mousePressEvent(QMouseEvent *) override;
  void mouseReleaseEvent(QMouseEvent *) override;
  void keyPressEvent(QKeyEvent *e) override;

  void focusInEvent(QFocusEvent *fe) override;
  void focusOutEvent(QFocusEvent *fe) override;

signals:
  void focusOut();
  void controlPointChanged(bool isDragging);
  void controlPointAdded(int index);
  void controlPointRemoved(int index);

  void firstLastXPostionChanged(double, double);
  void updateCurrentPosition(int, QPointF);
};

//=============================================================================
// ToneCurveField
//-----------------------------------------------------------------------------

class DVAPI ToneCurveField final : public QWidget {
  Q_OBJECT

  QStackedWidget *m_toneCurveStackedWidget;
  QStackedWidget *m_sliderStackedWidget;
  QComboBox *m_channelListChooser;
  CheckBox *m_isLinearCheckBox, *m_isEnlargedCheckBox;

  DoubleLineEdit *m_currentInput, *m_currentOutput;
  int m_currentPointIndex;
  QComboBox *m_rangeMode;

public:
  ToneCurveField(QWidget *parent = 0, FxHistogramRender *fxHistogramRender = 0);

  void setCurrentChannel(int currentChannel);
  ChennelCurveEditor *getChannelEditor(int channel) const;
  ChennelCurveEditor *getCurrentChannelEditor() const;
  DoublePairField *getCurrentSlider() const;

  int getChannelCount() { return m_toneCurveStackedWidget->count(); }

  void setIsLinearCheckBox(bool isChecked);
  bool isEnlarged();

protected slots:
  void sliderValueChanged(bool);
  void onFirstLastXPostionChanged(double, double);
  void onUpdateCurrentPosition(int, QPointF);
  void onCurrentPointEditted();
  void onCurrentChannelSwitched(int);
  void onRangeModeSwitched(int);

public slots:
  void setLinear(bool);
  void setEnlarged(bool);
  void setLinearManually(bool);

signals:
  void currentChannelIndexChanged(int);
  void isLinearChanged(bool);
  void sizeChanged();
};
}  // namespace DVGui

#endif  // TONECURVEFIELD_H
