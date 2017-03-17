#pragma once

#ifndef CONTROLPOINT_SELECTION_INCLUDED
#define CONTROLPOINT_SELECTION_INCLUDED

#include "toonzqt/selection.h"
#include "tool.h"
#include "tstroke.h"
#include "tcurves.h"

//=============================================================================
// ControlPointEditorStroke
//-----------------------------------------------------------------------------

/*!ControlPointEditorStroke class performs all mathematical operations
 * on Stroke */
class ControlPointEditorStroke {
private:
  //! Control Points include speed in and speed out
  class ControlPoint {
  public:
    int m_indexPoint;
    TPointD m_speedIn;
    TPointD m_speedOut;
    bool m_isCusp;

    ControlPoint(int i, TPointD speedIn, TPointD speedOut, bool isCusp = true)
        : m_indexPoint(i)
        , m_speedIn(speedIn)
        , m_speedOut(speedOut)
        , m_isCusp(isCusp) {}
    ControlPoint() {}
  };

  vector<ControlPoint> m_controlPoints;
  TStroke *m_stroke;
  int m_strokeIndex;

  /*! Fills the vector \b m_controlPoints choosing from \b m_stroke
  only the control points in even positions.
  */
  void updateControlPoints();

  //! Sets the value \b p.m_isCusp keeping SpeedIn and SpeedOut
  //! of the point.
  void setCusp(ControlPoint &p);

  void setSpeedIn(ControlPoint &cp, const TPointD &p) {
    cp.m_speedIn = m_stroke->getControlPoint(cp.m_indexPoint) - p;
  }
  void setSpeedOut(ControlPoint &cp, const TPointD &p) {
    cp.m_speedOut = p - m_stroke->getControlPoint(cp.m_indexPoint);
  }

  /*!Inserts the point in chunk of the greater length among those
     included between the chunk \b indexA and \b indexB.
  */
  void insertPoint(int indexA, int indexB);

  /*! Check that \b m_stroke between two cusps is always an even number
  of Chunk: otherwise it calls \b insertPoint;
  */
  void adjustChunkParity();

  //! Move the control point number \b index by factor delta
  void moveSingleControlPoint(int indexPoint, const TPointD &delta);

  /*! Move the control prior to the point \b index by a factor delta.
If \b moveSpeed==true adjusts move speed, otherwise moves the control points.
  */
  void movePrecControlPoints(int indexPoint, const TPointD &delta,
                             bool moveSpeed);

  /*! Move the control following the point \b index by a factor delta.
If \b moveSpeed==true adjusts move speed, otherwise moves the control points.
  */
  void moveNextControlPoints(int indexPoint, const TPointD &delta,
                             bool moveSpeed);

public:
  ControlPointEditorStroke() : m_stroke(0), m_strokeIndex(-1) {}

  /*!Viene modificato lo stroke in modo tale che tra due cuspidi esista sempre
  un
  numero pari di chunk.
  ATTENZIONE: poiche' puo' aggiungere punti di controllo allo stroke, e' bene
  richiamare
  tale funzione solo quando e' necessario.
  */
  void setStroke(TStroke *stroke, int strokeIndex);

  void setStrokeIndex(int strokeIndex) { m_strokeIndex = strokeIndex; }
  int getStrokeIndex() { return m_strokeIndex; }

  TThickPoint getControlPoint(int index) const {
    assert(m_stroke && 0 <= index && index < (int)m_controlPoints.size());
    return m_stroke->getControlPoint(m_controlPoints[index].m_indexPoint);
  }
  TThickPoint getSpeedIn(int index) const {
    assert(m_stroke && 0 <= index && index < (int)m_controlPoints.size());
    return m_controlPoints[index].m_speedIn;
  }
  TThickPoint getSpeedOut(int index) const {
    assert(m_stroke && 0 <= index && index < (int)m_controlPoints.size());
    return m_controlPoints[index].m_speedOut;
  }

  int getControlPointCount() const { return m_controlPoints.size(); }

  //! Returns \b true if the point index is a cusp.
  bool getIsCusp(int index) const {
    assert(m_stroke && 0 <= index && index < (int)getControlPointCount());
    return m_controlPoints[index].m_isCusp;
  }

  //! Sets the value of \b m_isCusp of the index-th point to \b isCusp.
  void linkUnlinkSpeeds(int index, bool isCusp) {
    m_controlPoints[index].m_isCusp = isCusp;
  }

  /*! Moves the ControlPoint \b index by a factor of delta with continuity,
  that is moving adjacent control points if necessary.*/
  void moveControlPoint(int index, const TPointD &delta);

  //! Remove the control point \b index.
  void deleteControlPoint(int index);

  /*! Add the control point \b pos.
	  Returns the index of newly added control point.*/
  int addControlPoint(const TPointD &pos);

  /*! Ritorna l'indice del cp piu' vicino al punto pos.
            distance2 riceve il valore del quadrato della distanza
            se la ControlPointEditorStroke e' vuota ritorna -1.*/
  int getClosestControlPoint(const TPointD &pos, double &distance2) const;

  //! Ritorna true sse e' definita una curva e se pos e' abbastanza vicino alla
  //! curva
  bool isCloseTo(const TPointD &pos, double pixelSize) const;

  /*! Ritorna l'indice del cp il cui bilancino e' piu' vicino al punto pos.
                  \b minDistance2 riceve il valore del quadrato della distanza
            se la ControlPointEditorStroke e' vuota ritorna -1. \b isIn e' true
     se il bilancino cliccato
            corrisponde allo SpeedIn.*/
  int getClosestSpeed(const TPointD &pos, double &minDistance2,
                      bool &isIn) const;

  /*! Move the balance of the point \b index by a factor delta. \b isIn must
     be true if you want to move SpeedIn. */
  void moveSpeed(int index, const TPointD &delta, bool isIn, double pixelSize);

  /*! If isLinear is true, sets to "0" the value of speedIn and the value
     speedOut; otherwise sets it to default value. Returns true if at least
		 one point is modified.*/
  bool setLinear(int index, bool isLinear);

  void setLinearSpeedIn(int index);
  void setLinearSpeedOut(int index);

  bool isSpeedInLinear(int index);
  bool isSpeedOutLinear(int index);

  bool isSelfLoop() { return m_stroke->isSelfLoop(); }
};

//=============================================================================
// ControlPointSelection
//-----------------------------------------------------------------------------

class ControlPointSelection : public QObject, public TSelection {
  Q_OBJECT

private:
  std::set<int> m_selectedPoints;
  ControlPointEditorStroke *m_controlPointEditorStroke;

public:
  ControlPointSelection() : m_controlPointEditorStroke(0) {}

  void setControlPointEditorStroke(
      ControlPointEditorStroke *controlPointEditorStroke) {
    m_controlPointEditorStroke = controlPointEditorStroke;
  }

  bool isEmpty() const { return m_selectedPoints.empty(); }

  void selectNone() { m_selectedPoints.clear(); }
  bool isSelected(int index) const;
  void select(int index);
  void unselect(int index);

  void deleteControlPoints();

  void addMenuItems(QMenu *menu);

  void enableCommands();

protected slots:
  void setLinear();
  void setUnlinear();
};

#endif  // CONTROLPOINT_SELECTION_INCLUDED
