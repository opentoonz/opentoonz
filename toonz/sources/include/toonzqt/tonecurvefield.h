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
class IntPairField;
class CheckBox;

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

public:
  ChennelCurveEditor(QWidget *parent = 0, HistogramView *histogramView = 0);

  void setPoints(QList<TPointD> points);
  QList<TPointD> getPoints();

  int getCurrentControlPointIndex() { return m_currentControlPointIndex; };
  void setCurrentControlPointIndex(int index) {
    m_currentControlPointIndex = index;
  };

  bool eventFilter(QObject *object, QEvent *event) override;

  void setFirstLastXPosition(std::pair<int, int> values, bool isDragging);

  void setLinear(bool isLinear);

protected:
  QPointF strokeToViewPoint(const TPointD p);
  TPointD viewToStrokePoint(const QPointF &p);
  int getClosestPointIndex(const QPointF &pos, double &minDistance2) const;

  bool isCentralControlPoint(int index) { return index % 3 == 0; }
  bool isLeftControlPoint(int index) { return index % 3 == 2; }
  bool isRightControlPoint(int index) { return index % 3 == 1; }

  void movePoint(int index, const QPointF delta);
  void setPoint(int index, const QPointF point);
  QPointF checkPoint(const QPointF p);

  void moveCurrentControlPoint(const QPointF delta);
  void moveCentralControlPoint(int index, const QPointF delta);
  //	bool eraseControlPointWhileMove(int index, const QPointF delta);

  void addControlPoint(double percent);

  void removeControlPoint(int index);
  void removeCurrentControlPoint();

  void selectNextControlPoint();
  void selectPreviousControlPoint();
  void moveCurrentControlPointUp();
  void moveCurrentControlPointDown();

  QPainterPath getPainterPath();

  void paintEvent(QPaintEvent *) override;
  void mouseMoveEvent(QMouseEvent *) override;
  void mousePressEvent(QMouseEvent *) override;
  void mouseReleaseEvent(QMouseEvent *) override;
  void keyPressEvent(QKeyEvent *e) override;
  void enterEvent(QEvent *) override;
  void leaveEvent(QEvent *) override;

  void focusInEvent(QFocusEvent *fe) override;
  void focusOutEvent(QFocusEvent *fe) override;

signals:
  void focusOut();
  void controlPointChanged(bool isDragging);
  void controlPointAdded(int index);
  void controlPointRemoved(int index);

  void firstLastXPostionChanged(int, int);
};

//=============================================================================
// ToneCurveField
//-----------------------------------------------------------------------------

class DVAPI ToneCurveField final : public QWidget {
  Q_OBJECT

  QStackedWidget *m_toneCurveStackedWidget;
  QStackedWidget *m_sliderStackedWidget;
  QComboBox *m_channelListChooser;
  CheckBox *m_isLinearCheckBox;

public:
  ToneCurveField(QWidget *parent = 0, FxHistogramRender *fxHistogramRender = 0);

  void setCurrentChannel(int currentChannel);
  ChennelCurveEditor *getChannelEditor(int channel) const;
  ChennelCurveEditor *getCurrentChannelEditor() const;
  IntPairField *getCurrentSlider() const;

  int getChannelCount() { return m_toneCurveStackedWidget->count(); }

  void setIsLinearCheckBox(bool isChecked);

protected slots:
  void sliderValueChanged(bool);
  void onFirstLastXPostionChanged(int, int);

public slots:
  void setLinear(bool);

  void setLinearManually(bool);

signals:
  void currentChannelIndexChanged(int);
  void isLinearChanged(bool);
};
}

#endif  // TONECURVEFIELD_H
