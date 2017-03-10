#pragma once

//-----------------------------------------------------------------------------
//  tstrokeoutline.h:
//-----------------------------------------------------------------------------
#ifndef T_STROKE_OUTLINE_H
#define T_STROKE_OUTLINE_H

#include "tgeometry.h"

#undef DVAPI
#undef DVVAR
#ifdef TVECTORIMAGE_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

#ifdef _MSC_VER
#pragma warning(disable : 4251)
#endif

// forward declaration
class TStroke;
class TRegion;
class TRegionOutline;

//-----------------------------------------------------------------------------
/*!
  FIXME
 */
struct TOutlinePoint {
  /*!
VEDERE COMMENTI DI TStrokeOutline
*/
  double x, y;  //  coordinate geometriche del punto sull'outline per il valore
                //  't' del parametro
  double u, v;  //  coordinate di texture del punto (v = "ascissa curvilinea", u
                //  = "parametro di ampiezza")
  // TPointD tangent;  //  vettore tangente alla centerline (nel punto che ha lo
  // stesso parametro 't' (approssimazione...))
  // TPointD normal; //  vettore normale alla centerline (nel punto che ha lo
  // stesso parametro 't' (approssimazione...))
  int stepCount;  //  contatore incrementale per il campionamento di outline
                  //  procedurali

  TOutlinePoint(double x_, double y_,
                // TPointD tangent_ = TPointD(),
                // TPointD normal_ = TPointD(),
                int stepCount_,  // = 0,
                double u_,       // = 0,
                double v_)       // = 0)
      : x(x_),
        y(y_),
        u(u_),
        v(v_),
        // tangent(tangent_),
        // normal(normal_),
        stepCount(stepCount_) {}

  TOutlinePoint(const TPointD &p,
                // const TPointD &tangent_ = TPointD(),
                // const TPointD &normal = TPointD(),
                int stepCount_,    // = 0,
                const TPointD &t)  // = TPointD())
      : x(p.x),
        y(p.y),
        u(t.x),
        v(t.y),
        // tangent(tangent_),
        // normal(normal),
        stepCount(stepCount_) {}

  TOutlinePoint(const TPointD &p, int stepCount_)
      : x(p.x)
      , y(p.y)
      , u(0)
      , v(0)
      ,
      // tangent(tangent_),
      // normal(normal),
      stepCount(stepCount_) {}

  TOutlinePoint(const TPointD &p) : x(p.x), y(p.y), u(0), v(0), stepCount(0) {}
};

//-----------------------------------------------------------------------------

inline TPointD convert(const TOutlinePoint &p) { return TPointD(p.x, p.y); }

//-----------------------------------------------------------------------------

/*!
  FIXME
 */
