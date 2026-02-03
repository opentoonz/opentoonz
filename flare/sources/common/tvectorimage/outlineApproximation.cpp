

// outlineApproximation.cpp: implementation of the outlineApproximation class.
//
//////////////////////////////////////////////////////////////////////

#include "tstrokeoutline.h"
#include "tstroke.h"
#include "tcurves.h"
#include "tmathutil.h"
#include "tgl.h"

//#include "tcolorfunctions.h"

typedef std::pair<TQuadratic *, TQuadratic *> outlineEdge;
typedef std::vector<outlineEdge> outlineBoundary;

const double infDouble = (std::numeric_limits<double>::max)();

/*
ONLY FOT TEST

TSegment  g_tangEnvelope_1;
TSegment  g_tangEnvelope_2;

vector<TQuadratic>  g_testOutline;
*/
namespace Outline {

class infinityCurvature {};

class notValidOutline {};
}

namespace {

/*
  This formula is derived from Graphic Gems pag. 600

    e = h^2 |a|/8

      e = pixel size
      h = step
      a = acceleration of curve (for a quadratic is a constant value)
  */
double localComputeStep(const TQuadratic &quad, double pixelSize) {
  double step = 2;

  TPointD A = quad.getP0() - 2.0 * quad.getP1() +
              quad.getP2();  // 2*A is the acceleration of the curve

  double A_len        = norm(A);
  if (A_len > 0) step = sqrt(2 * pixelSize / A_len);

  return step;
}
//---------------------------------------------------------------------------

// selezionano lo spicchio da calcolare nella costruzione dei tappi
// (semicirconferenze iniziali e finali)
const int QUARTER_BEGIN = 1;
const int QUARTER_END   = 0;

// selezionano il pezzo d'outline da calcolare (sopra/sotto)
const int OUTLINE_UP   = 1;
const int OUTLINE_DOWN = 0;

// utili
const double ratio_1_3 = 1.0 / 3.0;
const double ratio_2_3 = 2.0 / 3.0;

//---------------------------------------------------------------------------

// torna la curvature per t=0
template <class T>
double curvature_t0(const T *curve) {
  assert(curve);
  TPointD v1 = curve->getP1() - curve->getP0();
  TPointD v2 = curve->getP2() - curve->getP1();

  double v_cross = cross(v1, v2);

  if (isAlmostZero(v_cross)) return infDouble;

  return ratio_2_3 * v_cross / pow(norm(v1), ratio_1_3);
}

//---------------------------------------------------------------------------

// torna la curvature per t=1
double curvature_t1(const TThickQuadratic *curve) {
  assert(curve);
  TThickQuadratic tmp;

  tmp.setThickP0(curve->getThickP2());
  tmp.setThickP1(curve->getThickP1());
  tmp.setThickP2(curve->getThickP0());

  return curvature_t0(&tmp);
}

//---------------------------------------------------------------------------

// estrae il punto dell'outline per il parametro specificato
// N.B: e' sbagliata non tiene conto degli inviluppi
TPointD getPointInOutline(const TThickQuadratic *tq, double t, int upOrDown) {
  assert(tq);
  const TThickPoint &p = tq->getThickPoint(t);
  TPointD n            = tq->getSpeed(t);
  if (norm2(n)) {
    n = normalize(n);
    n = upOrDown ? rotate90(n) : rotate270(n);
  }

  return convert(p) + p.thick * n;
}

//---------------------------------------------------------------------------

bool checkPointInOutline(const TPointD &pointToTest, const TThickQuadratic *tq,
                         double t, double error) {
  assert(tq);
  TThickPoint tpnt = tq->getThickPoint(t);

  if (fabs(sq(pointToTest.x - tpnt.x) + sq(pointToTest.y - tpnt.y) -
           sq(tpnt.thick)) < error)
    return true;

  return false;
}

//---------------------------------------------------------------------------

// costruisce un ramo di outline (sopra o sotto) per una quadratica cicciona
TQuadratic *makeOutlineForThickQuadratic(const TThickQuadratic *tq,
                                         int upOrDown) {
  assert(tq);
  // if(!outline) return 0;

  TThickPoint p0 = tq->getThickP0(),
              // p1 = tq->getThickP0(),
      p2 = tq->getThickP2();

  TPointD t0 = tq->getP1() - tq->getP0();
  TPointD t1 = tq->getP2() - tq->getP1();

  if (t0 == t1) return 0;

  TPointD N0 = tq->getSpeed(0.0), N2 = tq->getSpeed(1.0);

  if (!norm2(N0) && !norm2(N2)) throw Outline::notValidOutline();

  if (norm2(N0)) {
    N0 = normalize(N0);
    N0 = upOrDown ? rotate90(N0) : rotate270(N0);
  }

  if (norm2(N2)) {
    N2 = normalize(N2);
    N2 = upOrDown ? rotate90(N2) : rotate270(N2);
  }

  TPointD p0aux = (convert(p0) + p0.thick * N0);
  TPointD p2aux = (convert(p2) + p2.thick * N2);

  TQuadratic radius(TPointD(tq->getThickP0().thick, 0.0),
                    TPointD(tq->getThickP1().thick, 0.0),
                    TPointD(tq->getThickP2().thick, 0.0));

  TPointD r0 = radius.getSpeed(0.0);
  TPointD r1 = radius.getSpeed(1.0);

  TPointD v0, v2;

  double ct0 = curvature_t0(tq);

  if (ct0 != infDouble)
    v0 = (1 + p0.thick * ct0) * t0 + 0.5 * r0.x * N0;
  else
    v0 = r0.x * N0;

  double ct1 = curvature_t1(tq);

  if (ct1 != infDouble)
    v2 = (1 + p2.thick * ct1) * t1 + 0.5 * r1.x * N2;
  else
    v2 = r1.x * N2;

  /*
try {
v0 = (1 + p0.thick * curvature_t0( tq )) * t0 + 0.5 * r0.x * N0;
}
catch( Outline::infinityCurvature& ) {
}

try {
v2 = (1 + p2.thick * curvature_t1( tq )) * t1 + 0.5 * r1.x * N2;
}
catch( Outline::infinityCurvature& ) {
}
*/
  //    g_tangEnvelope_1.setP0( outline.getP0() );
  //    g_tangEnvelope_1.setP1( outline.getP0() + v0 );

  //    g_tangEnvelope_2.setP0( outline.getP2() );
  //    g_tangEnvelope_2.setP1( outline.getP2() + v2 );

  //  solve sistem  p1 = p0 + k * v1 = p2 + m * v2 to find

  double det = v0.x * v0.y - v2.x * v2.y;
  if (areAlmostEqual(det, 0.0)) return 0;
  double xsol;
  try {
    xsol = ((p0aux.x - p2aux.x) * v2.y - (p0aux.y - p2aux.y) * v2.x) / det;
    // tsolveSistem( A, 2, b );
  } catch (TMathException &) {
    return new TQuadratic((upOrDown) ? p0aux : p2aux, (p0aux + p2aux) * 0.5,
                          (upOrDown) ? p2aux : p0aux);
  } catch (std::exception &e) {
    std::string s(e.what());
    abort();
  } catch (...) {
    abort();
  }

  return new TQuadratic((upOrDown) ? p0aux : p2aux, p0aux + xsol * v0,
                        (upOrDown) ? p2aux : p0aux);
}

//---------------------------------------------------------------------------

/*
    costruisce l'outline per una singola quadratica senza
    inserire le semicirconferenze iniziali e finali
   */
void makeOutline(/*std::ofstream& cout,*/
                 outlineBoundary &outl, const TThickQuadratic &t,
                 double error) {
  outlineEdge edge;
  const TThickQuadratic *tq = &t;
  edge.first = edge.second = 0;
  try {
    edge.first  = makeOutlineForThickQuadratic(tq, OUTLINE_UP);
    edge.second = makeOutlineForThickQuadratic(tq, OUTLINE_DOWN);
  } catch (Outline::notValidOutline &) {
    delete edge.first;
    delete edge.second;
    return;
  }

  const TQuadratic *q_up     = edge.first;
  const TQuadratic *q_down   = edge.second;
  const double parameterTest = 0.5;

  // forza l'uscita per valori troppo piccoli
  bool isAlmostAPoint =
      areAlmostEqual(tq->getThickP0(), tq->getThickP1(), 1e-2) &&
      areAlmostEqual(tq->getThickP1(), tq->getThickP2(), 1e-2) /*&&
      areAlmostEqual( tq.getThickP0(), tq.getThickP2(), 1e-2 )*/;

  if (isAlmostAPoint ||
      (q_up && checkPointInOutline(q_up->getPoint(parameterTest), tq,
                                  parameterTest, error) &&
          q_down && checkPointInOutline(q_down->getPoint(parameterTest), tq,
                                        parameterTest, error))) {
    /*	if (edge.first)
      cout << "left: "<< *(edge.first);
else
      cout << "left: "<< 0;
    if (edge.second)
      cout << "right: "<<*(edge.second);
else
      cout << "right: "<< 0;

    cout<<std::endl;*/

    outl.push_back(edge);
    return;
  } else {
    delete edge.first;
    delete edge.second;
  }

  TThickQuadratic tq_left, tq_right;

  tq->split(0.5, tq_left, tq_right);

  makeOutline(/*out,*/ outl, tq_left, error);
  makeOutline(/*cout,*/ outl, tq_right, error);
}

//---------------------------------------------------------------------------

void splitCircularArcIntoQuadraticCurves(const TPointD &Center,
                                         const TPointD &Pstart,
                                         const TPointD &Pend,
                                         std::vector<TQuadratic *> &quadArray) {
  // It splits a circular anticlockwise arc into a sequence of quadratic bezier
  // curves
  // Every quadratic curve can approximate an arc no TLonger than 45 degrees (or
  // 60).
  // It supposes that Pstart and Pend are onto the circumference (so that their
  // lengths
  // are equal to the radius of the circumference), otherwise the resulting
  // curves could
  // be unpredictable.
  // The last component in quadCurve[] is an ending void curve

  /* ----------------------------------------------------------------------------------
   */
  // If you want to split the arc into arcs no TLonger than 45 degrees (so that
  // the whole
  // curve will be splitted into 8 pieces) you have to set these constants as
  // follows:
  // cos_ang     ==> cos_45   = 0.5 * sqrt(2);
  // sin_ang     ==> sin_45   = 0.5 * sqrt(2);
  // tan_semiang ==> tan_22p5 = 0.4142135623730950488016887242097;
  // N_QUAD                   = 8;

  // If you want to split the arc into arcs no TLonger than 60 degrees (so that
  // the whole
  // curve will be splitted into 6 pieces) you have to set these constants as
  // follows:
  // cos_ang     ==> cos_60 = 0.5;
  // sin_ang     ==> sin_60 = 0.5 * sqrt(3);
  // tan_semiang ==> tan_30 = 0.57735026918962576450914878050196;
  // N_QUAD                 = 6;
  /* ----------------------------------------------------------------------------------
   */

  // Defines some useful constant to split the arc into arcs no TLonger than
  // 'ang' degrees
  // (the whole circumference will be splitted into 360/ang quadratic curves).
  const double cos_ang     = 0.5 * sqrt(2.0);
  const double sin_ang     = 0.5 * sqrt(2.0);
  const double tan_semiang = 0.4142135623730950488016887242097;
  const int N_QUAD         = 8;  // it's 360/ang

  // First of all, it computes the vectors from the center to the circumference,
  // in Pstart and Pend, and their cross and dot products
  TPointD Rstart = Pstart - Center;  // its length is R (radius of the circle)
  TPointD Rend   = Pend - Center;    // its length is R (radius of the circle)
  double cross_prod       = cross(Rstart, Rend);  // it's Rstart x Rend
  double dot_prod         = Rstart * Rend;
  const double sqr_radius = Rstart * Rstart;
  TPointD aliasPstart     = Pstart;
  TQuadratic *quad;

  while ((cross_prod <= 0) ||
         (dot_prod <= cos_ang * sqr_radius))  // the circular arc is TLonger
                                              // than a 'ang' degrees arc
  {
    if ((int)quadArray.size() == N_QUAD)  // this is possible if Pstart or Pend
                                          // is not onto the circumference
      return;
    TPointD Rstart_rot_ang(cos_ang * Rstart.x - sin_ang * Rstart.y,
                           sin_ang * Rstart.x + cos_ang * Rstart.y);
    TPointD Rstart_rot_90(-Rstart.y, Rstart.x);
    quad =
        new TQuadratic(aliasPstart, aliasPstart + tan_semiang * Rstart_rot_90,
                       Center + Rstart_rot_ang);
    quadArray.push_back(quad);

    // quad->computeMinStepAtNormalSize ();

    // And moves anticlockwise the starting point on the circumference by 'ang'
    // degrees
    Rstart      = Rstart_rot_ang;
    aliasPstart = quad->getP2();
    cross_prod  = cross(Rstart, Rend);  // it's Rstart x Rend
    dot_prod    = Rstart * Rend;

    // after the rotation of 'ang' degrees, the remaining part of the arc could
    // be a 0 degree
    // arc, so it must stop and exit from the function
    if ((cross_prod <= 0) && (dot_prod > 0.95 * sqr_radius)) return;
  }

  if ((cross_prod > 0) && (dot_prod > 0))  // the last quadratic curve
                                           // approximates an arc shorter than a
                                           // 'ang' degrees arc
  {
    TPointD Rstart_rot_90(-Rstart.y, Rstart.x);

    double deg_index = (sqr_radius - dot_prod) / (sqr_radius + dot_prod);

    quad = new TQuadratic(aliasPstart,
                          (deg_index < 0)
                              ? 0.5 * (aliasPstart + Pend)
                              : aliasPstart + sqrt(deg_index) * Rstart_rot_90,
                          Pend);
    quadArray.push_back(quad);

  } else  // the last curve, already computed, is as TLong as a 'ang' degrees
          // arc
    quadArray.back()->setP2(Pend);
}

//---------------------------------------------------------------------------

// copia arrayUp e arrayDown nel vettore dell'outline
// se le dimensioni sono diverse il vettore con il numero
// minore di quadratiche viene riempito con quadratiche degeneri
// con i punti di controllo coincidenti nell'ultimo estremo valido
void copy(/*std::ofstream& cout,*/
          const std::vector<TQuadratic *> &arrayUp,
          const std::vector<TQuadratic *> &arrayDown, outlineBoundary &ob) {
  int minSize = std::min(arrayUp.size(), arrayDown.size());

  assert(minSize > 0);

  int i;

  for (i = 0; i < minSize; ++i) {
    // cout<<"left: "<< *(arrayUp[i])<< "right: "<<*(arrayDown[i])<<std::endl;
    // cout<"left: "<< arrayUp[i].getP0()<<", "arrayUp[i].getP1()<<",
    // "arrayUp[i].getP2()<< "right: "<< << arrayDown[i].getP0()<<",
    // "arrayDown[i].getP1()<<", "arrayDown[i].getP2()<<endl;
    ob.push_back(outlineEdge(arrayUp[i], arrayDown[i]));
  }
  if (arrayUp.size() != arrayDown.size()) {
    const std::vector<TQuadratic *> &vMaxSize =
        arrayUp.size() > arrayDown.size() ? arrayUp : arrayDown;
    const std::vector<TQuadratic *> &vMinSize =
        arrayUp.size() < arrayDown.size() ? arrayUp : arrayDown;

    int delta = vMaxSize.size() - vMinSize.size();

    if (arrayUp.size() > arrayDown.size())
      while (i < minSize + delta) {
        // cout<<"left: "<< arrayUp[i]<< "right: "<< 0<<std::endl;
        ob.push_back(outlineEdge(arrayUp[i], (TQuadratic *)0));
        i++;
      }
    else
      while (i < minSize + delta) {
        // cout<<"left: "<< 0 << "right: "<< arrayDown[i]<<std::endl;
        ob.push_back(outlineEdge((TQuadratic *)0, arrayDown[i]));
        i++;
      }
  }
}

//---------------------------------------------------------------------------

inline void changeQuadraticDirection(TQuadratic *q) {
  TPointD p = q->getP2();
  q->setP2(q->getP0());
  q->setP0(p);
}

//---------------------------------------------------------------------------

// cambia il verso del vettore di quadratiche (vedi changeDirection di
// tstroke.cpp)
void changeDirection(std::vector<TQuadratic *> &array, bool onlyQuads = false) {
  UINT chunkCount = array.size();
  UINT to         = tfloor(chunkCount * 0.5);
  UINT i;

  if (chunkCount & 1) changeQuadraticDirection(array[to]);

  --chunkCount;

  for (i = 0; i < to; ++i) {
    changeQuadraticDirection(array[i]);
    changeQuadraticDirection(array[chunkCount - i]);

    if (!onlyQuads) std::swap(array[i], array[chunkCount - i]);
  }
}

//---------------------------------------------------------------------------

// estrae i punti necessari a costruire la semicirconferenza
// iniziale e finale di una curva cicciona
TQuadratic getCircleQuarter(const TThickQuadratic *tq, int versus) {
  TQuadratic out;

  TPointD v = versus ? -tq->getSpeed(0.0) : tq->getSpeed(1.0);

  if (norm2(v)) v = normalize(v);

  TPointD center = versus ? tq->getP0() : tq->getP2();
  double radius  = versus ? tq->getThickP0().thick : tq->getThickP2().thick;

  out.setP0(center + (versus ? rotate270(v) : rotate90(v)) * radius);
  out.setP1(center + v * radius);
  out.setP2(center + (versus ? rotate90(v) : rotate270(v)) * radius);

  return out;
}

//---------------------------------------------------------------------------

void drawQuadratic(const TQuadratic &quad, double pixelSize) {
  double m_min_step_at_normal_size = localComputeStep(quad, pixelSize);

  // It draws the curve as a linear piecewise approximation

  double invSqrtScale = 1.0;
  // First of all, it computes the control circles of the curve in screen
  // coordinates
  TPointD scP0 = quad.getP0();
  TPointD scP1 = quad.getP1();
  TPointD scP2 = quad.getP2();

  TPointD A = scP0 - 2 * scP1 + scP2;
  TPointD B = scP0 - scP1;

  double h;
  h         = invSqrtScale * m_min_step_at_normal_size;
  double h2 = h * h;

  TPointD P = scP0, D2 = 2 * h2 * A, D1 = A * h2 - 2 * B * h;

  if (h < 0 || isAlmostZero(h)) return;
  assert(h > 0);

  // It draws the whole curve, using forward differencing
  glBegin(GL_LINE_STRIP);  // The curve starts from scP0
  glVertex2d(scP0.x, scP0.y);

  for (double t = h; t < 1; t = t + h) {
    P  = P + D1;
    D1 = D1 + D2;
    glVertex2d(P.x, P.y);
  }

  glVertex2d(scP2.x, scP2.y);  // The curve ends in scP2
  glEnd();
}

//---------------------------------------------------------------------------

}  // end of unnamed namespace

