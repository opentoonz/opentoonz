#pragma once

#ifndef MARKSBAR_H
#define MARKSBAR_H

// Toonz includes
#include "tcommon.h"

// Qt includes
#include <QFrame>

#undef DVAPI
#undef DVVAR
#ifdef TOONZQT_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//=============================================================================
// MarksBar
//-----------------------------------------------------------------------------

/*!
  A MarksBar is a generalized slider with multiple values.
*/
class DVAPI MarksBar final : public QFrame {
  Q_OBJECT

protected:
  int m_min, m_max, m_sortDist;

  QVector<int> m_values;
  QVector<QColor> m_colors;
  int m_selectedMark;

  bool m_markUp;

public:
  MarksBar(QWidget *parent = 0, bool markUp = true);

  void setRange(int min, int max, int sortDistance = -1);
  int minimum() const { return m_min; }
  int maximum() const { return m_max; }
  int sortDistance() const { return m_sortDist; }

  const QVector<int> &values() const { return m_values; }
  QVector<int> &values() { return m_values; }

  const QVector<QColor> &colors() const { return m_colors; }
  QVector<QColor> &colors() { return m_colors; }

  void setMarkSide(bool up);
  bool markSide() const { return m_markUp; }

  void conformValues(bool preferRollDown = true);

protected:
  virtual void drawMark(QPainter &p, int pos, const QColor &color);

  int valToPos(int val);
  int posToVal(int pos);

  void paintEvent(QPaintEvent *pe) override;
  void mousePressEvent(QMouseEvent *me) override;
  void mouseMoveEvent(QMouseEvent *me) override;
  void mouseReleaseEvent(QMouseEvent *me) override;

signals:

  void marksUpdated();
  void marksReleased();
};

#endif  // MARKSBAR_H
