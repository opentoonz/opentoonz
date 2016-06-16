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

/*!La classe ControlPointEditorStroke effettua tutte le operazioni matematiche
 * sullo Stroke */
class ControlPointEditorStroke {
private:
  //! Punti di controllo comprensivi di SpeedIn e SpeenOut
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

  /*! Viene riempito il vettore \b m_controlPoints scegliendo da \b m_stroke
  soltanto
  i punti di controllo nelle posizioni pari.
  */
  void updateControlPoints();

  //! Viene settato il valore \b p.m_isCusp osservando lo SpeedIn e SpeedOut del
  //! punto.
  void setCusp(ControlPoint &p);

  void setSpeedIn(ControlPoint &cp, const TPointD &p) {
    cp.m_speedIn = m_stroke->getControlPoint(cp.m_indexPoint) - p;
  }
  void setSpeedOut(ControlPoint &cp, const TPointD &p) {
    cp.m_speedOut = p - m_stroke->getControlPoint(cp.m_indexPoint);
  }

  /*! Inserisce un punto nel chunk di lunghezza maggiore selezionato tra quelli
     compresi
          tra il chunk \b indexA e \b indexB.
  */
  void insertPoint(int indexA, int indexB);

  /*! Verifica che in \b m_stroke tra due cuspidi sia sempre presente un numero
  pari
  di Chunk: in caso contrario richiama la \b insertPoint;
  */
  void adjustChunkParity();

  //! Sposta il punto di controllo \b index di un fattore delta
  void moveSingleControlPoint(int indexPoint, const TPointD &delta);

  /*! Sposta i punti di controllo precedenti al punto \b index di un fattore
delta.
Se \b moveSpeed==true e' usato per movimento degli speed, altrimenti per
            movimento dei punti di controllo.
  */
  void movePrecControlPoints(int indexPoint, const TPointD &delta,
                             bool moveSpeed);

  /*! Sposta i punti di controllo successivi al punto \b index di un fattore
delta.
Se \b moveSpeed==true e' usato per movimento degli speed, altrimenti per
            movimento dei punti di controllo.
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

  //! Ritorna \b true se il punto index e' una cuspide.
  bool getIsCusp(int index) const {
    assert(m_stroke && 0 <= index && index < (int)getControlPointCount());
    return m_controlPoints[index].m_isCusp;
  }

  //! Viene settato il valore \b m_isCusp del punto index-esimo a \b isCusp.
  void linkUnlinkSpeeds(int index, bool isCusp) {
    m_controlPoints[index].m_isCusp = isCusp;
  }

  /*! Sposta il ControlPoint \b index di un fattore delta con continuita',
            cioe' spostando anche i punti di controllo adiacenti se
     necessario.*/
  void moveControlPoint(int index, const TPointD &delta);

  //! Cancella il punto di controllo \b point.
  void deleteControlPoint(int index);

  /*! Aggiunge il punto di controllo \b point.
                  Ritorna l'indice del punto di controllo appena inserito.*/
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

  /*! Sposta il bilancino del punto \b index di un fattore delta. \b isIn deve
     essere
                  true se si vuole spostare lo SpeedIn.*/
  void moveSpeed(int index, const TPointD &delta, bool isIn, double pixelSize);

  /*! Se isLinear e' true setta a "0" il valore dello speedIn e il valore dello
     speedOut;
                  altrimenti li setta ad un valore di default. Ritorna vero se
     almeno un punto e' ststo modificato.*/
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