//-----------------------------------------------------------------------------

static void makeOutline(const TStroke *stroke, int startQuad, int endQuad,
                        outlineBoundary &ob, double error2) {
  // std::ofstream cout("c:\\temp\\outline.txt");

  assert(stroke);
  assert(startQuad >= 0);
  assert(endQuad < stroke->getChunkCount());
  assert(startQuad <= endQuad);
  TThickQuadratic *tq;
  std::vector<TQuadratic *> arrayUp, arrayDown;
  TQuadratic arc;

  if (!stroke->getChunkCount()) return;
  // if (startQuad==0)
  {
    const TThickQuadratic *tq = stroke->getChunk(startQuad);

    // trova i punti sul cerchio che corrispondono
    // a due fette di 90 gradi.
    // Ritorna una quadratica invece di tre singoli punti solo per compattezza.
    TQuadratic arc = getCircleQuarter(tq, QUARTER_BEGIN);

    // estrae le quadratiche che corrispondono ad i due archi...
    splitCircularArcIntoQuadraticCurves(tq->getP0(), arc.getP0(), arc.getP1(),
                                        arrayUp);
    // e le ordina in modo che l'outline sia composta sempre da
    // una curva superiore ed una inferiore corrispondente
    changeDirection(arrayUp);
    splitCircularArcIntoQuadraticCurves(tq->getP0(), arc.getP1(), arc.getP2(),
                                        arrayDown);
    changeDirection(arrayDown, true);
    // copia le curve nell'outline; se gli array non hanno la stessa dimensione
    //  quello con meno curve viene riempito con curve improprie
    //  che hanno i punti di controllo coincidente con l'ultimo estremo valido
    // cout<<"quads del semicerchio left:"<<std::endl;
    copy(/*cout,  */ arrayUp, arrayDown, ob);
  }

  for (int i = startQuad; i <= endQuad; ++i) {
    tq             = (TThickQuadratic *)stroke->getChunk(i);
    TThickPoint p0 = tq->getThickP0();
    TThickPoint p1 = tq->getThickP1();
    TThickPoint p2 = tq->getThickP2();
    if (p0.x == p1.x) {
      if (p1.x == p2.x &&
          ((p1.y > p0.y && p1.y > p2.y) || (p1.y < p0.y && p1.y < p2.y)))
        tq = new TThickQuadratic(p0, 0.5 * (p0 + p1), p1);
    } else if (p0.y == p1.y) {
      if (p0.y == p2.y &&
          ((p1.x > p0.x && p1.x > p2.x) || (p1.x < p0.x && p1.x < p2.x)))
        tq = new TThickQuadratic(p0, 0.5 * (p0 + p1), p1);
    } else {
      double fac1 = 1.0 / (p0.x - p1.x);
      double fac2 = 1.0 / (p0.y - p1.y);

      double aux1 = fac1 * (p2.x - p1.x);
      double aux2 = fac2 * (p2.y - p1.y);
      double aux3 = fac1 * (p0.x - p2.x);
      double aux4 = fac2 * (p0.y - p2.y);

      if ((areAlmostEqual(aux1, aux2) && aux1 >= 0) ||
          (areAlmostEqual(aux3, aux4) && aux3 >= 0 && aux3 <= 1))
        tq = new TThickQuadratic(p0, 0.5 * (p0 + p1), p1);
    }

    // cout<<"quad# "<<i<<":" <<*tq<<std::endl;
    makeOutline(/*cout, */ ob, *tq, error2);
    if (tq != stroke->getChunk(i)) delete tq;
  }

  arrayUp.clear();
  arrayDown.clear();

  // come sopra ultimo pezzo di arco
  //	if (endQuad==stroke->getChunkCount()-1)
  {
    arc = getCircleQuarter(tq, QUARTER_END);
    splitCircularArcIntoQuadraticCurves(tq->getP2(), arc.getP1(), arc.getP0(),
                                        arrayUp);
    changeDirection(arrayUp);
    splitCircularArcIntoQuadraticCurves(tq->getP2(), arc.getP2(), arc.getP1(),
                                        arrayDown);
    changeDirection(arrayDown, true);
    // cout<<"quads del semicerchio right:"<<std::endl;

    copy(/*cout,*/ arrayUp, arrayDown, ob);
  }
}

//-----------------------------------------------------------------------------

static void drawOutline(const outlineBoundary &ob, double pixelSize) {
  for (UINT i = 0; i < ob.size(); ++i) {
    drawQuadratic(*ob[i].first, pixelSize);
    drawQuadratic(*ob[i].second, pixelSize);
  }
}

void computeOutlines(const TStroke *stroke, int startQuad, int endQuad,
                     std::vector<TQuadratic *> &quadArray, double error2) {
  outlineBoundary ob;
  makeOutline(stroke, startQuad, endQuad, ob, error2);

  assert(quadArray.empty());
  quadArray.resize(ob.size() * 2);

  int i, count = 0;
  for (i                                = 0; i < (int)ob.size(); i++)
    if (ob[i].first) quadArray[count++] = ob[i].first;

  for (i                                 = (int)ob.size() - 1; i >= 0; i--)
    if (ob[i].second) quadArray[count++] = ob[i].second;

  quadArray.resize(count);
  for (i = 0; i < (int)quadArray.size(); i++) quadArray[i]->reverse();

  std::reverse(quadArray.begin(), quadArray.end());
}

//-----------------------------------------------------------------------------
// End Of File
//-----------------------------------------------------------------------------