class DVAPI TStrokeOutline {
  /*!
gli elementi (TOutlinePoint) di m_outline rappresentano punti situati
alternativamente
sulle facce pari/dispari dell'outline (pari a destra/dispari a sinistra nel
verso di
disegno della centerline). Il calcolo delle coordinate geometriche TPointD(x, y)
di tali punti e' effettuato utilizzando la rappresentazione parametrica (di
parametro 't'
in [0, 1]) della centerline e delle facce pari/dispari dell'outline: i punti
p(2k) = (TPointD)m_outline[2k], p(2k + 1) = (TPointD)m_outline[2k + 1] (che si
ottengono
per lo stesso valore di 't' da 2 DIVERSE equazioni per le 2 facce pari/dispari
dell'outline)
sono totalmente scorrelati. In particolare 0.5*(p(2k) + p(2k + 1)) non coincide
con il punto
della centerline che si ottiene per lo stesso valore di 't' (stessa cosa dicasi
per
tangenti/normali sulla centerline rispetto alla media di quelle sull'outline):
l'ottima
approssimazione sperimentale induce pero' a considerarli praticamente uguali
nelle stroke
procedurali (stessa cosa dicasi per tangenti/normali). Si ottengono cosi' le
seguenti:
m_outline[2k].tangent == m_outline[2k + 1].tangent
m_outline[2k].normal == m_outline[2k + 1].normal
m_outline[2k].stepCount == m_outline[2k + 1].stepCount



I membri di ogni elemento (TOutlinePoint) di m_outline hanno la seguente
interpretazione:
TPointD(x, y) rappresenta la posizione geometrica del punto sull'outline;
'u' ("parametro di ampiezza") e 'v' ("ascissa curvilinea") sono significativi
solo per
stroke di tipo TTextureStyle e rappresentano i parametri di texture;
tangent/normal (significativi solo per stroke procedurali) rappresentano i
vettori
tangente/normale alla centerline nel punto (sulla centerline) che ha lo stesso
't' di
quello considerato di m_outline (approssimazione...);
stepCount (significativo solo per stroke procedurali) indicizza in modo
crescente le coppie
di punti m_outline[2k], m_outline[2k + 1] richieste (IN ECCESSO) dal passo
procedurale:
in pratica, nel caso di stroke procedurale, m_outline contiene tutti i punti
richiesti dal
passo (m_lengthStep: contato sulla centerline) di generazione della stroke
procedurale
(per questi TOutlinePoint, stepCount e' crescente sulle coppie:
  1)  m_outline[0].stepCount == m_outline[1].stepCount == 1;
  2)  m_outline[2k].stepCount == m_outline[2k + 1].stepCount;
  3)  se m_outline[2k + 1] e' un punto richiesto dal passo procedurale
(m_lengthStep) e
      m_outline[2k + 2h] e' un il punto successivo richiesto dal passo
(m_lengthStep),
      allora
    3.1)  m_outline[2k + s].stepCount = 0  (s = 2, ... , 2h - 1);
    3.2)  m_outline[2k + 2h].stepCount = m_outline[2k + 1].stepCount + 1;)
    3.3)  m_outline[2k + 2h].stepCount = m_outline[2k + 2h + 1].stepCount;
 )
 ed inoltre tutti quelli che servono per "addolcire" la linearizzazione a tratti
 dell'outline (per questi TOutlinePoint, stepCount e' 0 (facilita i
controlli...))
*/
private:
  std::vector<TOutlinePoint> m_outline;

public:
  TStrokeOutline() {}
  ~TStrokeOutline() {}

  std::vector<TOutlinePoint> &getArray() { return m_outline; }

  TStrokeOutline(const TStrokeOutline &);
  TStrokeOutline &operator=(const TStrokeOutline &);

  void addOutlinePoint(const TOutlinePoint &);
};

//-----------------------------------------------------------------------------

DVAPI TStrokeOutline getOutline(const TStroke &stroke);

//-----------------------------------------------------------------------------

class TQuadratic;

DVAPI std::vector<TQuadratic> getOutlineWithQuadratic(const TStroke &stroke);

DVAPI void computeOutlines(const TStroke *stroke, int startQuad, int endQuad,
                           std::vector<TQuadratic *> &quadArray, double error2);

//---------------------------------------------------------------------------------------

namespace TOutlineUtil {

class DVAPI OutlineParameter {
public:
  double m_lengthStep;  //  max lengthStep (sulla centerline) per la
                        //  linearizzazione dell'outline
  OutlineParameter(double lengthStep = 0) : m_lengthStep(lengthStep) {}
};

// per adesso implementata in tellipticbrush.cpp (per motivi storici)
DVAPI void makeOutline(const TStroke &stroke, TStrokeOutline &outline,
                       const OutlineParameter &param);
DVAPI void makeOutline(const TStroke &path, const TStroke &brush,
                       const TRectD &brushBox, TStrokeOutline &outline,
                       const OutlineParameter &param);

DVAPI void makeOutline(const TStroke &path, const TRegion &brush,
                       const TRectD &brushBox, TRegionOutline &outline);

DVAPI TRectD computeBBox(const TStroke &stroke);
}

//---------------------------------------------------------------------------------------

#endif  // T_STROKE_OUTLINE_H

//-----------------------------------------------------------------------------
// End Of File
//-----------------------------------------------------------------------------
