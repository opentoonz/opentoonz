

#include "tmachine.h"
#include "tpixelgr.h"
#include "quickputP.h"

//#include "tspecialstyleid.h"
#include "tsystem.h"

#include "tcolorstyles.h"
#include "tpixelutils.h"
//#include "tstopwatch.h"
#ifndef TNZCORE_LIGHT
#include "tpalette.h"
#include "trastercm.h"
#include "tropcm.h"
#endif

using namespace TConsts;

#ifdef _WIN32
#include <emmintrin.h>  // per SSE2
#endif

#include <memory>

//===========================================================================
/*
Versione con estensione dell'ultimo pixel e con default_value
per pixel "fuori" dall'immagine di ingresso.


Sistemi di coordinate a meno di una traslazione:

UV: coordinate dell'immagine di partenza
ST: coordinate di filtro (il raggio del filtro e' intero in ST)
FG: coordinate del filtro discretizzato
XY: coordinate dell'immagine di arrivo

Tra UV e ST c'e' una traslazione intera finche' non c'e' ingrandimento
e non c'e' blur. Il blur aggiunge uno scale, e altrettanto fa l'ingrandimento.
Tra ST e FG c'e' uno scale per la risoluzione del filtro.

Oggetti:
out : pixel di output (centro di ST e FG)
ref : pixel di riferimento dell'immagine di input
pix : pixel contribuente

Notazione per le coordinate:
obj_x  : coordinate intere di obj
obj_x_ : coordinate float  di obj

Notazione per le coppie di coordinate:
obj1_obj2_x  : coordinate intere di obj1 rispetto a obj2
obj1_obj2_x_ : coordinate float  di obj1 rispetto a obj2

Matrici affini:
aff_xy2uv  : matrice di trasformazione delle coordinate da XY a UV
aff0_xy2uv : stessa matrice con la parte di shift messa a 0


Una tantum:

aff_uv2xy  = aff
aff_xy2uv  = aff_inv (aff_uv2xy)
aff0_uv2xy = aff_place (00, 00, aff_uv2xy)
  vedi il codice, comunque ottimizzo una rotazione seguita da una scalatura
  anisotropa. Cerco i fattori di scala facendo radici di somme di quadrati.
  Mi regolo sui fattori di scala come se dovessi considerare la scalatura
  da sola. In questo modo tutto si riporta alla vecchia maniera se i fattori
  di scala sono uguali. Se sono diversi il risultato e' comunque esatto per
  rotazioni di multipli di 90 gradi, e non ha discontinuita'.
aff0_uv2st = aff_mult (aff0_xy2st, aff0_uv2xy)
aff0_st2fg = aff_scale (filter_resolution, Aff_I)
aff0_uv2fg = aff_mult (aff0_st2fg, aff0_uv2st)

pix_ref_uv[] = tutti quelli che servono (vedi sotto)
pix_ref_fg_  = AFF_M_V (aff0_uv2fg, pix_ref_uv)
pix_ref_fg[] = ROUND (pix_ref_fg_)


Ciclo su out_xy:

out_uv_ = AFF_M_V (aff_xy2uv, out_xy)
ref_uv = INT_LE (out_uv_)
ref_out_uv_ = ref_uv - out_uv_
ref_out_fg_ = AFF_M_V (aff0_uv2fg, ref_out_uv_)
ref_out_fg  = ROUND (ref_out_fg_)

Ciclo sui pix:

pix_out_fg = pix_ref_fg + ref_out_fg
weight = filter[pix_out_f] * filter[pix_out_g]


Per sapere quali sono i pix che servono:

-filter_fg_radius < pix_out_fg < filter_fg_radius
min_pix_out_uv_ < pix_out_uv_ < max_pix_out_uv_
min_pix_out_uv_ < pix_ref_uv_ + ref_out_uv_ < max_pix_out_uv_
min_pix_out_uv_ + out_ref_uv_ < pix_ref_uv_ < max_pix_out_uv_ + out_ref_uv_
min_pix_out_uv_ < pix_ref_uv_ < max_pix_out_uv_ + 1
Ciclo su tutti quelli che soddisfano questa condizione

0 <= out_ref_uv_ < 1
-1 < ref_out_uv_ <= 0
min_ref_out_fg_ <= ref_out_fg_ <= max_ref_out_fg_
min_ref_out_fg  <= ref_out_fg  <= max_ref_out_fg
-filter_fg_radius < pix_out_fg < filter_fg_radius
-filter_fg_radius < pix_ref_fg + ref_out_fg < filter_fg_radius
-filter_fg_radius - ref_out_fg < pix_ref_fg < filter_fg_radius - ref_out_fg
-filter_fg_radius - max_ref_out_fg < pix_ref_fg < filter_fg_radius -
min_ref_out_fg
Scarto quelli che non soddisfano questa condizione


Come e' fatto il filtro:

  TOP                          filter_array[filter_array_size-1]
   |  filter[max_filter_fg]
   |  filter[max_pix_out_fg]
   |  filter[0]
   |  filter[min_pix_out_fg]
  BOT filter[min_filter_fg] == filter_array[0]

*/

//------------------------------------------------------------------------------
//---------------------------------------------------------------------------

#if !defined(TNZ_LITTLE_ENDIAN)
TNZ_LITTLE_ENDIAN undefined !!
#endif

    // 2^36 * 1.5,  (52-_shiftamt=36) uses limited precision to floor
    const double _double2fixmagic = 68719476736.0 * 1.5;

// 16.16 fixed point representation
const TINT32 _shiftamt = 16;

#if TNZ_LITTLE_ENDIAN
#define iexp_ 1
#define iman_ 0
#else
#define iexp_ 0
#define iman_ 1
#endif

inline TINT32 Double2Int(double val) {
  val = val + _double2fixmagic;
  return ((TINT32 *)&val)[iman_] >> _shiftamt;
}

#define DOUBLE_TO_INT32(D)                                                     \
  (d2iaux = D, d2iaux += _double2fixmagic,                                     \
   (((TINT32 *)&(d2iaux))[iman_] >> _shiftamt))

//#define USE_DOUBLE_TO_INT

//===========================================================================

inline double sinc0(double x, int a) {
  return sin((M_PI / (a)) * (x)) / ((M_PI / (a)) * (x));
}

inline double sinc(double x, int a) {
  return (x) == 0.0 ? 1.0 : sin((M_PI / (a)) * (x)) / ((M_PI / (a)) * (x));
}

inline UCHAR TO8BIT(float X) {
  return (((X) < 0.0F) ? 0 : (((X) > 255.0F) ? 255 : tround(X)));
}

const UCHAR BORDER_GR8 = 255;
const UCHAR GREY_GR8   = 127;

#ifdef USE_INLINE_FUNS

//---------------------------------------------------------------------------

inline double aff0MV1(const TAffine &aff, double v1, double v2) {
  return aff.a11 * v1 + aff.a12 * v2;
}

//---------------------------------------------------------------------------

inline double affMV1(const TAffine &aff, double v1, double v2) {
  return aff.a11 * v1 + aff.a12 * v2 + aff.a13;
}

//---------------------------------------------------------------------------

inline double aff0MV2(const TAffine &aff, double v1, double v2) {
  return aff.a21 * v1 + aff.a22 * v2;
}

//---------------------------------------------------------------------------

inline double affMV2(const TAffine &aff, double v1, double v2) {
  return aff.a21 * v1 + aff.a22 * v2 + aff.a23;
}

#else  // !USE_INLINE_FUNS

#ifndef USE_DOUBLE_TO_INT
#define ROUND(x)                                                               \
  ((int)(((int)(-0.9F) == 0 && (x) < 0.0F) ? ((x)-0.5F) : ((x) + 0.5F)))
#define ROUNDP(x) ((int)((x) + 0.5F))
#define FLOOR(x) ((int)(x) > (x) ? (int)(x)-1 : (int)(x))
#define CEIL(x) ((int)(x) < (x) ? (int)(x) + 1 : (int)(x))
#else
#define ROUND(x)                                                               \
  (DOUBLE_TO_INT32(((int)(-0.9F) == 0 && (x) < 0.0F) ? ((x)-0.5F)              \
                                                     : ((x) + 0.5F)))
#define ROUNDP(x) (DOUBLE_TO_INT32((x) + 0.5F))
#define FLOOR(x)                                                               \
  (DOUBLE_TO_INT32(x) > (x) ? DOUBLE_TO_INT32(x) - 1 : DOUBLE_TO_INT32(x))
#define CEIL(x)                                                                \
  (DOUBLE_TO_INT32(x) < (x) ? DOUBLE_TO_INT32(x) + 1 : DOUBLE_TO_INT32(x))
#endif

#define INTLE(x) (FLOOR(x))
#define INTGT(x) (FLOOR(x) + 1)
#define INTLT(x) (CEIL(x) - 1)
#define INTGE(x) (CEIL(x))

#define NOT_LESS_THAN(MIN, X)                                                  \
  {                                                                            \
    if ((X) < (MIN)) (X) = (MIN);                                              \
  }
#define NOT_MORE_THAN(MAX, X)                                                  \
  {                                                                            \
    if ((X) > (MAX)) (X) = (MAX);                                              \
  }

#define tround ROUND
#define troundp ROUNDP
#define tfloor FLOOR
#define tceil CEIL

#define intLE INTLE
#define intGT INTGT
#define intLT INTLT
#define intGE INTGE

#define notLessThan NOT_LESS_THAN
#define notMoreThan NOT_MORE_THAN

#define AFF0_M_V_1(AFF, V1, V2) ((AFF).a11 * (V1) + (AFF).a12 * (V2))
#define AFF0_M_V_2(AFF, V1, V2) ((AFF).a21 * (V1) + (AFF).a22 * (V2))

#define AFF_M_V_1(AFF, V1, V2) ((AFF).a11 * (V1) + (AFF).a12 * (V2) + (AFF).a13)
#define AFF_M_V_2(AFF, V1, V2) ((AFF).a21 * (V1) + (AFF).a22 * (V2) + (AFF).a23)

#define aff0MV1 AFF0_M_V_1
#define aff0MV2 AFF0_M_V_2

#define affMV1 AFF_M_V_1
#define affMV2 AFF_M_V_2

#endif  // USE_INLINE_FUNS

//---------------------------------------------------------------------------

struct FILTER {
  int first, last;
  float *w;
  float *w_base;
};

struct NOCALC {
  int first, last;
};

//---------------------------------------------------------------------------
inline int get_filter_radius(TRop::ResampleFilterType flt_type) {
  switch (flt_type) {
  case TRop::Triangle:
    return 1;
  case TRop::Mitchell:
    return 2;
  case TRop::Cubic5:
    return 2;
  case TRop::Cubic75:
    return 2;
  case TRop::Cubic1:
    return 2;
  case TRop::Hann2:
    return 2;
  case TRop::Hann3:
    return 3;
  case TRop::Hamming2:
    return 2;
  case TRop::Hamming3:
    return 3;
  case TRop::Lanczos2:
    return 2;
  case TRop::Lanczos3:
    return 3;
  case TRop::Gauss:
    return 2;
  default:
    assert(!"bad filter type");
  }
  return 0;
}

//---------------------------------------------------------------------------

//! Equivalent to aff * TRectD(u0, v0, u1, v0).
inline void minmax(double u0, double v0, double u1, double v1,
                   const TAffine &aff, double &x0, double &y0, double &x1,
                   double &y1) {
  double xmin, ymin;
  double xmax, ymax;
  double x_a, y_a;
  double x_b, y_b;
  double x_c, y_c;
  double x_d, y_d;

  x_a  = affMV1(aff, u0, v0);
  y_a  = affMV2(aff, u0, v0);
  x_b  = affMV1(aff, u1, v0);
  y_b  = affMV2(aff, u1, v0);
  x_c  = affMV1(aff, u1, v1);
  y_c  = affMV2(aff, u1, v1);
  x_d  = affMV1(aff, u0, v1);
  y_d  = affMV2(aff, u0, v1);
  xmin = std::min(x_a, x_b);
  xmax = std::max(x_a, x_b);
  xmin = std::min(xmin, x_c);
  xmax = std::max(xmax, x_c);
  xmin = std::min(xmin, x_d);
  xmax = std::max(xmax, x_d);
  ymin = std::min(y_a, y_b);
  ymax = std::max(y_a, y_b);
  ymin = std::min(ymin, y_c);
  ymax = std::max(ymax, y_c);
  ymin = std::min(ymin, y_d);
  ymax = std::max(ymax, y_d);
  x0   = xmin;
  y0   = ymin;
  x1   = xmax;
  y1   = ymax;
}

/*---------------------------------------------------------------------------*/

/*
inline bool trivial_rot (TAffine inv, int *dudx, int *dudy,
                                    int *dvdx, int *dvdy)
{
*dudx = 0;
*dudy = 0;
*dvdx = 0;
*dvdy = 0;
if (! (inv.a12 == 0 && inv.a21 == 0 || inv.a11 == 0 && inv.a22 == 0))
  return false;
if (! (inv.a11 == 1 || inv.a11 == 0 || inv.a11 == -1))
  return false;
if (! (inv.a12 == 1 || inv.a12 == 0 || inv.a12 == -1))
  return false;
if (! (inv.a21 == 1 || inv.a21 == 0 || inv.a21 == -1))
  return false;
if (! (inv.a22 == 1 || inv.a22 == 0 || inv.a22 == -1))
  return false;
*dudx = (int)inv.a11;
*dudy = (int)inv.a12;
*dvdx = (int)inv.a21;
*dvdy = (int)inv.a22;
return true;
}
*/

//-----------------------------------------------------------------------------

//
// see Mitchell&Netravali, "Reconstruction Filters in Computer Graphics",
// SIGGRAPH 88.  Mitchell code provided by Paul Heckbert.
//
//

//-----------------------------------------------------------------------------

static double p0, p2, p3, q0, q1, q2, q3;

inline void mitchellinit(double b, double c) {
  p0 = (6.0 - 2.0 * b) / 6.0;
  p2 = (-18.0 + 12.0 * b + 6.0 * c) / 6.0;
  p3 = (12.0 - 9.0 * b - 6.0 * c) / 6.0;
  q0 = (8.0 * b + 24.0 * c) / 6.0;
  q1 = (-12.0 * b - 48.0 * c) / 6.0;
  q2 = (6.0 * b + 30.0 * c) / 6.0;
  q3 = (-b - 6.0 * c) / 6.0;
}

const int fltradMitchell = 2;
static inline double flt_mitchell(
    double x) /*Mitchell & Netravali's two-param cubic*/
{
  static int mitfirsted;

  if (!mitfirsted) {
    mitchellinit(1.0 / 3.0, 1.0 / 3.0);
    mitfirsted = 1;
  }
  if (x < -2.0) return 0.0;
  if (x < -1.0) return (q0 - x * (q1 - x * (q2 - x * q3)));
  if (x < 0.0) return (p0 + x * x * (p2 - x * p3));
  if (x < 1.0) return (p0 + x * x * (p2 + x * p3));
  if (x < 2.0) return (q0 + x * (q1 + x * (q2 + x * q3)));
  return 0.0;
}

//-----------------------------------------------------------------------------

const int fltradTriangle = 1;
static inline double flt_triangle(double x) {
  if (x < -1.0) return 0.0;
  if (x < 0.0) return 1.0 + x;
  if (x < 1.0) return 1.0 - x;
  return 0.0;
}

//-----------------------------------------------------------------------------

const int fltradCubic5 = 2;
static inline double flt_cubic_5(double x) {
  if (x < 0.0) x = -x;
  if (x < 1.0) return 2.5 * x * x * x - 3.5 * x * x + 1;
  if (x < 2.0) return 0.5 * x * x * x - 2.5 * x * x + 4 * x - 2;
  return 0.0;
}

//-----------------------------------------------------------------------------

const int fltradCubic75 = 2;
static inline double flt_cubic_75(double x) {
  if (x < 0.0) x = -x;
  if (x < 1.0) return 2.75 * x * x * x - 3.75 * x * x + 1;
  if (x < 2.0) return 0.75 * x * x * x - 3.75 * x * x + 6 * x - 3;
  return 0.0;
}

//-----------------------------------------------------------------------------

const int fltradCubic1 = 2;
static inline double flt_cubic_1(double x) {
  if (x < 0.0) x = -x;
  if (x < 1.0) return 3 * x * x * x - 4 * x * x + 1;
  if (x < 2.0) return x * x * x - 5 * x * x + 8 * x - 4;
  return 0.0;
}

//-----------------------------------------------------------------------------

const int fltradHann2 = 2;
static inline double flt_hann2(double x) {
  if (x <= -2.0) return 0.0;
  if (x < 2.0) return sinc(x, 1) * (0.5 + 0.5 * cos(M_PI_2 * x));
  return 0.0;
}

//-----------------------------------------------------------------------------

const int fltradHann3 = 3;
static inline double flt_hann3(double x) {
  if (x <= -3.0) return 0.0;
  if (x < 3.0) return sinc(x, 1) * (0.5 + 0.5 * cos(M_PI_3 * x));
  return 0.0;
}

//-----------------------------------------------------------------------------

const int fltradHamming2 = 2;
static inline double flt_hamming2(double x) {
  if (x <= -2.0) return 0.0;
  if (x < 2.0) return sinc(x, 1) * (0.54 + 0.46 * cos(M_PI_2 * x));
  return 0.0;
}

//-----------------------------------------------------------------------------

const int fltradHamming3 = 3;
static inline double flt_hamming3(double x) {
  if (x <= -3.0) return 0.0;
  if (x < 3.0) return sinc(x, 1) * (0.54 + 0.46 * cos(M_PI_3 * x));
  return 0.0;
}

//-----------------------------------------------------------------------------

const int fltradLanczos2 = 2;
static inline double flt_lanczos2(double x) {
  if (x <= -2.0) return 0.0;
  if (x < 2.0) return sinc(x, 1) * sinc(x, 2);
  return 0.0;
}

//-----------------------------------------------------------------------------

const int fltradLanczos3 = 3;
static inline double flt_lanczos3(double x) {
  if (x <= -3.0) return 0.0;
  if (x < 3.0) return sinc(x, 1) * sinc(x, 3);
  return 0.0;
}

//-----------------------------------------------------------------------------

const int fltradGauss = 2;
static inline double flt_gauss(double x) {
  if (x <= -2.0) return 0.0;
  if (x < 2.0) return exp(-M_PI * x * x);
  return 0.0; /* exp(-M_PI*2*2)~=3.5*10^-6 */
}

//-----------------------------------------------------------------------------

const int fltradW1 = 2;
static inline double flt_w_1(double x) {
  if (x < 0.0) x = -x;
  if (x < 0.5) return 1 - 0.5 * x;
  if (x < 1.0) return 1.5 - 1.5 * x;
  if (x < 1.5) return 0.5 - 0.5 * x;
  if (x < 2.0) return 0.5 * x - 1.0;
  return 0.0;
}

//-----------------------------------------------------------------------------

static inline void get_flt_fun_rad(TRop::ResampleFilterType flt_type,
                                   double (**flt_fun)(double),
                                   double &flt_rad) {
  double (*fun)(double);
  double rad;

  switch (flt_type) {
  case TRop::Triangle:
    fun = flt_triangle;
    rad = fltradTriangle;
    break;
  case TRop::Mitchell:
    fun = flt_mitchell;
    rad = fltradMitchell;
    break;
  case TRop::Cubic5:
    fun = flt_cubic_5;
    rad = fltradCubic5;
    break;
  case TRop::Cubic75:
    fun = flt_cubic_75;
    rad = fltradCubic75;
    break;
  case TRop::Cubic1:
    fun = flt_cubic_1;
    rad = fltradCubic1;
    break;
  case TRop::Hann2:
    fun = flt_hann2;
    rad = fltradHann2;
    break;
  case TRop::Hann3:
    fun = flt_hann3;
    rad = fltradHann3;
    break;
  case TRop::Hamming2:
    fun = flt_hamming2;
    rad = fltradHamming2;
    break;
  case TRop::Hamming3:
    fun = flt_hamming3;
    rad = fltradHamming3;
    break;
  case TRop::Lanczos2:
    fun = flt_lanczos2;
    rad = fltradLanczos2;
    break;
  case TRop::Lanczos3:
    fun = flt_lanczos3;
    rad = fltradLanczos3;
    break;
  case TRop::Gauss:
    fun = flt_gauss;
    rad = fltradGauss;
    break;
  case 101:
    fun = flt_w_1;
    rad = fltradW1;
    break;
  default:
    fun = flt_triangle;
    rad = fltradTriangle;
    break;
  }
  if (flt_fun) *flt_fun = fun;
  flt_rad               = rad;
}

//---------------------------------------------------------------------------

static FILTER *create_filter(TRop::ResampleFilterType flt_type, double blur,
                             double dx_du, double delta_x, int lx, double &xrad,
                             int &umin, int &umax, int &uwidth) {
  double (*flt_fun)(double);
  FILTER *filter, *f;
  double du_dx;
  int x;
  double u_;
  int u, ulo, uhi, ulomin, uhimax, m, n, nmax;
  double flt_rad, rad_u, rad_x, nodedist_u, nodefreq_u, sum, norm, w;

#ifdef USE_DOUBLE_TO_INT
  double d2iaux;
#endif

  get_flt_fun_rad(flt_type, &flt_fun, flt_rad);
  du_dx = 1 / dx_du;
  if (dx_du > 1)
    nodedist_u = blur; /* magnification */
  else
    nodedist_u = du_dx * blur; /* minification */
  rad_u        = flt_rad * nodedist_u;
  rad_x        = rad_u * dx_du;
  nodefreq_u   = 1 / nodedist_u;
  /*
mu = lu - 1;
*/
  filter = new FILTER[lx];
  nmax   = 0;
  ulomin = c_maxint - 1;
  uhimax = c_minint + 1;
  for (x = 0; x < lx; x++) {
    f   = filter + x;
    u_  = (x - delta_x) * du_dx;
    ulo = intGT(u_ - rad_u);
    uhi = intLT(u_ + rad_u);
    /*
NOT_LESS_THAN( 0, ulo)
NOT_MORE_THAN(mu, uhi)
*/
    m = uhi - ulo + 1;
    if (m > 0) {
      f->w_base = new float[m];
      f->w      = f->w_base - ulo;
      for (sum = 0.0, u = ulo; u <= uhi; u++) {
        w = (*flt_fun)((u - u_) * nodefreq_u);
        sum += w;
        f->w[u] = (float)w;
      }
      for (; ulo <= uhi; ulo++)
        if (f->w[ulo]) break;
      for (; uhi >= ulo; uhi--)
        if (f->w[uhi]) break;
      if (ulo < ulomin) ulomin = ulo;
      if (uhi > uhimax) uhimax = uhi;
      n                        = uhi - ulo + 1;
      if (n > nmax) nmax       = n;
      f->first                 = ulo;
      f->last                  = uhi;
      norm                     = 1 / sum;
      for (u = ulo; u <= uhi; u++) f->w[u] *= (float)norm;
    } else {
      f->w_base = 0;
      f->first  = ulo;
      f->last   = uhi;
    }
  }
  xrad   = rad_x;
  umin   = ulomin;
  umax   = uhimax;
  uwidth = nmax;
  return filter;
}

//-----------------------------------------------------------------------------

static NOCALC *create_nocalc(TRop::ResampleFilterType flt_type, double blur,
                             double dx_du, double delta_x, int lx, int umin,
                             int umax, int &xwidth) {
  /*

Il nocalc serve a stabilire che un insieme di pixel u (di ingresso)
non ha bisogno di essere calcolato, perche tutti i pixel x (di uscita)
su cui questo insieme si distribuisce non hanno bisogno di essere calcolati.

Il significato del nocalc a coordinata x e':
se arrivati a x si e' trovata una sequenza di width pixel x che
non e' necessario calcolare, allora non e' necessario calcolare tutti i
pixel u da nocalc->first a nocalc->last.

Per primo va calcolata la width. Deve essere garantito che tutti i pixel u
siano coperti dal vettore di nocalc.
Un pixel u viene usato da un intervallo di x largo quanto il filtro (in x),
cioe' un intervallo aperto (-radx_,radx_) intorno a x(u).
Aggiungendo una unita' x a questa larghezza si ha una larghezza tale che
se tutti i pixel x sono nocalc, un intervallo largo 1 in unita' x
di pixel u non necessita di essere calcolato.
Vogliamo che ulo_ <= first <= last < uhi_ con uhi_ = ulo_ + u(1).
Devono essere nocalc almeno gli x in (x(ulo_)-radx_, x(uhi_)+radx_).
Poniamo x = x(uhi_)+radx_-1.
uhi_ = u(x-radx_+1)
ulo_ = u(x-radx_)
x(ulo_)-radx_ = x-2*radx_   ma questo punto e' escluso, quindi l'intero GT e'
x - width + 1 = INT_LE (x-2*radx_+1)
1 - INT_LE (-2*radx_+1) = width
1 + INT_GE (2*radx_-1) = width
INT_GE (2*radx_) = width
Pero' per sicurezza facciamo
INT_GT (2*radx_) = width

*/

  NOCALC *nocalc;
  int width;
  double flt_rad;
  double rad_x;
  double du_dx;
  double ulo_, uhi_;
  int ulo, uhi;
  int x;

#ifdef USE_DOUBLE_TO_INT
  double d2iaux;
#endif

  du_dx = 1 / dx_du;
  get_flt_fun_rad(flt_type, 0, flt_rad);
  if (dx_du > 1) /* sto ingrandendo */
    rad_x = flt_rad * blur * dx_du;
  else
    rad_x = flt_rad * blur;
  rad_x += 0.5; /* ?!?!?!?!? */
  width  = intGT(2 * rad_x + 1);
  nocalc = new NOCALC[lx + width - 1];
  for (x = 0; x < lx + width - 1; x++) {
    ulo_            = (x - rad_x - delta_x) * du_dx;
    uhi_            = ulo_ + du_dx;
    ulo             = intGE(ulo_);
    uhi             = intLT(uhi_);
    nocalc[x].first = std::max(umin, ulo);
    nocalc[x].last  = std::min(umax, uhi);
  }
  xwidth = width;

  return nocalc;
}

//---------------------------------------------------------------------------
/*
inline UINT calcValueInit(UINT init_value){
  return init_value;//0xffffU;
}
*/
/*
inline void calcValueInit(UINT &calc_value){
  calc_value = 0xffffU;
}

inline bool calcValueEmpty(UINT calc_value){
  return calc_value == 0xffffU;
}
inline bool calcValueReady(UINT calc_value){
  return calc_value <=  0x1ffU;
}

inline void calcValueAdvance(UINT &calc_value){
  calc_value >>= 1;
}

inline void calcValueNoCalc(UINT &calc_value){
  calc_value &= ~0x80U;
}
*/
#define CALC_VALUE_INIT                                                        \
  { calc_value = 0xffffU; }
#define CALC_VALUE_EMPTY (calc_value == 0xffffU)
#define CALC_VALUE_READY (calc_value <= 0x1ffU)
#define CALC_VALUE_ADVANCE                                                     \
  { calc_value >>= 1; }
#define CALC_VALUE_NOCALC                                                      \
  { calc_value &= ~0x80U; }

template <typename PixType>
#ifdef _MSC_VER
__forceinline
#endif
    void
    ResampleCalcAlgo(PixType *buffer_in, int lu, int lv, int wrap_in,
                     int max_pix_ref_u, int min_pix_ref_u, int max_pix_ref_v,
                     int min_pix_ref_v, UCHAR *calc, int calc_bytesize,
                     int calc_bytewrap)
/*
lu = width
lv = height
wrap_in = wrap
*/
{
  PixType *prev_line_in;
  PixType *last_line_in;
  PixType prev_value;
  PixType left_value;
  PixType last_value;

  UINT calc_value;
  UCHAR *calc_byte = 0;
  int goodcols;
  std::unique_ptr<int[]> col_height(new int[lu]);
  int ref_u, ref_v;
  int filter_diam_u = max_pix_ref_u - min_pix_ref_u + 1;
  int filter_diam_v = max_pix_ref_v - min_pix_ref_v + 1;
  int last_u, last_v;

  int *ch;
  int *ch_end;

  assert(col_height);

  CALC_VALUE_INIT
  ch     = col_height.get();
  ch_end = ch + lu;

  while (ch < ch_end) {
    *ch = filter_diam_v;
    ++ch;
  }

  last_line_in = buffer_in;
  for (last_v = 1, ref_v = last_v - max_pix_ref_v; ref_v < 0;
       last_v++, ref_v++) {
    prev_line_in = last_line_in;
    last_line_in = buffer_in + last_v * wrap_in;
    for (last_u = 0; last_u < lu; last_u++) {
      last_value = last_line_in[last_u];
      prev_value = prev_line_in[last_u];
      if (last_value == prev_value)
        col_height[last_u]++;
      else
        col_height[last_u] = 1;
    }
  }

  for (; last_v < lv; last_v++, ref_v++) {
    prev_line_in = last_line_in;
    last_line_in = buffer_in + last_v * wrap_in;
    last_value   = last_line_in[0];
    goodcols     = 0;
    for (last_u = 0, ref_u = last_u - max_pix_ref_u; ref_u < 0;
         last_u++, ref_u++) {
      left_value = last_value;
      last_value = last_line_in[last_u];
      prev_value = prev_line_in[last_u];
      if (last_value == prev_value) {
        col_height[last_u]++;
        if (col_height[last_u] >= filter_diam_v)
          if (last_value == left_value)
            goodcols++;
          else
            goodcols = 1;
        else
          goodcols = 0;
      } else {
        col_height[last_u] = 1;
        goodcols           = 0;
      }
    }
    calc_byte = calc + calc_bytewrap * ref_v;
    CALC_VALUE_INIT
    for (; last_u < lu; last_u++, ref_u++) {
      left_value = last_value;
      last_value = last_line_in[last_u];
      prev_value = prev_line_in[last_u];
      if (last_value == prev_value) {
        col_height[last_u]++;
        if (col_height[last_u] >= filter_diam_v)
          if (last_value == left_value) {
            goodcols++;
            if (goodcols >= filter_diam_u) CALC_VALUE_NOCALC
          } else
            goodcols = 1;
        else
          goodcols = 0;
      } else {
        col_height[last_u] = 1;
        goodcols           = 0;
      }
      if (CALC_VALUE_READY) {
        *calc_byte++ = (UCHAR)calc_value;
        CALC_VALUE_INIT
      } else
        CALC_VALUE_ADVANCE
    }
    for (; ref_u < lu; last_u++, ref_u++) {
      if (CALC_VALUE_READY) {
        *calc_byte++ = (UCHAR)calc_value;
        CALC_VALUE_INIT
      } else
        CALC_VALUE_ADVANCE
    }
    if (!CALC_VALUE_EMPTY) {
      while (!CALC_VALUE_READY) CALC_VALUE_ADVANCE
      *calc_byte++ = (UCHAR)calc_value;
    }
  }

  for (; ref_v < lv; last_v++, ref_v++) {
    for (last_u = 0, ref_u = last_u - max_pix_ref_u; ref_u < 0;
         last_u++, ref_u++) {
    }
    calc_byte = calc + calc_bytewrap * ref_v;
    CALC_VALUE_INIT
    for (; last_u < lu; last_u++, ref_u++) {
      if (CALC_VALUE_READY) {
        *calc_byte++ = (UCHAR)calc_value;
        CALC_VALUE_INIT
      } else
        CALC_VALUE_ADVANCE
    }
    for (; ref_u < lu; last_u++, ref_u++) {
      if (CALC_VALUE_READY) {
        *calc_byte++ = (UCHAR)calc_value;
        CALC_VALUE_INIT
      } else
        CALC_VALUE_ADVANCE
    }
    if (!CALC_VALUE_EMPTY) {
      while (!CALC_VALUE_READY) CALC_VALUE_ADVANCE
      *calc_byte++ = (UCHAR)calc_value;
    }
  }
  assert(!calc_byte || calc_byte == calc + calc_bytesize);
}

/*---------------------------------------------------------------------------*/

template <class T>
void create_calc(const TRasterPT<T> &rin, int min_pix_ref_u, int max_pix_ref_u,
                 int min_pix_ref_v, int max_pix_ref_v, UCHAR *&p_calc,
                 int &p_calc_allocsize, int &p_calc_bytewrap) {
  UCHAR *calc;
  int lu, lv;
  int wrap_in;
  int calc_bytesize;
  int calc_bytewrap;

  lu      = rin->getLx();
  lv      = rin->getLy();
  wrap_in = rin->getWrap();

  p_calc_bytewrap = (lu + 7) >> 3;  // ceil(lu/8)
  calc_bytewrap   = p_calc_bytewrap;
  calc_bytesize   = calc_bytewrap * lv;  // lv * ceil(lu/8)
  if (calc_bytesize > p_calc_allocsize) {
    if (p_calc_allocsize) delete[](p_calc);
    // TMALLOC (*p_calc, calc_bytesize)
    p_calc = new UCHAR[calc_bytesize];
    assert(p_calc);
    memset(p_calc, 0xff, calc_bytesize);
    p_calc_allocsize = calc_bytesize;
  }
  calc = p_calc;
  if (lu < max_pix_ref_u + 1 || lv < max_pix_ref_v + 1) {
    memset(calc, 0xff, calc_bytesize);
    return;
  }

  // RESAMPLE_CALC_ALGO

  ResampleCalcAlgo<T>(rin->pixels(), lu, lv, wrap_in, max_pix_ref_u,
                      min_pix_ref_u, max_pix_ref_v, min_pix_ref_v, calc,
                      calc_bytesize, calc_bytewrap);
}

//---------------------------------------------------------------------------

namespace {

template <class T>
class Converter {
public:
  static inline T convert(const TPixel32 &pixin) { return pixin; }
};

#define BYTE_FROM_USHORT(u) (((256U * 255U + 1U) * u + (1 << 23)) >> 24)
#define USHORT_FROM_BYTE(u) (u | u << 8)

template <>
class Converter<TPixel64> {
public:
  static inline TPixel64 convert(const TPixel32 &pix) {
    return TPixel64(USHORT_FROM_BYTE(pix.r), USHORT_FROM_BYTE(pix.g),
                    USHORT_FROM_BYTE(pix.b), USHORT_FROM_BYTE(pix.m));
  }
};

//-----------------------------------------------------------------------------

inline double get_filter_value(TRop::ResampleFilterType flt_type, double x) {
  // it is assumed that x != 0 (not checked only for speed reasons)
  switch (flt_type) {
  case TRop::Triangle:
    if (x < -1.0) return 0.0;
    if (x < 0.0) return 1.0 + x;
    if (x < 1.0) return 1.0 - x;
    return 0.0;

  case TRop::Mitchell: {
    static double p0, p2, p3, q0, q1, q2, q3;

    if (!p0) {
      const double b = 1.0 / 3.0;
      const double c = 1.0 / 3.0;

      p0 = (6.0 - 2.0 * b) / 6.0;
      p2 = (-18.0 + 12.0 * b + 6.0 * c) / 6.0;
      p3 = (12.0 - 9.0 * b - 6.0 * c) / 6.0;
      q0 = (8.0 * b + 24.0 * c) / 6.0;
      q1 = (-12.0 * b - 48.0 * c) / 6.0;
      q2 = (6.0 * b + 30.0 * c) / 6.0;
      q3 = (-b - 6.0 * c) / 6.0;
    }
    if (x < -2.0) return 0.0;
    if (x < -1.0) return (q0 - x * (q1 - x * (q2 - x * q3)));
    if (x < 0.0) return (p0 + x * x * (p2 - x * p3));
    if (x < 1.0) return (p0 + x * x * (p2 + x * p3));
    if (x < 2.0) return (q0 + x * (q1 + x * (q2 + x * q3)));

    break;
  }

  case TRop::Cubic5:
    if (x < 0.0) x = -x;
    if (x < 1.0) return 2.5 * x * x * x - 3.5 * x * x + 1;
    if (x < 2.0) return 0.5 * x * x * x - 2.5 * x * x + 4 * x - 2;
    break;

  case TRop::Cubic75:
    if (x < 0.0) x = -x;
    if (x < 1.0) return 2.75 * x * x * x - 3.75 * x * x + 1;
    if (x < 2.0) return 0.75 * x * x * x - 3.75 * x * x + 6 * x - 3;
    break;

  case TRop::Cubic1:
    if (x < 0.0) x = -x;
    if (x < 1.0) return 3 * x * x * x - 4 * x * x + 1;
    if (x < 2.0) return x * x * x - 5 * x * x + 8 * x - 4;
    break;

  case TRop::Hann2:
    if (x <= -2.0) return 0.0;
    if (x < 2.0) return sinc0(x, 1) * (0.5 + 0.5 * cos(M_PI_2 * x));
    break;

  case TRop::Hann3:
    if (x <= -3.0) return 0.0;
    if (x < 3.0) return sinc0(x, 1) * (0.5 + 0.5 * cos(M_PI_3 * x));
    break;

  case TRop::Hamming2:
    if (x <= -2.0) return 0.0;
    if (x < 2.0) return sinc0(x, 1) * (0.54 + 0.46 * cos(M_PI_2 * x));
    break;

  case TRop::Hamming3:
    if (x <= -3.0) return 0.0;
    if (x < 3.0) return sinc0(x, 1) * (0.54 + 0.46 * cos(M_PI_3 * x));
    break;

  case TRop::Lanczos2:
    if (x <= -2.0) return 0.0;
    if (x < 2.0) return sinc0(x, 1) * sinc0(x, 2);
    break;

  case TRop::Lanczos3:
    if (x <= -3.0) return 0.0;
    if (x < 3.0) return sinc0(x, 1) * sinc0(x, 3);
    break;

  case TRop::Gauss:
    if (x <= -2.0) return 0.0;
    if (x < 2.0) return exp(-M_PI * x * x); /* exp(-M_PI*2*2)~=3.5*10^-6 */
    break;
  default:
    assert(!"bad filter type");
    break;
  }
  return 0.0;
}

//---------------------------------------------------------------------------
template <class T>
void resample_clear_rgbm(TRasterPT<T> rout, T default_value) {
  T *buffer_out;
  buffer_out = rout->pixels();
  for (int out_y = 0; out_y < rout->getLy(); out_y++)
    for (int out_x = 0; out_x < rout->getLx(); out_x++)
      buffer_out[out_x + out_y * rout->getWrap()] = default_value;
}

//---------------------------------------------------------------------------

template <class T, typename SUMS_TYPE>
void resample_main_rgbm(TRasterPT<T> rout, const TRasterPT<T> &rin,
                        const TAffine &aff_xy2uv, const TAffine &aff0_uv2fg,
                        int min_pix_ref_u, int min_pix_ref_v, int max_pix_ref_u,
                        int max_pix_ref_v, int n_pix, int *pix_ref_u,
                        int *pix_ref_v, int *pix_ref_f, int *pix_ref_g,
                        short *filter) {
  const T *buffer_in;
  T *buffer_out;
  T *pix_out;
  int lu, lv, wrap_in, mu, mv;
  int lx, ly, wrap_out;
  int out_x, out_y;
  double out_x_, out_y_;
  double out_u_, out_v_;
  int ref_u, ref_v;
  int pix_u, pix_v;
  double ref_out_u_, ref_out_v_;
  double ref_out_f_, ref_out_g_;
  int ref_out_f, ref_out_g;
  int pix_out_f, pix_out_g;
  int filter_mu, filter_mv;
  UINT inside_limit_u, inside_limit_v;
  int inside_nonempty;
  int outside_min_u, outside_min_v;
  int outside_max_u, outside_max_v;
  UCHAR *calc;
  int calc_allocsize;
  int calc_bytewrap;
  UCHAR calc_value;
  bool must_calc;
  T pix_value, default_value(0, 0, 0, 0);
  SUMS_TYPE weight, sum_weights;
  double inv_sum_weights;
  SUMS_TYPE sum_contribs_r, sum_contribs_g, sum_contribs_b, sum_contribs_m;
  double out_fval_r, out_fval_g, out_fval_b, out_fval_m;
  int out_value_r, out_value_g, out_value_b, out_value_m;
  int i;

#ifdef USE_DOUBLE_TO_INT
  double d2iaux;
#endif

  if (!(rout->getLx() > 0 && rout->getLy() > 0)) return;

  if (!(rin->getLx() > 0 && rin->getLy() > 0)) {
    rout->clear();
    return;
  }

  calc           = 0;
  calc_allocsize = 0;

  // Create a bit array, each indicating whether a pixel has to be calculated or
  // not
  create_calc(rin, min_pix_ref_u, max_pix_ref_u, min_pix_ref_v, max_pix_ref_v,
              calc, calc_allocsize, calc_bytewrap);

  buffer_in  = rin->pixels();
  buffer_out = rout->pixels();
  lu         = rin->getLx();
  lx         = rout->getLx();
  lv         = rin->getLy();
  ly         = rout->getLy();
  wrap_in    = rin->getWrap();
  wrap_out   = rout->getWrap();
  mu         = lu - 1;
  mv         = lv - 1;

  filter_mu       = max_pix_ref_u - min_pix_ref_u;
  filter_mv       = max_pix_ref_v - min_pix_ref_v;
  inside_limit_u  = lu - filter_mu;
  inside_limit_v  = lv - filter_mv;
  inside_nonempty = (int)inside_limit_u > 0 && (int)inside_limit_v > 0;
  outside_min_u   = -max_pix_ref_u;
  outside_min_v   = -max_pix_ref_v;
  outside_max_u   = mu - min_pix_ref_u;
  outside_max_v   = mv - min_pix_ref_v;

  // For every pixel of the output image
  for (out_y = 0, out_y_ = 0.5; out_y < ly; out_y++, out_y_ += 1.0) {
    for (out_x = 0, out_x_ = 0.5; out_x < lx; out_x++, out_x_ += 1.0) {
      pix_out = buffer_out + out_y * wrap_out + out_x;

      // Take the pre-image of the pixel through the passed affine
      out_u_ = affMV1(aff_xy2uv, out_x_, out_y_);
      out_v_ = affMV2(aff_xy2uv, out_x_, out_y_);

      // Convert to integer coordinates
      ref_u = intLE(out_u_);
      ref_v = intLE(out_v_);

      // NOTE: The following condition is equivalent to:
      // (ref_u + min_pix_ref_u >= 0 && ref_v + min_pix_ref_v >= 0 &&
      //  ref_u + max_pix_ref_u < lu && ref_v + max_pix_ref_v < lv)
      // - since the presence of (UINT) makes integeres < 0 become >> 0
      if (inside_nonempty && (UINT)(ref_u + min_pix_ref_u) < inside_limit_u &&
          (UINT)(ref_v + min_pix_ref_v) < inside_limit_v) {
        // The filter mask starting around (ref_u, ref_v) is completely
        // contained
        // in the source raster

        // Get the calculation array mask byte
        calc_value = calc[(ref_u >> 3) + ref_v * calc_bytewrap];
        if (calc_value && ((calc_value >> (ref_u & 7)) &
                           1))  // If the mask bit for this pixel is on
        {
          ref_out_u_ = ref_u - out_u_;  // Fractionary part of the pre-image
          ref_out_v_ = ref_v - out_v_;
          ref_out_f_ = aff0MV1(aff0_uv2fg, ref_out_u_,
                               ref_out_v_);  // Make the image of it into fg
          ref_out_g_ = aff0MV2(aff0_uv2fg, ref_out_u_, ref_out_v_);
          ref_out_f  = tround(ref_out_f_);  // Convert to integer coordinates
          ref_out_g  = tround(ref_out_g_);

          sum_weights    = 0;
          sum_contribs_r = 0;
          sum_contribs_g = 0;
          sum_contribs_b = 0;
          sum_contribs_m = 0;

          // Make the weighted sum of source pixels
          for (i = n_pix - 1; i >= 0; --i) {
            // Build the weight for this pixel
            pix_out_f = pix_ref_f[i] + ref_out_f;  // image of the integer part
                                                   // + that of the fractionary
                                                   // part
            pix_out_g = pix_ref_g[i] + ref_out_g;
            weight    = (filter[pix_out_f] * filter[pix_out_g]) >> 16;

            // Add the weighted pixel contribute
            pix_u = pix_ref_u[i] + ref_u;
            pix_v = pix_ref_v[i] + ref_v;

            pix_value = buffer_in[pix_u + pix_v * wrap_in];
            sum_contribs_r += (SUMS_TYPE)pix_value.r * weight;
            sum_contribs_g += (SUMS_TYPE)pix_value.g * weight;
            sum_contribs_b += (SUMS_TYPE)pix_value.b * weight;
            sum_contribs_m += (SUMS_TYPE)pix_value.m * weight;
            sum_weights += weight;
          }

          inv_sum_weights = 1.0 / sum_weights;
          out_fval_r      = sum_contribs_r * inv_sum_weights;
          out_fval_g      = sum_contribs_g * inv_sum_weights;
          out_fval_b      = sum_contribs_b * inv_sum_weights;
          out_fval_m      = sum_contribs_m * inv_sum_weights;
          notLessThan(0.0, out_fval_r);
          notLessThan(0.0, out_fval_g);
          notLessThan(0.0, out_fval_b);
          notLessThan(0.0, out_fval_m);
          out_value_r = troundp(out_fval_r);
          out_value_g = troundp(out_fval_g);
          out_value_b = troundp(out_fval_b);
          out_value_m = troundp(out_fval_m);
          notMoreThan(T::maxChannelValue, out_value_r);
          notMoreThan(T::maxChannelValue, out_value_g);
          notMoreThan(T::maxChannelValue, out_value_b);
          notMoreThan(T::maxChannelValue, out_value_m);
          pix_out->r = out_value_r;
          pix_out->g = out_value_g;
          pix_out->b = out_value_b;
          pix_out->m = out_value_m;
        } else
          // The pixel is copied from the corresponding source...
          *pix_out = buffer_in[ref_u + ref_v * wrap_in];
      } else if (outside_min_u <= ref_u && ref_u <= outside_max_u &&
                 outside_min_v <= ref_v && ref_v <= outside_max_v) {
        if ((UINT)ref_u >= (UINT)lu || (UINT)ref_v >= (UINT)lv)
          must_calc = true;
        else {
          calc_value = calc[(ref_u >> 3) + ref_v * calc_bytewrap];
          must_calc  = calc_value && ((calc_value >> (ref_u & 7)) & 1);
        }

        if (must_calc) {
          ref_out_u_     = ref_u - out_u_;
          ref_out_v_     = ref_v - out_v_;
          ref_out_f_     = aff0MV1(aff0_uv2fg, ref_out_u_, ref_out_v_);
          ref_out_g_     = aff0MV2(aff0_uv2fg, ref_out_u_, ref_out_v_);
          ref_out_f      = tround(ref_out_f_);
          ref_out_g      = tround(ref_out_g_);
          sum_weights    = 0;
          sum_contribs_r = 0;
          sum_contribs_g = 0;
          sum_contribs_b = 0;
          sum_contribs_m = 0;

          for (i = n_pix - 1; i >= 0; --i) {
            pix_out_f = pix_ref_f[i] + ref_out_f;
            pix_out_g = pix_ref_g[i] + ref_out_g;
            weight    = (filter[pix_out_f] * filter[pix_out_g]) >> 16;
            pix_u     = pix_ref_u[i] + ref_u;
            pix_v     = pix_ref_v[i] + ref_v;

            if (pix_u < 0 || pix_u > mu || pix_v < 0 || pix_v > mv) {
              sum_weights += weight;  // 0-padding
              continue;
            }

            notLessThan(0, pix_u);  // Copy-padding
            notLessThan(0, pix_v);
            notMoreThan(mu, pix_u);
            notMoreThan(mv, pix_v);

            pix_value = buffer_in[pix_u + pix_v * wrap_in];
            sum_contribs_r += (SUMS_TYPE)pix_value.r * weight;
            sum_contribs_g += (SUMS_TYPE)pix_value.g * weight;
            sum_contribs_b += (SUMS_TYPE)pix_value.b * weight;
            sum_contribs_m += (SUMS_TYPE)pix_value.m * weight;
            sum_weights += weight;
          }

          inv_sum_weights = 1.0 / sum_weights;
          out_fval_r      = sum_contribs_r * inv_sum_weights;
          out_fval_g      = sum_contribs_g * inv_sum_weights;
          out_fval_b      = sum_contribs_b * inv_sum_weights;
          out_fval_m      = sum_contribs_m * inv_sum_weights;
          notLessThan(0.0, out_fval_r);
          notLessThan(0.0, out_fval_g);
          notLessThan(0.0, out_fval_b);
          notLessThan(0.0, out_fval_m);
          out_value_r = troundp(out_fval_r);
          out_value_g = troundp(out_fval_g);
          out_value_b = troundp(out_fval_b);
          out_value_m = troundp(out_fval_m);
          notMoreThan(T::maxChannelValue, out_value_r);
          notMoreThan(T::maxChannelValue, out_value_g);
          notMoreThan(T::maxChannelValue, out_value_b);
          notMoreThan(T::maxChannelValue, out_value_m);
          pix_out->r = out_value_r;
          pix_out->g = out_value_g;
          pix_out->b = out_value_b;
          pix_out->m = out_value_m;
        } else
          *pix_out = buffer_in[ref_u + ref_v * wrap_in];
      } else
        *pix_out = default_value;
    }
  }

  delete[] calc;
}

//---------------------------------------------------------------------------

#ifdef _WIN32

namespace {

DV_ALIGNED(16) class TPixelFloat {
public:
  TPixelFloat() : b(0), g(0), r(0), m(0) {}

  TPixelFloat(float rr, float gg, float bb, float mm)
      : b(bb), g(gg), r(rr), m(mm) {}

  TPixelFloat(const TPixel32 &pix) : b(pix.b), g(pix.g), r(pix.r), m(pix.m) {}

  float b, g, r, m;
};

}  // anonymous namespace

//---------------------------------------------------------------------------

template <class T>
void resample_main_rgbm_SSE2(TRasterPT<T> rout, const TRasterPT<T> &rin,
                             const TAffine &aff_xy2uv,
                             const TAffine &aff0_uv2fg, int min_pix_ref_u,
                             int min_pix_ref_v, int max_pix_ref_u,
                             int max_pix_ref_v, int n_pix, int *pix_ref_u,
                             int *pix_ref_v, int *pix_ref_f, int *pix_ref_g,
                             short *filter) {
  __m128i zeros = _mm_setzero_si128();
  const T *buffer_in;
  T *buffer_out;
  int lu, lv, wrap_in, mu, mv;
  int lx, ly, wrap_out;
  int out_x, out_y;
  double out_x_, out_y_;
  double out_u_, out_v_;
  int ref_u, ref_v;
  int pix_u, pix_v;
  double ref_out_u_, ref_out_v_;
  double ref_out_f_, ref_out_g_;
  int ref_out_f, ref_out_g;
  int pix_out_f, pix_out_g;
  int filter_mu, filter_mv;
  UINT inside_limit_u, inside_limit_v;
  int inside_nonempty;
  // double outside_min_u_,  outside_min_v_;
  // double outside_max_u_,  outside_max_v_;
  int outside_min_u, outside_min_v;
  int outside_max_u, outside_max_v;
  UCHAR *calc;
  int calc_allocsize;
  int calc_bytewrap;
  UCHAR calc_value;
  bool must_calc;
  T pix_value;
  T default_value(0, 0, 0, 0);
  float weight;
  float sum_weights;
  float inv_sum_weights;
  int i;

#ifdef USE_DOUBLE_TO_INT
  double d2iaux;
#endif

  T *pix_out;

  __m128 sum_contribs_packed;

  __m128i pix_value_packed_i;
  __m128 pix_value_packed;
  __m128 weight_packed;

  __m128 zeros2 = _mm_setzero_ps();

  float maxChannelValue        = (float)T::maxChannelValue;
  __m128 maxChanneValue_packed = _mm_load1_ps(&maxChannelValue);

  if (!(rout->getLx() > 0 && rout->getLy() > 0)) return;
  if (!(rin->getLx() > 0 && rin->getLy() > 0)) {
    resample_clear_rgbm(rout, default_value);
    return;
  }
  calc           = 0;
  calc_allocsize = 0;
  create_calc(rin, min_pix_ref_u, max_pix_ref_u, min_pix_ref_v, max_pix_ref_v,
              calc, calc_allocsize, calc_bytewrap);

  buffer_in  = rin->pixels();
  buffer_out = rout->pixels();
  lu         = rin->getLx();
  lx         = rout->getLx();
  lv         = rin->getLy();
  ly         = rout->getLy();
  wrap_in    = rin->getWrap();
  wrap_out   = rout->getWrap();
  mu         = lu - 1;
  mv         = lv - 1;

  filter_mu       = max_pix_ref_u - min_pix_ref_u;
  filter_mv       = max_pix_ref_v - min_pix_ref_v;
  inside_limit_u  = lu - filter_mu;
  inside_limit_v  = lv - filter_mv;
  inside_nonempty = (int)inside_limit_u > 0 && (int)inside_limit_v > 0;
  outside_min_u   = -max_pix_ref_u;
  outside_min_v   = -max_pix_ref_v;
  outside_max_u   = mu - min_pix_ref_u;
  outside_max_v   = mv - min_pix_ref_v;

  for (out_y = 0, out_y_ = 0.5; out_y < ly; out_y++, out_y_ += 1.0) {
    for (out_x = 0, out_x_ = 0.5; out_x < lx; out_x++, out_x_ += 1.0) {
      pix_out = buffer_out + out_y * wrap_out + out_x;

      out_u_ = affMV1(aff_xy2uv, out_x_, out_y_);
      out_v_ = affMV2(aff_xy2uv, out_x_, out_y_);
      ref_u  = intLE(out_u_);
      ref_v  = intLE(out_v_);

      if (inside_nonempty && (UINT)(ref_u + min_pix_ref_u) < inside_limit_u &&
          (UINT)(ref_v + min_pix_ref_v) < inside_limit_v) {
        calc_value = calc[(ref_u >> 3) + ref_v * calc_bytewrap];

        if (calc_value && ((calc_value >> (ref_u & 7)) & 1)) {
          ref_out_u_  = ref_u - out_u_;
          ref_out_v_  = ref_v - out_v_;
          ref_out_f_  = aff0MV1(aff0_uv2fg, ref_out_u_, ref_out_v_);
          ref_out_g_  = aff0MV2(aff0_uv2fg, ref_out_u_, ref_out_v_);
          ref_out_f   = tround(ref_out_f_);
          ref_out_g   = tround(ref_out_g_);
          sum_weights = 0;

          sum_contribs_packed = _mm_setzero_ps();

          for (i = n_pix - 1; i >= 0; i--) {
            pix_out_f = pix_ref_f[i] + ref_out_f;
            pix_out_g = pix_ref_g[i] + ref_out_g;
            weight    = (float)((filter[pix_out_f] * filter[pix_out_g]) >> 16);
            pix_u     = pix_ref_u[i] + ref_u;
            pix_v     = pix_ref_v[i] + ref_v;

            pix_value          = buffer_in[pix_u + pix_v * wrap_in];
            pix_value_packed_i = _mm_unpacklo_epi8(
                _mm_cvtsi32_si128(*(DWORD *)&pix_value), zeros);
            pix_value_packed =
                _mm_cvtepi32_ps(_mm_unpacklo_epi16(pix_value_packed_i, zeros));

            weight_packed = _mm_load1_ps(&weight);
            sum_contribs_packed =
                _mm_add_ps(sum_contribs_packed,
                           _mm_mul_ps(pix_value_packed, weight_packed));

            sum_weights += weight;
          }

          inv_sum_weights               = 1.0f / sum_weights;
          __m128 inv_sum_weights_packed = _mm_load1_ps(&inv_sum_weights);

          __m128 out_fval_packed =
              _mm_mul_ps(sum_contribs_packed, inv_sum_weights_packed);
          out_fval_packed = _mm_max_ps(out_fval_packed, zeros2);
          out_fval_packed = _mm_min_ps(out_fval_packed, maxChanneValue_packed);

          __m128i out_value_packed_i = _mm_cvtps_epi32(out_fval_packed);
          out_value_packed_i  = _mm_packs_epi32(out_value_packed_i, zeros);
          out_value_packed_i  = _mm_packus_epi16(out_value_packed_i, zeros);
          *(DWORD *)(pix_out) = _mm_cvtsi128_si32(out_value_packed_i);
        } else
          *pix_out = buffer_in[ref_u + ref_v * wrap_in];
      } else
          // if( outside_min_u_ <= out_u_ && out_u_ <= outside_max_u_ &&
          //    outside_min_v_ <= out_v_ && out_v_ <= outside_max_v_ )
          if (outside_min_u <= ref_u && ref_u <= outside_max_u &&
              outside_min_v <= ref_v && ref_v <= outside_max_v) {
        if ((UINT)ref_u >= (UINT)lu || (UINT)ref_v >= (UINT)lv)
          must_calc = true;
        else {
          calc_value = calc[(ref_u >> 3) + ref_v * calc_bytewrap];
          must_calc  = calc_value && ((calc_value >> (ref_u & 7)) & 1);
        }

        if (must_calc) {
          ref_out_u_          = ref_u - out_u_;
          ref_out_v_          = ref_v - out_v_;
          ref_out_f_          = aff0MV1(aff0_uv2fg, ref_out_u_, ref_out_v_);
          ref_out_g_          = aff0MV2(aff0_uv2fg, ref_out_u_, ref_out_v_);
          ref_out_f           = tround(ref_out_f_);
          ref_out_g           = tround(ref_out_g_);
          sum_weights         = 0;
          sum_contribs_packed = _mm_setzero_ps();

          for (i = n_pix - 1; i >= 0; i--) {
            pix_out_f = pix_ref_f[i] + ref_out_f;
            pix_out_g = pix_ref_g[i] + ref_out_g;
            weight    = (float)((filter[pix_out_f] * filter[pix_out_g]) >> 16);
            pix_u     = pix_ref_u[i] + ref_u;
            pix_v     = pix_ref_v[i] + ref_v;

            if (pix_u < 0 || pix_u > mu || pix_v < 0 || pix_v > mv) {
              sum_weights += weight;
              continue;
            }

            notLessThan(0, pix_u);
            notLessThan(0, pix_v);
            notMoreThan(mu, pix_u);
            notMoreThan(mv, pix_v);

            pix_value          = buffer_in[pix_u + pix_v * wrap_in];
            pix_value_packed_i = _mm_unpacklo_epi8(
                _mm_cvtsi32_si128(*(DWORD *)&pix_value), zeros);
            pix_value_packed =
                _mm_cvtepi32_ps(_mm_unpacklo_epi16(pix_value_packed_i, zeros));

            weight_packed = _mm_load1_ps(&weight);
            sum_contribs_packed =
                _mm_add_ps(sum_contribs_packed,
                           _mm_mul_ps(pix_value_packed, weight_packed));

            sum_weights += weight;
          }
          inv_sum_weights = 1.0f / sum_weights;

          __m128 inv_sum_weights_packed = _mm_load1_ps(&inv_sum_weights);
          __m128 out_fval_packed =
              _mm_mul_ps(sum_contribs_packed, inv_sum_weights_packed);
          out_fval_packed = _mm_max_ps(out_fval_packed, zeros2);
          out_fval_packed = _mm_min_ps(out_fval_packed, maxChanneValue_packed);

          __m128i out_value_packed_i = _mm_cvtps_epi32(out_fval_packed);
          out_value_packed_i  = _mm_packs_epi32(out_value_packed_i, zeros);
          out_value_packed_i  = _mm_packus_epi16(out_value_packed_i, zeros);
          *(DWORD *)(pix_out) = _mm_cvtsi128_si32(out_value_packed_i);
        } else
          *pix_out = buffer_in[ref_u + ref_v * wrap_in];
      } else {
        *pix_out = default_value;
      }
    }
  }
  if (calc) delete[] calc;
}

namespace {

//---------------------------------------------------------------------------

void inline blendBySSE2(TPixel32 *pix_out, float *ink, float *paint,
                        float *tone, const __m128 &maxtone_packed,
                        const __m128i &zeros) {
  __m128 a_packed = _mm_load_ps(ink);
  __m128 b_packed = _mm_load_ps(paint);

  __m128 num_packed  = _mm_load1_ps(tone);
  __m128 diff_packed = _mm_sub_ps(maxtone_packed, num_packed);

  // calcola in modo vettoriale out = ((den-num)*a + num*b)/den
  __m128 pix_value_packed = _mm_mul_ps(diff_packed, a_packed);
  __m128 tmpPix_packed    = _mm_mul_ps(num_packed, b_packed);

  pix_value_packed = _mm_add_ps(pix_value_packed, tmpPix_packed);
  pix_value_packed = _mm_div_ps(pix_value_packed, maxtone_packed);

  // converte i canali da float a char
  __m128i pix_value_packed_i = _mm_cvtps_epi32(pix_value_packed);
  pix_value_packed_i         = _mm_packs_epi32(pix_value_packed_i, zeros);
  pix_value_packed_i         = _mm_packus_epi16(pix_value_packed_i, zeros);

  *(DWORD *)(pix_out) = _mm_cvtsi128_si32(pix_value_packed_i);
}

//---------------------------------------------------------------------------

void inline blendBySSE2(__m128 &pix_out_packed, float *ink, float *paint,
                        float *tone, const __m128 &maxtone_packed,
                        const __m128i &zeros) {
  __m128 a_packed = _mm_load_ps(ink);
  __m128 b_packed = _mm_load_ps(paint);

  __m128 num_packed  = _mm_load1_ps(tone);
  __m128 diff_packed = _mm_sub_ps(maxtone_packed, num_packed);

  // calcola in modo vettoriale out = ((den-num)*a + num*b)/den
  pix_out_packed       = _mm_mul_ps(diff_packed, a_packed);
  __m128 tmpPix_packed = _mm_mul_ps(num_packed, b_packed);

  pix_out_packed = _mm_add_ps(pix_out_packed, tmpPix_packed);
  pix_out_packed = _mm_div_ps(pix_out_packed, maxtone_packed);
}

}  // namespace

#endif  // _WIN32
//---------------------------------------------------------------------------

static void get_prow_gr8(const TRasterGR8P &rin, double a11, double a12,
                         double a21, double a22, int pmin, int pmax, int q,
                         float *prow) {
  UCHAR *bufin_gr8, *in_gr8;
  int u, v;
  int p, p1, p2;
  UINT lu, lv;
  UINT mu, mv;
  int du, dv;
  double u_0, v_0;
  double u_, v_;
  double fu, fv;
  double gu, gv;

#ifdef USE_DOUBLE_TO_INT
  double d2iaux;
#endif

#ifdef BORDER
#undef BORDER
#endif
#define BORDER BORDER_GR8

  bufin_gr8 = (UCHAR *)rin->pixels();
  lu        = rin->getLx();
  mu        = lu - 1;
  lv        = rin->getLy();
  mv        = lv - 1;
  du        = 1;
  dv        = rin->getWrap();
  u_0       = a12 * q;
  v_0       = a22 * q;

  for (p = pmin; p <= pmax; p++)
    if (!prow[p]) {
      u_ = u_0 + a11 * p;
      u  = tfloor(u_);
      v_ = v_0 + a21 * p;
      v  = tfloor(v_);
      if ((UINT)u < mu && (UINT)v < mv) break;
      fu      = u_ - u;
      gu      = 1. - fu;
      fv      = v_ - v;
      gv      = 1. - fv;
      in_gr8  = bufin_gr8 + (u * du + v * dv);
      prow[p] = (float)troundp(
          fu * gv *
              (((UINT)(u + 1) < lu && (UINT)v < lv) ? in_gr8[du] : BORDER) +
          fu * fv * (((UINT)(u + 1) < lu && (UINT)(v + 1) < lv)
                         ? in_gr8[du + dv]
                         : BORDER) +
          gu * gv * (((UINT)u < lu && (UINT)v < lv) ? in_gr8[0] : BORDER) +
          gu * fv *
              (((UINT)u < lu && (UINT)(v + 1) < lv) ? in_gr8[dv] : BORDER));
    }
  p1 = p;
  for (p = pmax; p > p1; p--)
    if (!prow[p]) {
      u_ = u_0 + a11 * p;
      u  = tfloor(u_);
      v_ = v_0 + a21 * p;
      v  = tfloor(v_);
      if ((UINT)u < mu && (UINT)v < mv) break;
      fu      = u_ - u;
      gu      = 1. - fu;
      fv      = v_ - v;
      gv      = 1. - fv;
      in_gr8  = bufin_gr8 + (u * du + v * dv);
      prow[p] = (float)troundp(
          fu * gv *
              (((UINT)(u + 1) < lu && (UINT)v < lv) ? in_gr8[du] : BORDER) +
          fu * fv * (((UINT)(u + 1) < lu && (UINT)(v + 1) < lv)
                         ? in_gr8[du + dv]
                         : BORDER) +
          gu * gv * (((UINT)u < lu && (UINT)v < lv) ? in_gr8[0] : BORDER) +
          gu * fv *
              (((UINT)u < lu && (UINT)(v + 1) < lv) ? in_gr8[dv] : BORDER));
    }
  p2 = p;
  for (p = p1; p <= p2; p++)
    if (!prow[p]) {
      u_     = u_0 + a11 * p;
      u      = (int)(u_);
      v_     = v_0 + a21 * p;
      v      = (int)(v_);
      fu     = u_ - u;
      gu     = 1. - fu;
      fv     = v_ - v;
      gv     = 1. - fv;
      in_gr8 = bufin_gr8 + (u * du + v * dv);
      prow[p] =
          (float)troundp(fu * gv * in_gr8[du] + fu * fv * in_gr8[du + dv] +
                         gu * gv * in_gr8[0] + gu * fv * in_gr8[dv]);
    }
}

//---------------------------------------------------------------------------

#define grey(PIXEL) (TPixelGR8::from(PIXEL).value)

static void get_prow_gr8(const TRaster32P &rin, double a11, double a12,
                         double a21, double a22, int pmin, int pmax, int q,
                         float *prow) {
  TPixel *bufin_32, *in_32;
  int u, v;
  int p, p1, p2;
  UINT lu, lv;
  UINT mu, mv;
  int du, dv;
  double u_0, v_0;
  double u_, v_;
  double fu, fv;
  double gu, gv;

#ifdef USE_DOUBLE_TO_INT
  double d2iaux;
#endif

#ifdef BORDER
#undef BORDER
#endif
#define BORDER BORDER_GR8

  bufin_32 = (TPixel *)rin->pixels();
  lu       = rin->getLx();
  mu       = lu - 1;
  lv       = rin->getLy();
  mv       = lv - 1;
  du       = 1;
  dv       = rin->getWrap();
  u_0      = a12 * q;
  v_0      = a22 * q;

  for (p = pmin; p <= pmax; p++)
    if (!prow[p]) {
      u_ = u_0 + a11 * p;
      u  = tfloor(u_);
      v_ = v_0 + a21 * p;
      v  = tfloor(v_);
      if ((UINT)u < mu && (UINT)v < mv) break;
      fu      = u_ - u;
      gu      = 1. - fu;
      fv      = v_ - v;
      gv      = 1. - fv;
      in_32   = bufin_32 + (u * du + v * dv);
      prow[p] = (float)troundp(
          fu * gv * (((UINT)(u + 1) < lu && (UINT)v < lv) ? grey(in_32[du])
                                                          : BORDER) +
          fu * fv * (((UINT)(u + 1) < lu && (UINT)(v + 1) < lv)
                         ? grey(in_32[du + dv])
                         : BORDER) +
          gu * gv * (((UINT)u < lu && (UINT)v < lv) ? grey(in_32[0]) : BORDER) +
          gu * fv * (((UINT)u < lu && (UINT)(v + 1) < lv) ? grey(in_32[dv])
                                                          : BORDER));
    }
  p1 = p;
  for (p = pmax; p > p1; p--)
    if (!prow[p]) {
      u_ = u_0 + a11 * p;
      u  = tfloor(u_);
      v_ = v_0 + a21 * p;
      v  = tfloor(v_);
      if ((UINT)u < mu && (UINT)v < mv) break;
      fu      = u_ - u;
      gu      = 1. - fu;
      fv      = v_ - v;
      gv      = 1. - fv;
      in_32   = bufin_32 + (u * du + v * dv);
      prow[p] = (float)troundp(
          fu * gv * (((UINT)(u + 1) < lu && (UINT)v < lv) ? grey(in_32[du])
                                                          : BORDER) +
          fu * fv * (((UINT)(u + 1) < lu && (UINT)(v + 1) < lv)
                         ? grey(in_32[du + dv])
                         : BORDER) +
          gu * gv * (((UINT)u < lu && (UINT)v < lv) ? grey(in_32[0]) : BORDER) +
          gu * fv * (((UINT)u < lu && (UINT)(v + 1) < lv) ? grey(in_32[dv])
                                                          : BORDER));
    }
  p2 = p;
  for (p = p1; p <= p2; p++)
    if (!prow[p]) {
      u_      = u_0 + a11 * p;
      u       = (int)(u_);
      v_      = v_0 + a21 * p;
      v       = (int)(v_);
      fu      = u_ - u;
      gu      = 1. - fu;
      fv      = v_ - v;
      gv      = 1. - fv;
      in_32   = bufin_32 + (u * du + v * dv);
      prow[p] = (float)troundp(
          fu * gv * grey(in_32[du]) + fu * fv * grey(in_32[du + dv]) +
          gu * gv * grey(in_32[0]) + gu * fv * grey(in_32[dv]));
    }
}

//---------------------------------------------------------------------------
typedef float *MyFloatPtr;

static void rop_resample_gr8(const TRasterGR8P &rin, TRasterGR8P rout,
                             const TAffine &aff, const TAffine &invrot,
                             FILTER *rowflt, int pmin, int pmax, FILTER *colflt,
                             int qmin, int qmax, int nrows, int flatradu,
                             int flatradv, double flatradx_, double flatrady_,
                             NOCALC *rownoc, int nocdiamx, NOCALC *colnoc,
                             int nocdiamy) {
  FILTER *xflt, *yflt;
  UCHAR *bufin_gr8, *bufout_gr8, *in_gr8, *out_gr8;
  float *prow_base, *prow, **xrow_base, **xrow, *xxx, tmp;
  double x_, y_;
  int u, v;  //, vw;
  int p, q;
  int x, y;
  int lu, lv, mu, mv;
  int lx, ly, mx, my;
  // int dudp, dudq, dvdp, dvdq;
  int topq, topy;
  int wrapin, wrapout;
  int flatdiamu, flatdiamv;
  int xlo, xhi, ylo, yhi;
  int *nocheight;
  int nocwidth;
  int i, j;

#ifdef USE_DOUBLE_TO_INT
  double d2iaux;
#endif

  bufin_gr8  = (UCHAR *)rin->pixels();
  bufout_gr8 = (UCHAR *)rout->pixels();
  wrapin     = rin->getWrap();
  wrapout    = rout->getWrap();

  lu        = rin->getLx();
  mu        = lu - 1;
  lv        = rin->getLy();
  mv        = lv - 1;
  lx        = rout->getLx();
  mx        = lx - 1;
  ly        = rout->getLy();
  my        = ly - 1;
  prow_base = new float[pmax - pmin + 1];
  prow      = prow_base - pmin;
  xrow_base = new MyFloatPtr[qmax - (qmin - nrows) + 1];
  xrow      = xrow_base - (qmin - nrows);
  topq      = qmin;

  // app = xrow+topq-nrows;
  i = 0;
  j = 3;

  for (i = 0; i < nrows; i++) *(xrow + topq - nrows + i) = new float[lx];

  // while(app<xrow+topq)
  //  *app++ = new float[lx];

  flatdiamu = flatradu * 2 + 1;
  flatdiamv = flatradv * 2 + 1;

  for (y = 0; y < ly; y++)
    memset(bufout_gr8 + y * wrapout, 127, sizeof(UCHAR) * lx);

  int count = 0; /* !!!!!!!!!!!!!!!!*/

  int *colheight;
  UCHAR *colval;
  int flatcols;
  UCHAR flatval;
  double xlo_, xhi_, ylo_, yhi_;

  /*  TCALLOC (colheight, lu);
TCALLOC (colval,    lu);*/
  colheight = new int[lu];
  // memset(colheight, 0, lu);
  colval = new UCHAR[lu];
  // memset(colval, 0, lu);
  for (u = 0; u < (int)lu; u++) {
    colheight[u] = 0;
    colval[u]    = BORDER_GR8;
  }
  for (v = 0; v < (int)lv; v++) {
    x_ = affMV1(aff, 0 - flatradu, v - flatradv);
    y_ = affMV2(aff, 0 - flatradu, v - flatradv);

    xlo_ = tceil(x_ - flatradx_);
    xhi_ = tfloor(x_ + flatradx_);
    ylo_ = tceil(y_ - flatrady_);
    yhi_ = tfloor(y_ + flatrady_);

    flatcols = 0;
    flatval  = BORDER_GR8;
    for (u = 0, in_gr8 = bufin_gr8 + v * wrapin; u < (int)lu; u++, in_gr8++) {
      if (*in_gr8 == colval[u])
        colheight[u]++;
      else {
        colheight[u] = 1;
        colval[u]    = *in_gr8;
      }
      if (colheight[u] >= flatdiamv)
        if (colval[u] == flatval)
          flatcols++;
        else
          flatcols = 1;
      else
        flatcols = 0;
      flatval    = colval[u];
      if (flatcols >= flatdiamu) {
#ifdef VECCHIA_MANIERA
        x_  = AFF_M_V_1(aff, u - flatradu, v - flatradv);
        y_  = AFF_M_V_2(aff, u - flatradu, v - flatradv);
        xlo = CEIL(x_ - flatradx_);
        xhi = FLOOR(x_ + flatradx_);
        ylo = CEIL(y_ - flatrady_);
        yhi = FLOOR(y_ + flatrady_);
        NOT_LESS_THAN(0, xlo);
        NOT_MORE_THAN(mx, xhi);
        NOT_LESS_THAN(0, ylo);
        NOT_MORE_THAN(my, yhi);
#endif
        xlo = std::max(0, (int)xlo_);
        xhi = std::min(mx, (int)xhi_);
        ylo = std::max(0, (int)ylo_);
        yhi = std::min(my, (int)yhi_);
        for (y = ylo; y <= yhi; y++)
          for (x                        = xlo; x <= xhi; x++)
            bufout_gr8[x + y * wrapout] = flatval, count++;
      }
      xlo_ += aff.a11;
      xhi_ += aff.a11;
      ylo_ += aff.a21;
      yhi_ += aff.a21;
    }
  }
  delete[] colval;
  delete[] colheight;

  topy = 0;
  /*TCALLOC (nocheight, lx);*/
  nocheight = new int[lx];
  memset(nocheight, 0, lx * sizeof(int));
  out_gr8 = bufout_gr8;
  for (x = 0; x < lx; x++)
    if (out_gr8[x] != GREY_GR8)
      nocheight[x]++;
    else
      nocheight[x] = 0;

  for (y = 0, yflt = colflt; y < ly; y++, yflt++) {
    for (; topq <= yflt->last; topq++) {
      xrow[topq] = xrow[topq - nrows];
      xxx        = xrow[topq];
      memset(xxx, 0, sizeof(*xxx) * lx); /* 0.0 == nocalc */
      while (topy < ly - 1 && colnoc[topy].last < topq) {
        topy++;
        out_gr8 = bufout_gr8 + topy * wrapout;
        for (x = 0; x < lx; x++)
          if (out_gr8[x] != GREY_GR8)
            nocheight[x]++;
          else
            nocheight[x] = 0;
      }
      if (topy < ly && colnoc[topy].first <= topq) {
        for (x                                = 0; x < lx; x++)
          if (nocheight[x] < nocdiamy) xxx[x] = 1.0; /* 1.0 == calc */
      } else {
        for (x = 0; x < lx; x++) xxx[x] = 1.0; /* 1.0 == calc */
      }

      memset(prow + pmin, 0,
             sizeof(*prow) * (pmax - pmin + 1)); /* 0.0 == calc */
      nocwidth = 0;
      for (x = 0; x < lx; x++)
        if (xxx[x])
          nocwidth = 0;
        else {
          nocwidth++;
          if (nocwidth >= nocdiamx)
            for (p    = rownoc[x].first; p <= rownoc[x].last; p++)
              prow[p] = 1.0; /* 1.0 == nocalc */
        }
      get_prow_gr8(rin, invrot.a11, invrot.a12, invrot.a21, invrot.a22, pmin,
                   pmax, topq, prow);
      for (x = 0, xflt = rowflt; x < lx; x++, xflt++)
        if (xxx[x]) {
          for (tmp = 0.0, p = xflt->first; p <= xflt->last; p++)
            tmp += xflt->w[p] * prow[p];
          xxx[x] = tmp;
        }
    }
    out_gr8 = bufout_gr8 + wrapout * y;
    for (x = 0; x < lx; x++)
      if (out_gr8[x] == GREY_GR8) {
        for (tmp = 0.0, q = yflt->first; q <= yflt->last; q++)
          tmp += yflt->w[q] * xrow[q][x];
        out_gr8[x] = TO8BIT(tmp);
      }
  }
  // cest_plus_facile (xrow);

  for (q = 0; q < nrows; q++) delete xrow_base[q];
  delete xrow_base;
  delete prow_base;
}

//---------------------------------------------------------------------------

static void rop_resample_rgbm32_gr8(const TRaster32P &rin, TRasterGR8P rout,
                                    const TAffine &aff, const TAffine &invrot,
                                    FILTER *rowflt, int pmin, int pmax,
                                    FILTER *colflt, int qmin, int qmax,
                                    int nrows, int flatradu, int flatradv,
                                    double flatradx_, double flatrady_,
                                    NOCALC *rownoc, int nocdiamx,
                                    NOCALC *colnoc, int nocdiamy) {
  FILTER *xflt, *yflt;
  UCHAR *bufout_gr8, *out_gr8;
  TPixel *bufin_32, *in_32;
  float *prow_base, *prow, **xrow_base, **xrow, *xxx, tmp;
  double x_, y_;
  int u, v;  //, vw;
  int p, q;
  int x, y;
  int lu, lv, mu, mv;
  int lx, ly, mx, my;
  // int dudp, dudq, dvdp, dvdq;
  int topq, topy;
  int wrapin, wrapout;
  int flatdiamu, flatdiamv;
  int xlo, xhi, ylo, yhi;
  int *nocheight;
  int nocwidth;
  int i, j;

#ifdef USE_DOUBLE_TO_INT
  double d2iaux;
#endif

  bufin_32   = (TPixel *)rin->pixels();
  bufout_gr8 = (UCHAR *)rout->pixels();
  wrapin     = rin->getWrap();
  wrapout    = rout->getWrap();

  lu        = rin->getLx();
  mu        = lu - 1;
  lv        = rin->getLy();
  mv        = lv - 1;
  lx        = rout->getLx();
  mx        = lx - 1;
  ly        = rout->getLy();
  my        = ly - 1;
  prow_base = new float[pmax - pmin + 1];
  prow      = prow_base - pmin;
  xrow_base = new MyFloatPtr[qmax - (qmin - nrows) + 1];
  xrow      = xrow_base - (qmin - nrows);
  topq      = qmin;

  // app = xrow+topq-nrows;
  i = 0;
  j = 3;

  for (i = 0; i < nrows; i++) *(xrow + topq - nrows + i) = new float[lx];

  // while(app<xrow+topq)
  //  *app++ = new float[lx];

  flatdiamu = flatradu * 2 + 1;
  flatdiamv = flatradv * 2 + 1;

  for (y = 0; y < ly; y++)
    memset(bufout_gr8 + y * wrapout, 127, sizeof(UCHAR) * lx);

  int count = 0; /* !!!!!!!!!!!!!!!!*/

  int *colheight;
  UCHAR *colval;
  int flatcols;
  UCHAR flatval;
  double xlo_, xhi_, ylo_, yhi_;

  /*  TCALLOC (colheight, lu);
TCALLOC (colval,    lu);*/
  colheight = new int[lu];
  // memset(colheight, 0, lu);
  colval = new UCHAR[lu];
  // memset(colval, 0, lu);
  for (u = 0; u < (int)lu; u++) {
    colheight[u] = 0;
    colval[u]    = BORDER_GR8;
  }
  for (v = 0; v < (int)lv; v++) {
    x_ = affMV1(aff, 0 - flatradu, v - flatradv);
    y_ = affMV2(aff, 0 - flatradu, v - flatradv);

    xlo_ = tceil(x_ - flatradx_);
    xhi_ = tfloor(x_ + flatradx_);
    ylo_ = tceil(y_ - flatrady_);
    yhi_ = tfloor(y_ + flatrady_);

    flatcols = 0;
    flatval  = BORDER_GR8;
    for (u = 0, in_32 = bufin_32 + v * wrapin; u < (int)lu; u++, in_32++) {
      if (grey(*in_32) == colval[u])
        colheight[u]++;
      else {
        colheight[u] = 1;
        colval[u]    = grey(*in_32);
      }
      if (colheight[u] >= flatdiamv)
        if (colval[u] == flatval)
          flatcols++;
        else
          flatcols = 1;
      else
        flatcols = 0;
      flatval    = colval[u];
      if (flatcols >= flatdiamu) {
#ifdef VECCHIA_MANIERA
        x_  = AFF_M_V_1(aff, u - flatradu, v - flatradv);
        y_  = AFF_M_V_2(aff, u - flatradu, v - flatradv);
        xlo = CEIL(x_ - flatradx_);
        xhi = FLOOR(x_ + flatradx_);
        ylo = CEIL(y_ - flatrady_);
        yhi = FLOOR(y_ + flatrady_);
        NOT_LESS_THAN(0, xlo);
        NOT_MORE_THAN(mx, xhi);
        NOT_LESS_THAN(0, ylo);
        NOT_MORE_THAN(my, yhi);
#endif
        xlo = std::max(0, (int)xlo_);
        xhi = std::min(mx, (int)xhi_);
        ylo = std::max(0, (int)ylo_);
        yhi = std::min(my, (int)yhi_);
        for (y = ylo; y <= yhi; y++)
          for (x                        = xlo; x <= xhi; x++)
            bufout_gr8[x + y * wrapout] = flatval, count++;
      }
      xlo_ += aff.a11;
      xhi_ += aff.a11;
      ylo_ += aff.a21;
      yhi_ += aff.a21;
    }
  }
  delete[] colval;
  delete[] colheight;

  topy = 0;
  /*TCALLOC (nocheight, lx);*/
  nocheight = new int[lx];
  memset(nocheight, 0, lx * sizeof(int));
  out_gr8 = bufout_gr8;
  for (x = 0; x < lx; x++)
    if (out_gr8[x] != GREY_GR8)
      nocheight[x]++;
    else
      nocheight[x] = 0;

  for (y = 0, yflt = colflt; y < ly; y++, yflt++) {
    for (; topq <= yflt->last; topq++) {
      xrow[topq] = xrow[topq - nrows];
      xxx        = xrow[topq];
      memset(xxx, 0, sizeof(*xxx) * lx); /* 0.0 == nocalc */
      while (topy < ly - 1 && colnoc[topy].last < topq) {
        topy++;
        out_gr8 = bufout_gr8 + topy * wrapout;
        for (x = 0; x < lx; x++)
          if (out_gr8[x] != GREY_GR8)
            nocheight[x]++;
          else
            nocheight[x] = 0;
      }
      if (topy < ly && colnoc[topy].first <= topq) {
        for (x                                = 0; x < lx; x++)
          if (nocheight[x] < nocdiamy) xxx[x] = 1.0; /* 1.0 == calc */
      } else {
        for (x = 0; x < lx; x++) xxx[x] = 1.0; /* 1.0 == calc */
      }

      memset(prow + pmin, 0,
             sizeof(*prow) * (pmax - pmin + 1)); /* 0.0 == calc */
      nocwidth = 0;
      for (x = 0; x < lx; x++)
        if (xxx[x])
          nocwidth = 0;
        else {
          nocwidth++;
          if (nocwidth >= nocdiamx)
            for (p    = rownoc[x].first; p <= rownoc[x].last; p++)
              prow[p] = 1.0; /* 1.0 == nocalc */
        }
      get_prow_gr8(rin, invrot.a11, invrot.a12, invrot.a21, invrot.a22, pmin,
                   pmax, topq, prow);
      for (x = 0, xflt = rowflt; x < lx; x++, xflt++)
        if (xxx[x]) {
          for (tmp = 0.0, p = xflt->first; p <= xflt->last; p++)
            tmp += xflt->w[p] * prow[p];
          xxx[x] = tmp;
        }
    }
    out_gr8 = bufout_gr8 + wrapout * y;
    for (x = 0; x < lx; x++)
      if (out_gr8[x] == GREY_GR8) {
        for (tmp = 0.0, q = yflt->first; q <= yflt->last; q++)
          tmp += yflt->w[q] * xrow[q][x];
        out_gr8[x] = TO8BIT(tmp);
      }
  }
  // cest_plus_facile (xrow);

  for (q = 0; q < nrows; q++) delete xrow_base[q];
  delete xrow_base;
  delete prow_base;
}

//---------------------------------------------------------------------------

// #define USE_STATIC_VARS

//---------------------------------------------------------------------------

template <class T>
void rop_resample_rgbm(TRasterPT<T> rout, const TRasterPT<T> &rin,
                       const TAffine &aff, TRop::ResampleFilterType flt_type,
                       double blur) {
#define FILTER_RESOLUTION 1024
#define MAX_FILTER_VAL 32767

#ifdef USE_STATIC_VARS
  static TRop::ResampleFilterType current_flt_type = TRop::None;
  static std::unique_ptr<short[]> filter_array;
  static short *filter = 0;
  static int min_filter_fg, max_filter_fg;
  static int filter_array_size = 0;
  static int n_pix             = 0;
  static std::unique_ptr<int[]> pix_ref_u;
  static std::unique_ptr<int[]> pix_ref_v;
  static std::unique_ptr<int[]> pix_ref_f;
  static std::unique_ptr<int[]> pix_ref_g;
  static int current_max_n_pix = 0;
#else
  std::unique_ptr<short[]> filter_array;
  short *filter = 0;
  int min_filter_fg, max_filter_fg;
  int filter_array_size = 0;
  int n_pix             = 0;
  std::unique_ptr<int[]> pix_ref_u;
  std::unique_ptr<int[]> pix_ref_v;
  std::unique_ptr<int[]> pix_ref_f;
  std::unique_ptr<int[]> pix_ref_g;
  int current_max_n_pix = 0;
#endif
  int filter_st_radius;
  int filter_fg_radius;
  int filter_size;
  int f;
  double s_;
  double weight_;
  int weight;
  TAffine aff_uv2xy;
  TAffine aff_xy2uv;
  TAffine aff0_uv2xy;
  TAffine aff0_xy2st;
  TAffine aff0_uv2st;
  TAffine aff0_st2fg;
  TAffine aff0_uv2fg;
  TAffine aff0_fg2uv;
  double scale_x, scale_y;
  double inv_blur;
  int max_n_pix;
  double min_pix_out_u_, min_pix_out_v_;
  double max_pix_out_u_, max_pix_out_v_;
  int min_pix_ref_u, min_pix_ref_v;
  int max_pix_ref_u, max_pix_ref_v;
  int cur_pix_ref_u, cur_pix_ref_v;
  double cur_pix_ref_f_, cur_pix_ref_g_;
  int cur_pix_ref_f, cur_pix_ref_g;
  double min_ref_out_f_, min_ref_out_g_;
  double max_ref_out_f_, max_ref_out_g_;
  int min_ref_out_f, min_ref_out_g;
  int max_ref_out_f, max_ref_out_g;
  int min_pix_ref_f, min_pix_ref_g;
  int max_pix_ref_f, max_pix_ref_g;
  int min_pix_out_f, min_pix_out_g;
  int max_pix_out_f, max_pix_out_g;
  int min_pix_out_fg;
  int max_pix_out_fg;

#ifdef USE_DOUBLE_TO_INT
  double d2iaux;
#endif

  assert(flt_type != TRop::None);

  // Retrieve the filter radius in the st and fx references
  filter_st_radius = get_filter_radius(flt_type);
  filter_fg_radius = filter_st_radius * FILTER_RESOLUTION;

  // Retrieve the transformation affines among the involved references
  // NOTE: The 0.5 translation is needed in order to make the later
  // resample_main procedures work with pixel centers.
  aff_uv2xy  = aff * TTranslation(0.5, 0.5);
  aff0_uv2xy = aff_uv2xy.place(0.0, 0.0, 0.0, 0.0);
  aff_xy2uv  = aff_uv2xy.inv();

  // Consider the norm of (1,0) and (0,1) images.
  scale_x = sqrt(sq(aff_uv2xy.a11) + sq(aff_uv2xy.a12));
  scale_y = sqrt(sq(aff_uv2xy.a21) + sq(aff_uv2xy.a22));

  // Inserting the following scale will make shrinks look smooth.
  aff0_xy2st = TScale((scale_x > 1.0) ? 1.0 / scale_x : 1.0,
                      (scale_y > 1.0) ? 1.0 / scale_y : 1.0);

  if (blur > 1.0)  // Consider the blur as a scale in the filter reference
  {
    inv_blur   = 1.0 / blur;
    aff0_xy2st = TScale(inv_blur, inv_blur) * aff0_xy2st;
  }

  aff0_uv2st = aff0_xy2st * aff0_uv2xy;
  aff0_st2fg = TScale(FILTER_RESOLUTION, FILTER_RESOLUTION);
  aff0_uv2fg = aff0_st2fg * aff0_uv2st;
  aff0_fg2uv = aff0_uv2fg.inv();

  // Take the pre-image of the filter mask in uv coordinates. This is where
  // input pixels will be taken to find an output one.
  minmax(-filter_fg_radius, -filter_fg_radius, filter_fg_radius,
         filter_fg_radius, aff0_fg2uv, min_pix_out_u_, min_pix_out_v_,
         max_pix_out_u_, max_pix_out_v_);

  // Adjust them to integer coordinates. The intent here is that
  // of isolating their fractionary part - furthermore, we'll take
  // the *opposites* of fractionary parts (explained later).
  // NOTE: We'll assume we want to include in the filter mask all
  //*integer positions around a fractionary displacement of the origin*;
  // so the approximations below are stricly necessary.

  min_pix_ref_u = intLE(min_pix_out_u_);
  min_pix_ref_v = intLE(min_pix_out_v_);
  max_pix_ref_u = intGE(max_pix_out_u_);
  max_pix_ref_v = intGE(max_pix_out_v_);

  if (blur <= 1.0) {
    // If the blur radius has sub-pixel width
    if (aff_uv2xy.a12 == 0.0 && aff_uv2xy.a21 == 0.0) {
      // And it's the sole scales case
      if (aff_uv2xy.a11 == 1.0 && isInt(aff_uv2xy.a13 - 0.5)) {
        // And the x mapping is bijective, then prevent any filtering.
        min_pix_ref_u = 0;
        max_pix_ref_u = 0;
      }
      if (aff_uv2xy.a22 == 1.0 && isInt(aff_uv2xy.a23 - 0.5)) {
        // And the y mapping is bijective ...
        min_pix_ref_v = 0;
        max_pix_ref_v = 0;
      }
    } else if (aff_uv2xy.a11 == 0.0 && aff_uv2xy.a22 == 0.0) {
      // The mirrored version of the one above
      if (aff_uv2xy.a12 == 1.0 && isInt(aff_uv2xy.a13 - 0.5)) {
        min_pix_ref_v = 0;
        max_pix_ref_v = 0;
      }
      if (aff_uv2xy.a21 == 1.0 && isInt(aff_uv2xy.a23 - 0.5)) {
        min_pix_ref_u = 0;
        max_pix_ref_u = 0;
      }
    }
  }

  // Take the number of pixels involved in the filter (uv reference)
  max_n_pix =
      (max_pix_ref_u - min_pix_ref_u + 1) * (max_pix_ref_v - min_pix_ref_v + 1);

  if (max_n_pix > current_max_n_pix) {
    current_max_n_pix = max_n_pix;
    pix_ref_u.reset(new int[current_max_n_pix]);
    pix_ref_v.reset(new int[current_max_n_pix]);
    pix_ref_f.reset(new int[current_max_n_pix]);
    pix_ref_g.reset(new int[current_max_n_pix]);
    assert(pix_ref_u && pix_ref_v && pix_ref_f && pix_ref_g);
  }

  // Build the image of fractionary domain from the uv to fg reference
  minmax(-1, -1, 0, 0, aff0_uv2fg, min_ref_out_f_, min_ref_out_g_,
         max_ref_out_f_, max_ref_out_g_);

  min_ref_out_f = tround(min_ref_out_f_);
  min_ref_out_g = tround(min_ref_out_g_);
  max_ref_out_f = tround(max_ref_out_f_);
  max_ref_out_g = tround(max_ref_out_g_);

  // Remember that negative fractionary parts must be subtracted from their
  // integer counterparts
  min_pix_ref_f = -filter_fg_radius - max_ref_out_f;
  min_pix_ref_g = -filter_fg_radius - max_ref_out_g;
  max_pix_ref_f = filter_fg_radius - min_ref_out_f;
  max_pix_ref_g = filter_fg_radius - min_ref_out_g;

  min_pix_out_f = c_maxint;
  min_pix_out_g = c_maxint;
  max_pix_out_f = c_minint;
  max_pix_out_g = c_minint;
  n_pix         = 0;

  if (!pix_ref_u || !pix_ref_v || !pix_ref_f || !pix_ref_g) {
    throw TRopException(
        "tresample.cpp line2640  function rop_resample_rgbm() : alloc pix_ref "
        "failed");
  }

  // Build the *integer* part of the fg images of those coordinates inside the
  // uv filter bounds.

  // NOTE: Doing so reduces the execution time for the later resample_main
  // procedure -
  // the idea is the following:
  //  We want to build the output pixel (x,y) obtained from the source image
  //  through A.
  //  Then, we find (u,v) = (A^-1) * (x,y) = ([u],[v]) + ({u},{v}), where [] and
  //  {}
  //  denote integer and fractionary parts.
  //  Now, the convolution positions on fg for (u,v) can be thought of being
  //  calculated by taking
  //  images of integer displacements of (u,v). So, their calculation is
  //  definitely *not* directly
  //  dependent on the fractionary part of (u,v) - that is, the (i,j)th
  //  displacement position of FG(u,v)
  //  is:
  //          FG([u]+i,[v]+j) = FG(u+i,v+j) - FG({u},{v}) = FG(i,j) -
  //          FG({u},{v});
  //
  //  where it is assumed that FG(u,v) = (0,0), since the filter is to be
  //  considered centered on (u,v).

  for (cur_pix_ref_v = min_pix_ref_v; cur_pix_ref_v <= max_pix_ref_v;
       cur_pix_ref_v++)
    for (cur_pix_ref_u = min_pix_ref_u; cur_pix_ref_u <= max_pix_ref_u;
         cur_pix_ref_u++) {
      // Get the image of current uv position
      cur_pix_ref_f_ = affMV1(aff0_uv2fg, cur_pix_ref_u, cur_pix_ref_v);
      cur_pix_ref_g_ = affMV2(aff0_uv2fg, cur_pix_ref_u, cur_pix_ref_v);

      // And round it to the closest integer in fg
      cur_pix_ref_f = tround(cur_pix_ref_f_);
      cur_pix_ref_g = tround(cur_pix_ref_g_);
      if (min_pix_ref_f <= cur_pix_ref_f && cur_pix_ref_f <= max_pix_ref_f &&
          min_pix_ref_g <= cur_pix_ref_g && cur_pix_ref_g <= max_pix_ref_g) {
        pix_ref_u[n_pix] = cur_pix_ref_u;
        pix_ref_v[n_pix] = cur_pix_ref_v;
        pix_ref_f[n_pix] = cur_pix_ref_f;
        pix_ref_g[n_pix] = cur_pix_ref_g;
        notMoreThan(cur_pix_ref_f + min_ref_out_f,
                    min_pix_out_f);  // cur_pix_ref > min_pix_out - min_ref_out
        notMoreThan(cur_pix_ref_g + min_ref_out_g, min_pix_out_g);
        notLessThan(cur_pix_ref_f + max_ref_out_f,
                    max_pix_out_f);  // cur_pix_ref < max_pix_out - max_ref_out
        notLessThan(cur_pix_ref_g + max_ref_out_g, max_pix_out_g);
        n_pix++;
      }
    }
  assert(n_pix > 0);

#ifdef USE_STATIC_VARS
  if (flt_type != current_flt_type) {
    current_flt_type = flt_type;
#endif

    // Build a sufficient filter weights array
    min_filter_fg = -filter_fg_radius - FILTER_RESOLUTION * 3 / 2;  //???
    max_filter_fg = filter_fg_radius + FILTER_RESOLUTION * 3 / 2;
    filter_size   = max_filter_fg - min_filter_fg + 1;
    if (filter_size > filter_array_size)  // For the static vars case...
    {
      filter_array.reset(new short[filter_size]);
      assert(filter_array);
      filter_array_size = filter_size;
    }
    filter = filter_array.get() - min_filter_fg;  // Take the position
                                                  // corresponding to fg's (0,0)
                                                  // in the array
    filter[0] = MAX_FILTER_VAL;
    for (f = 1, s_ = 1.0 / FILTER_RESOLUTION; f < filter_fg_radius;
         f++, s_ += 1.0 / FILTER_RESOLUTION) {
      // Symmetrically build the array
      weight_    = get_filter_value(flt_type, s_) * (double)MAX_FILTER_VAL;
      weight     = tround(weight_);
      filter[f]  = weight;
      filter[-f] = weight;
    }
    for (f = filter_fg_radius; f <= max_filter_fg; f++) filter[f] = 0;
    for (f = -filter_fg_radius; f >= min_filter_fg; f--) filter[f] = 0;

#ifdef USE_STATIC_VARS
  }
#endif

  // Considering the bounding square in fg
  min_pix_out_fg = std::min(min_pix_out_f, min_pix_out_g);
  max_pix_out_fg = std::max(max_pix_out_f, max_pix_out_g);
  if (min_pix_out_fg < min_filter_fg || max_pix_out_fg > max_filter_fg) {
    // Reallocate the filter... and so on...
    filter_size = max_pix_out_fg - min_pix_out_fg + 1;
    if (filter_size > filter_array_size) {
      // controllare!!
      // TREALLOC (filter_array, filter_size)
      filter_array.reset(new short[filter_size]);

      assert(filter_array);
      filter_array_size = filter_size;
    }
    filter = filter_array.get() - min_filter_fg;
    if (min_pix_out_fg < min_filter_fg) {
      int delta = min_filter_fg - min_pix_out_fg;

      for (f              = max_filter_fg; f >= min_filter_fg; f--)
        filter[f + delta] = filter[f];
      filter += delta;
      for (f = min_filter_fg - 1; f >= min_pix_out_fg; f--) filter[f] = 0;
      min_filter_fg = min_pix_out_fg;
    }
    if (max_pix_out_fg > max_filter_fg) {
      for (f = max_filter_fg + 1; f <= max_pix_out_fg; f++) filter[f] = 0;
      max_filter_fg = max_pix_out_fg;
    }
  }

#ifdef _WIN32
  if ((TSystem::getCPUExtensions() & TSystem::CpuSupportsSse2) &&
      T::maxChannelValue == 255)
    resample_main_rgbm_SSE2<T>(rout, rin, aff_xy2uv, aff0_uv2fg, min_pix_ref_u,
                               min_pix_ref_v, max_pix_ref_u, max_pix_ref_v,
                               n_pix, pix_ref_u.get(), pix_ref_v.get(),
                               pix_ref_f.get(), pix_ref_g.get(), filter);
  else
#endif
      if (n_pix >= 512 || T::maxChannelValue > 255)
    resample_main_rgbm<T, TINT64>(
        rout, rin, aff_xy2uv, aff0_uv2fg, min_pix_ref_u, min_pix_ref_v,
        max_pix_ref_u, max_pix_ref_v, n_pix, pix_ref_u.get(), pix_ref_v.get(),
        pix_ref_f.get(), pix_ref_g.get(), filter);
  else
    resample_main_rgbm<T, TINT32>(
        rout, rin, aff_xy2uv, aff0_uv2fg, min_pix_ref_u, min_pix_ref_v,
        max_pix_ref_u, max_pix_ref_v, n_pix, pix_ref_u.get(), pix_ref_v.get(),
        pix_ref_f.get(), pix_ref_g.get(), filter);
}

//---------------------------------------------------------------------------

static void free_filter(FILTER *filter, int lx) {
  for (--lx; lx >= 0; lx--)
    if (filter[lx].w_base) delete (filter[lx].w_base);
  delete[] filter;
}

//-----------------------------------------------------------------------------

void do_resample(TRasterGR8P rout, const TRasterGR8P &rin, const TAffine &aff,
                 TRop::ResampleFilterType flt_type, double blur) {
  double jacob;
  double s11, s22, s13, s23;
  FILTER *rowf, *colf;
  NOCALC *rown, *coln;
  int pmin, pmax, qmin, qmax;
  int nrows, dummy;
  double negradu_, negradv_, posradu_, posradv_;
  double negradx_, negrady_, posradx_, posrady_;
  int nocdiamx, nocdiamy;
  double rad_x, rad_y;
  TAffine rot, scale, invrot;

#ifdef USE_DOUBLE_TO_INT
  double d2iaux;
#endif

  if (!(rout->getLx() > 0 && rout->getLy() > 0)) /* immagine out vuota */
  {
    return;
  }
  if (!(rin->getLx() > 0 && rin->getLy() > 0)) /* immagine in vuota */
  {
    rout->fill(TPixelGR8::Black);  // Black_rgbm
    return;
  }

  TRasterGR8P routGR8 = rout, rinGR8 = rin;
  if (routGR8 && rinGR8) {
    jacob = fabs(aff.det());
    if (jacob == 0.0)
      throw TRopException("AFFINE transformation has zero determinant");
    if (jacob < 1E-30)
      throw TRopException(
          "AFFINE transformation has (nearly) zero determinant");
    s11 = sqrt(jacob); /* provvisorio */
    s22 = s11;
    s13 = aff.a13;
    s23 = aff.a23;

    // rot = aff_place (0.0, 0.0, 0.0, 0.0, TScale(1/s11,
    // 1/s22)*aff);//eventualmente invertire ordine

    rot = (TScale(1 / s11, 1 / s22) * aff).place(0.0, 0.0, 0.0, 0.0);

    // scale = aff_place (0.0, 0.0, s13, s23, TScale(s11, s22));
    scale  = TScale(s11, s22).place(0.0, 0.0, s13, s23);
    invrot = rot.inv();

    rowf = create_filter(flt_type, blur, scale.a11, scale.a13, rout->getLx(),
                         rad_x, pmin, pmax, dummy);
    colf = create_filter(flt_type, blur, scale.a22, scale.a23, rout->getLy(),
                         rad_y, qmin, qmax, nrows);
    rown = create_nocalc(flt_type, blur, scale.a11, scale.a13, rout->getLx(),
                         pmin, pmax, nocdiamx);
    coln = create_nocalc(flt_type, blur, scale.a22, scale.a23, rout->getLy(),
                         qmin, qmax, nocdiamy);

#ifdef DBMALLOC
    malloc_chain_check(TRUE);
#endif
#ifdef MEMLEAK
    CheckMemory();
#endif

    TAffine aff_0 = aff.place(0.0, 0.0, 0.0, 0.0);
    TAffine inv_0 = aff_0.inv();

    minmax(-0.5, -0.5, 0.5, 0.5, aff_0, negradx_, negrady_, posradx_, posrady_);
    double flatradx_ = posradx_;
    double flatrady_ = posrady_;
    minmax(negradx_ - rad_x, negrady_ - rad_y, posradx_ + rad_x,
           posrady_ + rad_y, inv_0, negradu_, negradv_, posradu_, posradv_);

    int flatradu = tceil(posradu_) - 1;
    int flatradv = tceil(posradv_) - 1;

    rop_resample_gr8(rin, rout, aff, invrot, rowf, pmin, pmax, colf, qmin, qmax,
                     nrows, flatradu, flatradv, flatradx_, flatrady_, rown,
                     nocdiamx, coln, nocdiamy);

    // free_nocalc (coln);
    if (coln) delete (coln);
    // free_nocalc (rown);
    if (rown) delete (rown);
    free_filter(colf, rout->getLy());
    free_filter(rowf, rout->getLx());
    //----NON GESTIAMO ANCORA EXTRA BUFFER
    // rop_resample_extra (rin, rout, aff);

    return;
  } else
    throw TRopException("unsupported pixel type");
}

//-----------------------------------------------------------------------------

void do_resample(TRasterGR8P rout, const TRaster32P &rin, const TAffine &aff,
                 TRop::ResampleFilterType flt_type, double blur) {
  double jacob;
  double s11, s22, s13, s23;
  FILTER *rowf, *colf;
  NOCALC *rown, *coln;
  int pmin, pmax, qmin, qmax;
  int nrows, dummy;
  double negradu_, negradv_, posradu_, posradv_;
  double negradx_, negrady_, posradx_, posrady_;
  int nocdiamx, nocdiamy;
  double rad_x, rad_y;
  TAffine rot, scale, invrot;

#ifdef USE_DOUBLE_TO_INT
  double d2iaux;
#endif

  if (!(rout->getLx() > 0 && rout->getLy() > 0)) /* immagine out vuota */
  {
    return;
  }
  if (!(rin->getLx() > 0 && rin->getLy() > 0)) /* immagine in vuota */
  {
    rout->fill(TPixelGR8::Black);  // Black_rgbm
    return;
  }

  jacob = fabs(aff.det());
  if (jacob == 0.0)
    throw TRopException("AFFINE transformation has zero determinant");
  if (jacob < 1E-30)
    throw TRopException("AFFINE transformation has (nearly) zero determinant");
  s11 = sqrt(jacob); /* provvisorio */
  s22 = s11;
  s13 = aff.a13;
  s23 = aff.a23;

  // rot = aff_place (0.0, 0.0, 0.0, 0.0, TScale(1/s11,
  // 1/s22)*aff);//eventualmente invertire ordine

  rot = (TScale(1 / s11, 1 / s22) * aff).place(0.0, 0.0, 0.0, 0.0);

  // scale = aff_place (0.0, 0.0, s13, s23, TScale(s11, s22));
  scale  = TScale(s11, s22).place(0.0, 0.0, s13, s23);
  invrot = rot.inv();

  rowf = create_filter(flt_type, blur, scale.a11, scale.a13, rout->getLx(),
                       rad_x, pmin, pmax, dummy);
  colf = create_filter(flt_type, blur, scale.a22, scale.a23, rout->getLy(),
                       rad_y, qmin, qmax, nrows);
  rown = create_nocalc(flt_type, blur, scale.a11, scale.a13, rout->getLx(),
                       pmin, pmax, nocdiamx);
  coln = create_nocalc(flt_type, blur, scale.a22, scale.a23, rout->getLy(),
                       qmin, qmax, nocdiamy);

#ifdef DBMALLOC
  malloc_chain_check(TRUE);
#endif
#ifdef MEMLEAK
  CheckMemory();
#endif

  TAffine aff_0 = aff.place(0.0, 0.0, 0.0, 0.0);
  TAffine inv_0 = aff_0.inv();

  minmax(-0.5, -0.5, 0.5, 0.5, aff_0, negradx_, negrady_, posradx_, posrady_);
  double flatradx_ = posradx_;
  double flatrady_ = posrady_;
  minmax(negradx_ - rad_x, negrady_ - rad_y, posradx_ + rad_x, posrady_ + rad_y,
         inv_0, negradu_, negradv_, posradu_, posradv_);

  int flatradu = tceil(posradu_) - 1;
  int flatradv = tceil(posradv_) - 1;

  rop_resample_rgbm32_gr8(rin, rout, aff, invrot, rowf, pmin, pmax, colf, qmin,
                          qmax, nrows, flatradu, flatradv, flatradx_, flatrady_,
                          rown, nocdiamx, coln, nocdiamy);

  // free_nocalc (coln);
  if (coln) delete[] coln;
  // free_nocalc (rown);
  if (rown) delete[] rown;
  free_filter(colf, rout->getLy());
  free_filter(rowf, rout->getLx());
  //----NON GESTIAMO ANCORA EXTRA BUFFER
  // rop_resample_extra (rin, rout, aff);

  return;

  // else throw TRopException("unsupported pixel type");
}

//-----------------------------------------------------------------------------

template <class T>
void do_resample(TRasterPT<T> rout, const TRasterPT<T> &rin, const TAffine &aff,
                 TRop::ResampleFilterType flt_type, double blur)

{
#ifdef ALTRI_TIPI_DI_RASTER
  double jacob;
  double s11, s22, s13, s23;
  FILTER *rowf, *colf;
  NOCALC *rown, *coln;
  int pmin, pmax, qmin, qmax;
  int nrows, dummy;
  double negradu_, negradv_, posradu_, posradv_;
  double negradx_, negrady_, posradx_, posrady_;
  int nocdiamx, nocdiamy;
  double rad_x, rad_y;

#endif

  if (!(rout->getLx() > 0 && rout->getLy() > 0)) /* immagine out vuota */
  {
    return;
  }
  if (!(rin->getLx() > 0 && rin->getLy() > 0)) /* immagine in vuota */
  {
    rout->fill(T::Black);  // Black_rgbm
    return;
  }

  TRasterPT<T> rout_ = rout, rin_ = rin;
  if (rout_ && rin_) {
    rop_resample_rgbm<T>(rout, rin, aff, flt_type, blur);
    return;
  } else
    throw TRopException("unsupported pixel type");

#ifdef ALTRI_TIPI_DI_RASTER
  jacob = fabs(aff.det());
  if (jacob == 0.0)
    throw TRopException("AFFINE transformation has zero determinant");
  if (jacob < 1E-30)
    throw TRopException("AFFINE transformation has (nearly) zero determinant");
  s11 = sqrt(jacob); /* provvisorio */
  s22 = s11;
  s13 = aff.a13;
  s23 = aff.a23;

  rot = (TScale(1 / s11, 1 / s22) * aff).place(0.0, 0.0, 0.0, 0.0);

  scale  = TScale(s11, s22).place(0.0, 0.0, s13, s23);
  invrot = rot.inv();

  rowf = create_filter(flt_type, blur, scale.a11, scale.a13, rout->getLx(),
                       rad_x, pmin, pmax, dummy);
  colf = create_filter(flt_type, blur, scale.a22, scale.a23, rout->getLy(),
                       rad_y, qmin, qmax, nrows);
  rown = create_nocalc(flt_type, blur, scale.a11, scale.a13, rout->getLx(),
                       pmin, pmax, nocdiamx);
  coln = create_nocalc(flt_type, blur, scale.a22, scale.a23, rout->getLy(),
                       qmin, qmax, nocdiamy);

#ifdef DBMALLOC
  malloc_chain_check(TRUE);
#endif
#ifdef MEMLEAK
  CheckMemory();
#endif

  aff_0 = aff.place(0.0, 0.0, 0.0, 0.0);
  inv_0 = aff_0.inv();

  minmax(-0.5, -0.5, 0.5, 0.5, aff_0, negradx_, negrady_, posradx_, posrady_);
  minmax(negradx_ - rad_x, negrady_ - rad_y, posradx_ + rad_x, posrady_ + rad_y,
         inv_0, negradu_, negradv_, posradu_, posradv_);

  if (coln) delete (coln);
  if (rown) delete (rown);
  free_filter(colf, rout->getLy());
  free_filter(rowf, rout->getLx());
#endif
}

//-----------------------------------------------------------------------------

typedef struct {
  TUINT32 val;
  double tot;
} BLOB24;

//-----------------------------------------------------------------------------

#define MINOREQ(x, a) ((x) >= 0 && (x) <= (a))
#define MINOR(x, a) ((x) >= 0 && (x) < (a))

//-----------------------------------------------------------------------------
}  // namespace

#ifndef TNZCORE_LIGHT

namespace {
void do_resample(TRasterCM32P rout, const TRasterCM32P &rin,
                 const TAffine &aff) {
  TAffine inv;
  int lx, ly, mx, my;
  int lu, lv, mu, mv;
  int x, y, u, v;
  double u_0, v_0, u_, v_;
  double fu, fv, gu, gv;
  int i, j;
  int wrapin, wrapout;
  TUINT32 *bufin_tcm, *bufout_tcm;
  TUINT32 *in_tcm, *out_tcm;
  TUINT32 tcm[4];
  double w[4];
  TUINT32 transp;
  BLOB24 color_blob[4], new_color_blob;
  BLOB24 pencil_blob[4], new_pencil_blob;
  int color_blobs;
  int pencil_blobs;
  bool some_pencil;
  double tone_tot;
  TUINT32 color_mask, pencil_mask;
  TUINT32 tone_mask;
  int tone;

#ifdef USE_DOUBLE_TO_INT
  double d2iaux;
#endif

  if (!(rout->getLx() > 0 && rout->getLy() > 0)) /* immagine out vuota */
    return;

  rout->lock();

  if (!(rin->getLx() > 0 && rin->getLy() > 0)) /* immagine in vuota */
  {
    for (y = 0; y < rout->getLy(); y++)
      for (x = 0; x < rout->getLx(); x++)
        ((TUINT32 *)rout->getRawData())[x + y * rout->getWrap()] = 0xff;
    rout->unlock();
    return;
  }

  rin->lock();

  bufin_tcm  = (TUINT32 *)rin->getRawData();
  bufout_tcm = (TUINT32 *)rout->getRawData();
  wrapin     = rin->getWrap();
  wrapout    = rout->getWrap();
  lu         = rin->getLx();
  mu         = lu - 1;
  lv         = rin->getLy();
  mv         = lv - 1;
  lx         = rout->getLx();
  mx         = lx - 1;
  ly         = rout->getLy();
  my         = ly - 1;
  inv        = aff.inv();

  pencil_mask = TPixelCM32::getInkMask();
  color_mask  = TPixelCM32::getPaintMask();
  tone_mask   = TPixelCM32::getToneMask();

  transp = tone_mask;

  assert(tone_mask &
         0x1);  // Ensure that tone lies in the less significative bits

  // deal with every output line independently
  for (y = 0; y < ly; y++) {
    // Take inv*(0,y)
    u_0 = inv.a12 * y + inv.a13;
    v_0 = inv.a22 * y + inv.a23;

    out_tcm = bufout_tcm + wrapout * y;
    x       = 0;

    // Place transparent pixels until we reach a useful source pos.
    for (; x < lx; x++) {
      // Add inv*(x,0) and floor it
      u_ = u_0 + x * inv.a11;
      u  = tfloor(u_);
      v_ = v_0 + x * inv.a21;
      v  = tfloor(v_);
      if (MINOREQ(u + 1, lu) &&
          MINOREQ(v + 1, lv))  // u>=-1 && u<lu && v>=-1 && v<lv
        break;                 // Goto next for
      *out_tcm++ = transp;     // Otherwise, place a transparent pixel
    }

    // Deal with leftwise border pre-images
    for (; x < lx; x++) {
      u_ = u_0 + x * inv.a11;
      u  = tfloor(u_);
      v_ = v_0 + x * inv.a21;
      v  = tfloor(v_);
      if (MINOR(u, lu) && MINOR(v, lv))  // u>=0 && u<lu && v>=0 && v<lv
        break;
      in_tcm  = bufin_tcm + u + v * wrapin;
      bool u0 = MINOREQ(u, mu);
      bool v0 = MINOREQ(v, mv);
      bool u1 = MINOREQ(u + 1, mv);
      bool v1 = MINOREQ(v + 1, mv);
      tcm[0]  = (u0 && v0) ? in_tcm[0] : transp;
      tcm[1]  = (u1 && v0) ? in_tcm[1] : transp;
      tcm[2]  = (u0 && v1) ? in_tcm[wrapin] : transp;
      tcm[3]  = (u1 && v1) ? in_tcm[wrapin + 1] : transp;

      if (tcm[0] == tcm[1] && tcm[1] == tcm[2] && tcm[2] == tcm[3])
        *out_tcm++ = tcm[0];
      else {
        fu          = u_ - u;
        gu          = 1. - fu;
        fv          = v_ - v;
        gv          = 1. - fv;
        w[0]        = gu * gv;
        w[2]        = gu * fv;
        w[1]        = fu * gv;
        w[3]        = fu * fv;
        color_blobs = pencil_blobs = 0;
        tone_tot                   = 0.0;
        some_pencil                = false;
        for (i = 0; i < 4; i++) {
          tone                                        = tcm[i] & tone_mask;
          if ((TUINT32)tone != tone_mask) some_pencil = true;
          tone_tot += tone * w[i];
          new_color_blob.val = tcm[i] & color_mask;
          new_color_blob.tot = tone * w[i];
          for (j = 0; j < color_blobs; j++)
            if (color_blob[j].val == new_color_blob.val) break;
          if (j < color_blobs)
            color_blob[j].tot += new_color_blob.tot;
          else
            color_blob[color_blobs++] = new_color_blob;
          for (; j > 0 && color_blob[j].tot > color_blob[j - 1].tot; j--)
            std::swap(color_blob[j], color_blob[j - 1]);
          new_pencil_blob.val = tcm[i] & pencil_mask;
          new_pencil_blob.tot = (tone_mask - tone) * w[i];
          for (j = 0; j < pencil_blobs; j++)
            if (pencil_blob[j].val == new_pencil_blob.val) break;
          if (j < pencil_blobs)
            pencil_blob[j].tot += new_pencil_blob.tot;
          else
            pencil_blob[pencil_blobs++] = new_pencil_blob;
          for (; j > 0 && pencil_blob[j].tot > pencil_blob[j - 1].tot; j--)
            std::swap(pencil_blob[j], pencil_blob[j - 1]);
        }
        tone = troundp(tone_tot);
        // if (some_pencil && (TUINT32)tone == tone_mask)
        //  tone--;
        // if (color_blob[0].val==0 && pencil_blob[0].val==0)
        //  tone = 255;
        *out_tcm++ = color_blob[0].val | pencil_blob[0].val | tone;
      }
    }

    // Deal with useful source positions on the output line's pre-image
    for (; x < lx; x++) {
      u_ = u_0 + x * inv.a11;
      u  = tfloor(u_);
      v_ = v_0 + x * inv.a21;
      v  = tfloor(v_);
      if (!(MINOR(u, lu) && MINOR(v, lv)))  // u<0 || u>=lu || v<0 || v>=lv
        break;
      in_tcm = bufin_tcm + u +
               v * wrapin;  // Take the associated input pixel pointer
      tcm[0] = in_tcm[0];
      if (u < lu - 1 && v < lv - 1) {
        // Also take their 4 next neighours (we shall perform a kinf of bilinear
        // interpolation)
        tcm[1] = in_tcm[1];
        tcm[2] = in_tcm[wrapin];
        tcm[3] = in_tcm[wrapin + 1];
      } else {
        // Eventually, simulate the off-boundary ones
        tcm[1] = (u == lu - 1) ? in_tcm[0] : in_tcm[1];
        tcm[2] = (v == lv - 1) ? in_tcm[0] : in_tcm[wrapin];
        tcm[3] = (u == lu - 1 || v == lv - 1) ? in_tcm[0] : in_tcm[wrapin + 1];
      }
      if (tcm[0] == tcm[1] && tcm[1] == tcm[2] && tcm[2] == tcm[3])
        *out_tcm++ = tcm[0];  // If they are all equal, it's a copy-op
      else {
        // Otherwise, take the bilinear coordinates
        fu          = u_ - u;
        gu          = 1. - fu;
        fv          = v_ - v;
        gv          = 1. - fv;
        w[0]        = gu * gv;
        w[2]        = gu * fv;  // And the associated weights
        w[1]        = fu * gv;
        w[3]        = fu * fv;
        color_blobs = pencil_blobs = 0;
        tone_tot                   = 0.0;
        some_pencil                = false;
        // Examine all neighbouring pixels
        for (i = 0; i < 4; i++) {
          tone = tcm[i] & tone_mask;  // Take the tone
          if ((TUINT32)tone != tone_mask) some_pencil = true;
          tone_tot += tone * w[i];  // Build the weighted tone sum
          new_color_blob.val = tcm[i] & color_mask;
          new_color_blob.tot =
              tone * w[i];  // And the weighted paint tone for this pixel
          // Fill in the different colors found in an array. Equal colors are
          // stored as one
          // with summed weighted total tone.
          for (j = 0; j < color_blobs; j++)
            if (color_blob[j].val == new_color_blob.val) break;
          if (j < color_blobs)
            color_blob[j].tot += new_color_blob.tot;
          else
            color_blob[color_blobs++] = new_color_blob;
          // Sort the stored colors for decreasing weighted total tone
          for (; j > 0 && color_blob[j].tot > color_blob[j - 1].tot; j--)
            std::swap(color_blob[j], color_blob[j - 1]);

          // Deal the same way with ink colors.
          new_pencil_blob.val = tcm[i] & pencil_mask;
          new_pencil_blob.tot = (tone_mask - tone) * w[i];
          for (j = 0; j < pencil_blobs; j++)
            if (pencil_blob[j].val == new_pencil_blob.val) break;
          if (j < pencil_blobs)
            pencil_blob[j].tot += new_pencil_blob.tot;
          else
            pencil_blob[pencil_blobs++] = new_pencil_blob;
          for (; j > 0 && pencil_blob[j].tot > pencil_blob[j - 1].tot; j--)
            std::swap(pencil_blob[j], pencil_blob[j - 1]);
        }

        tone = tround(tone_tot);
        // if (some_pencil && (TUINT32)tone == tone_mask)
        //  tone--;
        // if (color_blob[0].val==0 && pencil_blob[0].val==0)
        //  tone = 255;

        // The output colors shall be the ones with maximum weighted total tone,
        // with the overall total tone as output tone.
        *out_tcm++ = color_blob[0].val | pencil_blob[0].val | tone;
      }
    }

    // Again, deal with border pixels at the end of line's pre-image
    for (; x < lx; x++) {
      u_ = u_0 + x * inv.a11;
      u  = tfloor(u_);
      v_ = v_0 + x * inv.a21;
      v  = tfloor(v_);
      if (!(MINOREQ(u + 1, lu) &&
            MINOREQ(v + 1, lv)))  // u<-1 || u>=lu || v<-1 || v>=lv
        break;
      in_tcm  = bufin_tcm + u + v * wrapin;
      bool u0 = MINOREQ(u, mu);
      bool v0 = MINOREQ(v, mv);
      bool u1 = MINOREQ(u + 1, mv);
      bool v1 = MINOREQ(v + 1, mv);

      tcm[0] = (u0 && v0) ? in_tcm[0] : transp;
      tcm[1] = (u1 && v0) ? in_tcm[1] : transp;
      tcm[2] = (u0 && v1) ? in_tcm[wrapin] : transp;
      tcm[3] = (u1 && v1) ? in_tcm[wrapin + 1] : transp;
      if (tcm[0] == tcm[1] && tcm[1] == tcm[2] && tcm[2] == tcm[3])
        *out_tcm++ = tcm[0];
      else {
        fu          = u_ - u;
        gu          = 1. - fu;
        fv          = v_ - v;
        gv          = 1. - fv;
        w[0]        = gu * gv;
        w[2]        = gu * fv;
        w[1]        = fu * gv;
        w[3]        = fu * fv;
        color_blobs = pencil_blobs = 0;
        tone_tot                   = 0.0;
        some_pencil                = false;
        for (i = 0; i < 4; i++) {
          tone                                        = tcm[i] & tone_mask;
          if ((TUINT32)tone != tone_mask) some_pencil = true;
          tone_tot += tone * w[i];
          new_color_blob.val = tcm[i] & color_mask;
          new_color_blob.tot = tone * w[i];
          for (j = 0; j < color_blobs; j++)
            if (color_blob[j].val == new_color_blob.val) break;
          if (j < color_blobs)
            color_blob[j].tot += new_color_blob.tot;
          else
            color_blob[color_blobs++] = new_color_blob;
          for (; j > 0 && color_blob[j].tot > color_blob[j - 1].tot; j--)
            std::swap(color_blob[j], color_blob[j - 1]);
          new_pencil_blob.val = tcm[i] & pencil_mask;
          new_pencil_blob.tot = (tone_mask - tone) * w[i];
          for (j = 0; j < pencil_blobs; j++)
            if (pencil_blob[j].val == new_pencil_blob.val) break;
          if (j < pencil_blobs)
            pencil_blob[j].tot += new_pencil_blob.tot;
          else
            pencil_blob[pencil_blobs++] = new_pencil_blob;
          for (; j > 0 && pencil_blob[j].tot > pencil_blob[j - 1].tot; j--)
            std::swap(pencil_blob[j], pencil_blob[j - 1]);
        }
        tone = troundp(tone_tot);
        // if (some_pencil && (TUINT32)tone == tone_mask)
        //  tone--;
        // if (color_blob[0].val==0 && pencil_blob[0].val==0)
        //  tone = 255;

        *out_tcm++ = color_blob[0].val | pencil_blob[0].val | tone;
      }
    }

    // Finally, deal with out-of-source pixels at the end of line's pre-image
    for (; x < lx; x++) *out_tcm++ = transp;
  }

  rin->unlock();
  rout->unlock();
}

//-----------------------------------------------------------------------------

#ifdef _WIN32
template <class T>
void resample_main_cm32_rgbm_SSE2(TRasterPT<T> rout, const TRasterCM32P &rin,
                                  const TAffine &aff_xy2uv,
                                  const TAffine &aff0_uv2fg, int min_pix_ref_u,
                                  int min_pix_ref_v, int max_pix_ref_u,
                                  int max_pix_ref_v, int n_pix, int *pix_ref_u,
                                  int *pix_ref_v, int *pix_ref_f,
                                  int *pix_ref_g, short *filter,
                                  TPalette *palette) {
  __m128i zeros = _mm_setzero_si128();
  const TPixelCM32 *buffer_in;
  T *buffer_out;
  int lu, lv, wrap_in, mu, mv;
  int lx, ly, wrap_out;
  int out_x, out_y;
  double out_x_, out_y_;
  double out_u_, out_v_;
  int ref_u, ref_v;
  int pix_u, pix_v;
  double ref_out_u_, ref_out_v_;
  double ref_out_f_, ref_out_g_;
  int ref_out_f, ref_out_g;
  int pix_out_f, pix_out_g;
  int inside_offset_u, inside_offset_v;
  UINT inside_limit_u, inside_limit_v;
  int inside_nonempty;
  double outside_min_u_, outside_min_v_;
  double outside_max_u_, outside_max_v_;
  UCHAR *calc;
  int calc_allocsize;
  int calc_bytewrap;
  UCHAR calc_value;
  bool must_calc;
  T pix_value;
  T default_value(0, 0, 0, 0);
  float weight;
  float sum_weights;
  float inv_sum_weights;
  int i;

#ifdef USE_DOUBLE_TO_INT
  double d2iaux;
#endif

  T *pix_out;

  __m128 sum_contribs_packed;
  __m128 pix_value_packed;
  __m128 weight_packed;

  __m128 zeros2 = _mm_setzero_ps();

  float maxChannelValue        = (float)T::maxChannelValue;
  __m128 maxChanneValue_packed = _mm_load1_ps(&maxChannelValue);

  if (!(rout->getLx() > 0 && rout->getLy() > 0)) return;
  if (!(rin->getLx() > 0 && rin->getLy() > 0)) {
    resample_clear_rgbm(rout, default_value);
    return;
  }
  calc           = 0;
  calc_allocsize = 0;

  create_calc(rin, min_pix_ref_u, max_pix_ref_u, min_pix_ref_v, max_pix_ref_v,
              calc, calc_allocsize, calc_bytewrap);

  buffer_in  = rin->pixels();
  buffer_out = rout->pixels();
  lu         = rin->getLx();
  lx         = rout->getLx();
  lv         = rin->getLy();
  ly         = rout->getLy();
  wrap_in    = rin->getWrap();
  wrap_out   = rout->getWrap();
  mu         = lu - 1;
  mv         = lv - 1;

  inside_offset_u = -min_pix_ref_u;
  inside_offset_v = -min_pix_ref_v;
  inside_limit_u  = lu - max_pix_ref_u - inside_offset_u;
  inside_limit_v  = lv - max_pix_ref_v - inside_offset_v;
  inside_nonempty = (int)inside_limit_u > 0 && (int)inside_limit_v > 0;
  outside_min_u_  = -0.5;
  outside_min_v_  = -0.5;
  outside_max_u_  = lu - 0.5;
  outside_max_v_  = lv - 0.5;

  int count = palette->getStyleCount();
  int count2 =
      std::max({count, TPixelCM32::getMaxInk(), TPixelCM32::getMaxPaint()});

  TPixelFloat *paints =
      (TPixelFloat *)_aligned_malloc(count2 * sizeof(TPixelFloat), 16);
  TPixelFloat *inks =
      (TPixelFloat *)_aligned_malloc(count2 * sizeof(TPixelFloat), 16);

  std::vector<TPixel32> paints2(count2);
  std::vector<TPixel32> inks2(count2);
  for (i = 0; i < palette->getStyleCount(); i++) {
    TPixel32 color = ::premultiply(palette->getStyle(i)->getAverageColor());
    paints[i] = inks[i] = TPixelFloat(color);
    paints2[i] = inks2[i] = color;
  }

  float maxTone     = (float)TPixelCM32::getMaxTone();
  __m128 den_packed = _mm_load1_ps(&maxTone);

  for (out_y = 0, out_y_ = 0.0; out_y < ly; out_y++, out_y_ += 1.0) {
    for (out_x = 0, out_x_ = 0.0; out_x < lx; out_x++, out_x_ += 1.0) {
      pix_out = buffer_out + out_y * wrap_out + out_x;

      out_u_ = affMV1(aff_xy2uv, out_x_, out_y_);
      out_v_ = affMV2(aff_xy2uv, out_x_, out_y_);
      ref_u  = intLE(out_u_);
      ref_v  = intLE(out_v_);

      if (inside_nonempty && (UINT)(ref_u - inside_offset_u) < inside_limit_u &&
          (UINT)(ref_v - inside_offset_v) < inside_limit_v) {
        calc_value = calc[(ref_u >> 3) + ref_v * calc_bytewrap];
        if (calc_value && ((calc_value >> (ref_u & 7)) & 1)) {
          ref_out_u_  = ref_u - out_u_;
          ref_out_v_  = ref_v - out_v_;
          ref_out_f_  = aff0MV1(aff0_uv2fg, ref_out_u_, ref_out_v_);
          ref_out_g_  = aff0MV2(aff0_uv2fg, ref_out_u_, ref_out_v_);
          ref_out_f   = tround(ref_out_f_);
          ref_out_g   = tround(ref_out_g_);
          sum_weights = 0;

          sum_contribs_packed = _mm_setzero_ps();

          for (i = n_pix - 1; i >= 0; i--) {
            pix_out_f = pix_ref_f[i] + ref_out_f;
            pix_out_g = pix_ref_g[i] + ref_out_g;
            weight    = (float)((filter[pix_out_f] * filter[pix_out_g]) >> 16);
            pix_u     = pix_ref_u[i] + ref_u;
            pix_v     = pix_ref_v[i] + ref_v;

            int pix_in_pos           = pix_u + pix_v * wrap_in;
            const TPixelCM32 *pix_in = buffer_in + pix_in_pos;
            int tone                 = pix_in->getTone();
            int paint                = pix_in->getPaint();
            int ink                  = pix_in->getInk();

            if (tone == TPixelCM32::getMaxTone())
              pix_value_packed = _mm_load_ps((float *)&(paints[paint]));
            else if (tone == 0)
              pix_value_packed = _mm_load_ps((float *)&(inks[ink]));
            else {
              float tt = (float)tone;
              blendBySSE2(pix_value_packed,  // il valore calcolato
                          (float *)&(inks[ink]), (float *)&(paints[paint]), &tt,
                          den_packed, zeros);
            }

            weight_packed = _mm_load1_ps(&weight);
            sum_contribs_packed =
                _mm_add_ps(sum_contribs_packed,
                           _mm_mul_ps(pix_value_packed, weight_packed));

            sum_weights += weight;
          }

          inv_sum_weights               = 1.0f / sum_weights;
          __m128 inv_sum_weights_packed = _mm_load1_ps(&inv_sum_weights);

          __m128 out_fval_packed =
              _mm_mul_ps(sum_contribs_packed, inv_sum_weights_packed);
          out_fval_packed = _mm_max_ps(out_fval_packed, zeros2);
          out_fval_packed = _mm_min_ps(out_fval_packed, maxChanneValue_packed);

          __m128i out_value_packed_i = _mm_cvtps_epi32(out_fval_packed);
          out_value_packed_i  = _mm_packs_epi32(out_value_packed_i, zeros);
          out_value_packed_i  = _mm_packus_epi16(out_value_packed_i, zeros);
          *(DWORD *)(pix_out) = _mm_cvtsi128_si32(out_value_packed_i);
        } else {
          int pix_in_pos           = ref_u + ref_v * wrap_in;
          const TPixelCM32 *pix_in = buffer_in + pix_in_pos;
          int tone                 = pix_in->getTone();
          int paint                = pix_in->getPaint();
          int ink                  = pix_in->getInk();

          if (tone == TPixelCM32::getMaxTone())
            *pix_out = paints2[paint];
          else if (tone == 0)
            *pix_out = inks2[ink];
          else
            *pix_out = blend(inks2[ink], paints2[paint], tone,
                             TPixelCM32::getMaxTone());
        }
      } else if (outside_min_u_ <= out_u_ && out_u_ <= outside_max_u_ &&
                 outside_min_v_ <= out_v_ && out_v_ <= outside_max_v_) {
        if ((UINT)ref_u >= (UINT)lu || (UINT)ref_v >= (UINT)lv)
          must_calc = true;
        else {
          calc_value = calc[(ref_u >> 3) + ref_v * calc_bytewrap];
          must_calc  = calc_value && ((calc_value >> (ref_u & 7)) & 1);
        }

        if (must_calc) {
          ref_out_u_          = ref_u - out_u_;
          ref_out_v_          = ref_v - out_v_;
          ref_out_f_          = aff0MV1(aff0_uv2fg, ref_out_u_, ref_out_v_);
          ref_out_g_          = aff0MV2(aff0_uv2fg, ref_out_u_, ref_out_v_);
          ref_out_f           = tround(ref_out_f_);
          ref_out_g           = tround(ref_out_g_);
          sum_weights         = 0;
          sum_contribs_packed = _mm_setzero_ps();

          for (i = n_pix - 1; i >= 0; i--) {
            pix_out_f = pix_ref_f[i] + ref_out_f;
            pix_out_g = pix_ref_g[i] + ref_out_g;
            weight    = (float)((filter[pix_out_f] * filter[pix_out_g]) >> 16);
            pix_u     = pix_ref_u[i] + ref_u;
            pix_v     = pix_ref_v[i] + ref_v;
            notLessThan(0, pix_u);
            notLessThan(0, pix_v);
            notMoreThan(mu, pix_u);
            notMoreThan(mv, pix_v);

            int pix_in_pos           = pix_u + pix_v * wrap_in;
            const TPixelCM32 *pix_in = buffer_in + pix_in_pos;
            int tone                 = pix_in->getTone();
            int paint                = pix_in->getPaint();
            int ink                  = pix_in->getInk();

            if (tone == TPixelCM32::getMaxTone())
              pix_value_packed = _mm_load_ps((float *)&(paints[paint]));
            else if (tone == 0)
              pix_value_packed = _mm_load_ps((float *)&(inks[ink]));
            else {
              float tt = (float)tone;
              blendBySSE2(pix_value_packed,  // il valore calcolato
                          (float *)&(inks[ink]), (float *)&(paints[paint]), &tt,
                          den_packed, zeros);
            }

            weight_packed = _mm_load1_ps(&weight);
            sum_contribs_packed =
                _mm_add_ps(sum_contribs_packed,
                           _mm_mul_ps(pix_value_packed, weight_packed));

            sum_weights += weight;
          }

          inv_sum_weights = 1.0f / sum_weights;

          __m128 inv_sum_weights_packed = _mm_load1_ps(&inv_sum_weights);
          __m128 out_fval_packed =
              _mm_mul_ps(sum_contribs_packed, inv_sum_weights_packed);
          out_fval_packed = _mm_max_ps(out_fval_packed, zeros2);
          out_fval_packed = _mm_min_ps(out_fval_packed, maxChanneValue_packed);

          __m128i out_value_packed_i = _mm_cvtps_epi32(out_fval_packed);
          out_value_packed_i  = _mm_packs_epi32(out_value_packed_i, zeros);
          out_value_packed_i  = _mm_packus_epi16(out_value_packed_i, zeros);
          *(DWORD *)(pix_out) = _mm_cvtsi128_si32(out_value_packed_i);
        } else {
          int pix_in_pos           = ref_u + ref_v * wrap_in;
          const TPixelCM32 *pix_in = buffer_in + pix_in_pos;
          int tone                 = pix_in->getTone();
          int paint                = pix_in->getPaint();
          int ink                  = pix_in->getInk();

          if (tone == TPixelCM32::getMaxTone())
            *pix_out = paints2[paint];
          else if (tone == 0)
            *pix_out = inks2[ink];
          else
            *pix_out = blend(inks2[ink], paints2[paint], tone,
                             TPixelCM32::getMaxTone());
        }
      } else {
        *pix_out = default_value;
      }
    }
  }
  if (calc) delete[] calc;
}

#endif

/*---------------------------------------------------------------------------*/

namespace {

template <class T>
void resample_main_cm32_rgbm_bigradius(
    TRasterPT<T> rout, const TRasterCM32P &rin, const TAffine &aff_xy2uv,
    const TAffine &aff0_uv2fg, int min_pix_ref_u, int min_pix_ref_v,
    int max_pix_ref_u, int max_pix_ref_v, int n_pix, int *pix_ref_u,
    int *pix_ref_v, int *pix_ref_f, int *pix_ref_g, short *filter,
    TPalette *palette) {
  // bigradius: cambia solo che i sum_contribs sono double invece che int

  const TPixelCM32 *buffer_in;
  T *buffer_out;
  int lu, lv, wrap_in, mu, mv;
  int lx, ly, wrap_out;
  int out_x, out_y;
  double out_x_, out_y_;
  double out_u_, out_v_;
  int ref_u, ref_v;
  int pix_u, pix_v;
  double ref_out_u_, ref_out_v_;
  double ref_out_f_, ref_out_g_;
  int ref_out_f, ref_out_g;
  int pix_out_f, pix_out_g;
  int inside_offset_u, inside_offset_v;
  UINT inside_limit_u, inside_limit_v;
  int inside_nonempty;
  double outside_min_u_, outside_min_v_;
  double outside_max_u_, outside_max_v_;
  UCHAR *calc;
  int calc_allocsize;
  int calc_bytewrap;
  UCHAR calc_value;
  bool must_calc;
  T pix_value;
  T default_value;
  float weight;
  float sum_weights;
  double inv_sum_weights;
  double sum_contribs_r, sum_contribs_g, sum_contribs_b, sum_contribs_m;
  double out_fval_r, out_fval_g, out_fval_b, out_fval_m;
  int out_value_r, out_value_g, out_value_b, out_value_m;
  int i;

#ifdef USE_DOUBLE_TO_INT
  double d2iaux;
#endif

  T *pix_out;

  default_value.r = 0;
  default_value.g = 0;
  default_value.b = 0;
  default_value.m = 0;

  if (!(rout->getLx() > 0 && rout->getLy() > 0)) return;
  if (!(rin->getLx() > 0 && rin->getLy() > 0)) {
    rout->clear();
    return;
  }

  calc           = 0;
  calc_allocsize = 0;
  create_calc(rin, min_pix_ref_u, max_pix_ref_u, min_pix_ref_v, max_pix_ref_v,
              calc, calc_allocsize, calc_bytewrap);

  buffer_in  = rin->pixels();
  buffer_out = rout->pixels();
  lu         = rin->getLx();
  lx         = rout->getLx();
  lv         = rin->getLy();
  ly         = rout->getLy();
  wrap_in    = rin->getWrap();
  wrap_out   = rout->getWrap();
  mu         = lu - 1;
  mv         = lv - 1;

  inside_offset_u = -min_pix_ref_u;
  inside_offset_v = -min_pix_ref_v;
  inside_limit_u  = lu - max_pix_ref_u - inside_offset_u;
  inside_limit_v  = lv - max_pix_ref_v - inside_offset_v;
  inside_nonempty = (int)inside_limit_u > 0 && (int)inside_limit_v > 0;
  outside_min_u_  = -0.5;
  outside_min_v_  = -0.5;
  outside_max_u_  = lu - 0.5;
  outside_max_v_  = lv - 0.5;

  int colorCount = palette->getStyleCount();
  colorCount     = std::max(
      {colorCount, TPixelCM32::getMaxInk(), TPixelCM32::getMaxPaint()});

  std::vector<TPixel32> paints(colorCount);
  std::vector<TPixel32> inks(colorCount);
  for (i      = 0; i < palette->getStyleCount(); i++)
    paints[i] = inks[i] =
        ::premultiply(palette->getStyle(i)->getAverageColor());

  for (out_y = 0, out_y_ = 0.0; out_y < ly; out_y++, out_y_ += 1.0) {
    for (out_x = 0, out_x_ = 0.0; out_x < lx; out_x++, out_x_ += 1.0) {
      pix_out = buffer_out + out_y * wrap_out + out_x;

      out_u_ = affMV1(aff_xy2uv, out_x_, out_y_);
      out_v_ = affMV2(aff_xy2uv, out_x_, out_y_);
      ref_u  = intLE(out_u_);
      ref_v  = intLE(out_v_);

      if (inside_nonempty && (UINT)(ref_u - inside_offset_u) < inside_limit_u &&
          (UINT)(ref_v - inside_offset_v) < inside_limit_v) {
        calc_value = calc[(ref_u >> 3) + ref_v * calc_bytewrap];
        if (calc_value && ((calc_value >> (ref_u & 7)) & 1)) {
          ref_out_u_     = ref_u - out_u_;
          ref_out_v_     = ref_v - out_v_;
          ref_out_f_     = aff0MV1(aff0_uv2fg, ref_out_u_, ref_out_v_);
          ref_out_g_     = aff0MV2(aff0_uv2fg, ref_out_u_, ref_out_v_);
          ref_out_f      = tround(ref_out_f_);
          ref_out_g      = tround(ref_out_g_);
          sum_weights    = 0;
          sum_contribs_r = 0;
          sum_contribs_g = 0;
          sum_contribs_b = 0;
          sum_contribs_m = 0;
          for (i = n_pix - 1; i >= 0; i--) {
            pix_out_f = pix_ref_f[i] + ref_out_f;
            pix_out_g = pix_ref_g[i] + ref_out_g;
            weight    = (float)((filter[pix_out_f] * filter[pix_out_g]) >> 16);
            pix_u     = pix_ref_u[i] + ref_u;
            pix_v     = pix_ref_v[i] + ref_v;

            int pix_in_pos = pix_u + pix_v * wrap_in;
            int tone       = buffer_in[pix_in_pos].getTone();
            int paint      = buffer_in[pix_in_pos].getPaint();
            int ink        = buffer_in[pix_in_pos].getInk();

            if (tone == TPixelCM32::getMaxTone())
              pix_value = Converter<T>::convert(paints[paint]);
            else if (tone == 0)
              pix_value = Converter<T>::convert(inks[ink]);
            else
              pix_value = Converter<T>::convert(blend(
                  inks[ink], paints[paint], tone, TPixelCM32::getMaxTone()));

            sum_contribs_r += (int)pix_value.r * weight;
            sum_contribs_g += (int)pix_value.g * weight;
            sum_contribs_b += (int)pix_value.b * weight;
            sum_contribs_m += (int)pix_value.m * weight;
            sum_weights += weight;
          }
          inv_sum_weights = 1.0 / sum_weights;
          out_fval_r      = sum_contribs_r * inv_sum_weights;
          out_fval_g      = sum_contribs_g * inv_sum_weights;
          out_fval_b      = sum_contribs_b * inv_sum_weights;
          out_fval_m      = sum_contribs_m * inv_sum_weights;
          notLessThan(0.0, out_fval_r);
          notLessThan(0.0, out_fval_g);
          notLessThan(0.0, out_fval_b);
          notLessThan(0.0, out_fval_m);
          out_value_r = troundp(out_fval_r);
          out_value_g = troundp(out_fval_g);
          out_value_b = troundp(out_fval_b);
          out_value_m = troundp(out_fval_m);
          notMoreThan(T::maxChannelValue, out_value_r);
          notMoreThan(T::maxChannelValue, out_value_g);
          notMoreThan(T::maxChannelValue, out_value_b);
          notMoreThan(T::maxChannelValue, out_value_m);

          pix_out->r = out_value_r;
          pix_out->g = out_value_g;
          pix_out->b = out_value_b;
          pix_out->m = out_value_m;

        } else {
          // *pix_out = buffer_in[ref_u + ref_v * wrap_in];

          int pix_in_pos = ref_u + ref_v * wrap_in;
          int tone       = buffer_in[pix_in_pos].getTone();
          int paint      = buffer_in[pix_in_pos].getPaint();
          int ink        = buffer_in[pix_in_pos].getInk();

          if (tone == TPixelCM32::getMaxTone())
            *pix_out = Converter<T>::convert(paints[paint]);
          else if (tone == 0)
            *pix_out = Converter<T>::convert(inks[ink]);
          else
            *pix_out = Converter<T>::convert(blend(
                inks[ink], paints[paint], tone, TPixelCM32::getMaxTone()));
        }
      } else if (outside_min_u_ <= out_u_ && out_u_ <= outside_max_u_ &&
                 outside_min_v_ <= out_v_ && out_v_ <= outside_max_v_) {
        if ((UINT)ref_u >= (UINT)lu || (UINT)ref_v >= (UINT)lv)
          must_calc = true;
        else {
          calc_value = calc[(ref_u >> 3) + ref_v * calc_bytewrap];
          must_calc  = calc_value && ((calc_value >> (ref_u & 7)) & 1);
        }
        if (must_calc) {
          ref_out_u_     = ref_u - out_u_;
          ref_out_v_     = ref_v - out_v_;
          ref_out_f_     = aff0MV1(aff0_uv2fg, ref_out_u_, ref_out_v_);
          ref_out_g_     = aff0MV2(aff0_uv2fg, ref_out_u_, ref_out_v_);
          ref_out_f      = tround(ref_out_f_);
          ref_out_g      = tround(ref_out_g_);
          sum_weights    = 0;
          sum_contribs_r = 0;
          sum_contribs_g = 0;
          sum_contribs_b = 0;
          sum_contribs_m = 0;
          for (i = n_pix - 1; i >= 0; i--) {
            pix_out_f = pix_ref_f[i] + ref_out_f;
            pix_out_g = pix_ref_g[i] + ref_out_g;
            weight    = (float)((filter[pix_out_f] * filter[pix_out_g]) >> 16);
            pix_u     = pix_ref_u[i] + ref_u;
            pix_v     = pix_ref_v[i] + ref_v;
            notLessThan(0, pix_u);
            notLessThan(0, pix_v);
            notMoreThan(mu, pix_u);
            notMoreThan(mv, pix_v);

            // pix_value = buffer_in[pix_u + pix_v * wrap_in];

            int pix_in_pos = pix_u + pix_v * wrap_in;
            int tone       = buffer_in[pix_in_pos].getTone();
            int paint      = buffer_in[pix_in_pos].getPaint();
            int ink        = buffer_in[pix_in_pos].getInk();

            if (tone == TPixelCM32::getMaxTone())
              pix_value = Converter<T>::convert(paints[paint]);
            else if (tone == 0)
              pix_value = Converter<T>::convert(inks[ink]);
            else
              pix_value = Converter<T>::convert(blend(
                  inks[ink], paints[paint], tone, TPixelCM32::getMaxTone()));

            sum_contribs_r += (int)pix_value.r * weight;
            sum_contribs_g += (int)pix_value.g * weight;
            sum_contribs_b += (int)pix_value.b * weight;
            sum_contribs_m += (int)pix_value.m * weight;
            sum_weights += weight;
          }

          inv_sum_weights = 1.0 / sum_weights;
          out_fval_r      = sum_contribs_r * inv_sum_weights;
          out_fval_g      = sum_contribs_g * inv_sum_weights;
          out_fval_b      = sum_contribs_b * inv_sum_weights;
          out_fval_m      = sum_contribs_m * inv_sum_weights;
          notLessThan(0.0, out_fval_r);
          notLessThan(0.0, out_fval_g);
          notLessThan(0.0, out_fval_b);
          notLessThan(0.0, out_fval_m);
          out_value_r = troundp(out_fval_r);
          out_value_g = troundp(out_fval_g);
          out_value_b = troundp(out_fval_b);
          out_value_m = troundp(out_fval_m);
          notMoreThan(T::maxChannelValue, out_value_r);
          notMoreThan(T::maxChannelValue, out_value_g);
          notMoreThan(T::maxChannelValue, out_value_b);
          notMoreThan(T::maxChannelValue, out_value_m);
          pix_out->r = out_value_r;
          pix_out->g = out_value_g;
          pix_out->b = out_value_b;
          pix_out->m = out_value_m;
        } else {
          int pix_in_pos = ref_u + ref_v * wrap_in;
          int tone       = buffer_in[pix_in_pos].getTone();
          int paint      = buffer_in[pix_in_pos].getPaint();
          int ink        = buffer_in[pix_in_pos].getInk();

          if (tone == TPixelCM32::getMaxTone())
            *pix_out = Converter<T>::convert(paints[paint]);
          else if (tone == 0)
            *pix_out = Converter<T>::convert(inks[ink]);
          else
            *pix_out = Converter<T>::convert(blend(
                inks[ink], paints[paint], tone, TPixelCM32::getMaxTone()));
        }
      } else {
        *pix_out = default_value;
      }
    }
  }

  if (calc) delete[] calc;
}
}

/*---------------------------------------------------------------------------*/

template <class T>
void resample_main_cm32_rgbm(TRasterPT<T> rout, const TRasterCM32P &rin,
                             const TAffine &aff_xy2uv,
                             const TAffine &aff0_uv2fg, int min_pix_ref_u,
                             int min_pix_ref_v, int max_pix_ref_u,
                             int max_pix_ref_v, int n_pix, int *pix_ref_u,
                             int *pix_ref_v, int *pix_ref_f, int *pix_ref_g,
                             short *filter, TPalette *palette) {
  const TPixelCM32 *buffer_in;
  T *buffer_out;
  int lu, lv, wrap_in, mu, mv;
  int lx, ly, wrap_out;
  int out_x, out_y;
  double out_x_, out_y_;
  double out_u_, out_v_;
  int ref_u, ref_v;
  int pix_u, pix_v;
  double ref_out_u_, ref_out_v_;
  double ref_out_f_, ref_out_g_;
  int ref_out_f, ref_out_g;
  int pix_out_f, pix_out_g;
  int inside_offset_u, inside_offset_v;
  UINT inside_limit_u, inside_limit_v;
  int inside_nonempty;
  double outside_min_u_, outside_min_v_;
  double outside_max_u_, outside_max_v_;
  UCHAR *calc;
  int calc_allocsize;
  int calc_bytewrap;
  UCHAR calc_value;
  bool must_calc;
  T pix_value;
  T default_value(0, 0, 0, 0);
  int weight;
  int sum_weights;
  double inv_sum_weights;
  int sum_contribs_r, sum_contribs_g, sum_contribs_b, sum_contribs_m;
  double out_fval_r, out_fval_g, out_fval_b, out_fval_m;
  int out_value_r, out_value_g, out_value_b, out_value_m;
  T out_value;
  int i;

#ifdef USE_DOUBLE_TO_INT
  double d2iaux;
#endif

  if (n_pix >= 512 || T::maxChannelValue > 255) {
    resample_main_cm32_rgbm_bigradius<T>(
        rout, rin, aff_xy2uv, aff0_uv2fg, min_pix_ref_u, min_pix_ref_v,
        max_pix_ref_u, max_pix_ref_v, n_pix, pix_ref_u, pix_ref_v, pix_ref_f,
        pix_ref_g, filter, palette);
    return;
  }

  if (!(rout->getLx() > 0 && rout->getLy() > 0)) return;

  if (!(rin->getLx() > 0 && rin->getLy() > 0)) {
    resample_clear_rgbm<T>(rout, default_value);
    return;
  }
  calc           = 0;
  calc_allocsize = 0;
  create_calc(rin, min_pix_ref_u, max_pix_ref_u, min_pix_ref_v, max_pix_ref_v,
              calc, calc_allocsize, calc_bytewrap);

  buffer_in  = rin->pixels();
  buffer_out = rout->pixels();
  lu         = rin->getLx();
  lx         = rout->getLx();
  lv         = rin->getLy();
  ly         = rout->getLy();
  wrap_in    = rin->getWrap();
  wrap_out   = rout->getWrap();
  mu         = lu - 1;
  mv         = lv - 1;

  inside_offset_u = -min_pix_ref_u;
  inside_offset_v = -min_pix_ref_v;
  inside_limit_u  = lu - max_pix_ref_u - inside_offset_u;
  inside_limit_v  = lv - max_pix_ref_v - inside_offset_v;
  inside_nonempty = (int)inside_limit_u > 0 && (int)inside_limit_v > 0;
  outside_min_u_  = -0.5;
  outside_min_v_  = -0.5;
  outside_max_u_  = lu - 0.5;
  outside_max_v_  = lv - 0.5;

  int colorCount = palette->getStyleCount();
  colorCount     = std::max(
      {colorCount, TPixelCM32::getMaxInk(), TPixelCM32::getMaxPaint()});

  std::vector<TPixel32> paints(colorCount);
  std::vector<TPixel32> inks(colorCount);
  for (i      = 0; i < palette->getStyleCount(); i++)
    paints[i] = inks[i] =
        ::premultiply(palette->getStyle(i)->getAverageColor());

  for (out_y = 0, out_y_ = 0.0; out_y < ly; out_y++, out_y_ += 1.0) {
    for (out_x = 0, out_x_ = 0.0; out_x < lx; out_x++, out_x_ += 1.0) {
      out_u_ = affMV1(aff_xy2uv, out_x_, out_y_);
      out_v_ = affMV2(aff_xy2uv, out_x_, out_y_);
      ref_u  = intLE(out_u_);
      ref_v  = intLE(out_v_);

      if (inside_nonempty && (UINT)(ref_u - inside_offset_u) < inside_limit_u &&
          (UINT)(ref_v - inside_offset_v) < inside_limit_v) {
        calc_value = calc[(ref_u >> 3) + ref_v * calc_bytewrap];
        if (calc_value && ((calc_value >> (ref_u & 7)) & 1)) {
          ref_out_u_     = ref_u - out_u_;
          ref_out_v_     = ref_v - out_v_;
          ref_out_f_     = aff0MV1(aff0_uv2fg, ref_out_u_, ref_out_v_);
          ref_out_g_     = aff0MV2(aff0_uv2fg, ref_out_u_, ref_out_v_);
          ref_out_f      = tround(ref_out_f_);
          ref_out_g      = tround(ref_out_g_);
          sum_weights    = 0;
          sum_contribs_r = 0;
          sum_contribs_g = 0;
          sum_contribs_b = 0;
          sum_contribs_m = 0;
          for (i = n_pix - 1; i >= 0; i--) {
            pix_out_f = pix_ref_f[i] + ref_out_f;
            pix_out_g = pix_ref_g[i] + ref_out_g;
            weight    = (filter[pix_out_f] * filter[pix_out_g]) >> 16;
            pix_u     = pix_ref_u[i] + ref_u;
            pix_v     = pix_ref_v[i] + ref_v;

            // pix_value = buffer_in[pix_u + pix_v * wrap_in];

            int pix_in_pos = pix_u + pix_v * wrap_in;
            int tone       = buffer_in[pix_in_pos].getTone();
            int paint      = buffer_in[pix_in_pos].getPaint();
            int ink        = buffer_in[pix_in_pos].getInk();

            if (tone == TPixelCM32::getMaxTone())
              pix_value = Converter<T>::convert(paints[paint]);
            else if (tone == 0)
              pix_value = Converter<T>::convert(inks[ink]);
            else
              pix_value = Converter<T>::convert(blend(
                  inks[ink], paints[paint], tone, TPixelCM32::getMaxTone()));

            sum_contribs_r += (int)pix_value.r * weight;
            sum_contribs_g += (int)pix_value.g * weight;
            sum_contribs_b += (int)pix_value.b * weight;
            sum_contribs_m += (int)pix_value.m * weight;
            sum_weights += weight;
          }

          inv_sum_weights = 1.0 / sum_weights;
          out_fval_r      = sum_contribs_r * inv_sum_weights;
          out_fval_g      = sum_contribs_g * inv_sum_weights;
          out_fval_b      = sum_contribs_b * inv_sum_weights;
          out_fval_m      = sum_contribs_m * inv_sum_weights;
          notLessThan(0.0, out_fval_r);
          notLessThan(0.0, out_fval_g);
          notLessThan(0.0, out_fval_b);
          notLessThan(0.0, out_fval_m);
          out_value_r = troundp(out_fval_r);
          out_value_g = troundp(out_fval_g);
          out_value_b = troundp(out_fval_b);
          out_value_m = troundp(out_fval_m);
          notMoreThan(T::maxChannelValue, out_value_r);
          notMoreThan(T::maxChannelValue, out_value_g);
          notMoreThan(T::maxChannelValue, out_value_b);
          notMoreThan(T::maxChannelValue, out_value_m);
          out_value.r = out_value_r;
          out_value.g = out_value_g;
          out_value.b = out_value_b;
          out_value.m = out_value_m;
        } else {
          // out_value = buffer_in[ref_u + ref_v * wrap_in];
          int pix_in_pos = ref_u + ref_v * wrap_in;
          int tone       = buffer_in[pix_in_pos].getTone();
          int paint      = buffer_in[pix_in_pos].getPaint();
          int ink        = buffer_in[pix_in_pos].getInk();

          if (tone == TPixelCM32::getMaxTone())
            out_value = Converter<T>::convert(paints[paint]);
          else if (tone == 0)
            out_value = Converter<T>::convert(inks[ink]);
          else
            out_value = Converter<T>::convert(blend(
                inks[ink], paints[paint], tone, TPixelCM32::getMaxTone()));
        }
      } else if (outside_min_u_ <= out_u_ && out_u_ <= outside_max_u_ &&
                 outside_min_v_ <= out_v_ && out_v_ <= outside_max_v_) {
        if ((UINT)ref_u >= (UINT)lu || (UINT)ref_v >= (UINT)lv)
          must_calc = true;
        else {
          calc_value = calc[(ref_u >> 3) + ref_v * calc_bytewrap];
          must_calc  = calc_value && ((calc_value >> (ref_u & 7)) & 1);
        }

        if (must_calc) {
          ref_out_u_     = ref_u - out_u_;
          ref_out_v_     = ref_v - out_v_;
          ref_out_f_     = aff0MV1(aff0_uv2fg, ref_out_u_, ref_out_v_);
          ref_out_g_     = aff0MV2(aff0_uv2fg, ref_out_u_, ref_out_v_);
          ref_out_f      = tround(ref_out_f_);
          ref_out_g      = tround(ref_out_g_);
          sum_weights    = 0;
          sum_contribs_r = 0;
          sum_contribs_g = 0;
          sum_contribs_b = 0;
          sum_contribs_m = 0;

          for (i = n_pix - 1; i >= 0; i--) {
            pix_out_f = pix_ref_f[i] + ref_out_f;
            pix_out_g = pix_ref_g[i] + ref_out_g;
            weight    = (filter[pix_out_f] * filter[pix_out_g]) >> 16;
            pix_u     = pix_ref_u[i] + ref_u;
            pix_v     = pix_ref_v[i] + ref_v;
            notLessThan(0, pix_u);
            notLessThan(0, pix_v);
            notMoreThan(mu, pix_u);
            notMoreThan(mv, pix_v);

            // pix_value = buffer_in[pix_u + pix_v * wrap_in];

            int pix_in_pos = pix_u + pix_v * wrap_in;
            int tone       = buffer_in[pix_in_pos].getTone();
            int paint      = buffer_in[pix_in_pos].getPaint();
            int ink        = buffer_in[pix_in_pos].getInk();

            if (tone == TPixelCM32::getMaxTone())
              pix_value = Converter<T>::convert(paints[paint]);
            else if (tone == 0)
              pix_value = Converter<T>::convert(inks[ink]);
            else
              pix_value = Converter<T>::convert(blend(
                  inks[ink], paints[paint], tone, TPixelCM32::getMaxTone()));

            sum_contribs_r += (int)pix_value.r * weight;
            sum_contribs_g += (int)pix_value.g * weight;
            sum_contribs_b += (int)pix_value.b * weight;
            sum_contribs_m += (int)pix_value.m * weight;
            sum_weights += weight;
          }

          inv_sum_weights = 1.0 / sum_weights;
          out_fval_r      = sum_contribs_r * inv_sum_weights;
          out_fval_g      = sum_contribs_g * inv_sum_weights;
          out_fval_b      = sum_contribs_b * inv_sum_weights;
          out_fval_m      = sum_contribs_m * inv_sum_weights;
          notLessThan(0.0, out_fval_r);
          notLessThan(0.0, out_fval_g);
          notLessThan(0.0, out_fval_b);
          notLessThan(0.0, out_fval_m);
          out_value_r = troundp(out_fval_r);
          out_value_g = troundp(out_fval_g);
          out_value_b = troundp(out_fval_b);
          out_value_m = troundp(out_fval_m);
          notMoreThan(T::maxChannelValue, out_value_r);
          notMoreThan(T::maxChannelValue, out_value_g);
          notMoreThan(T::maxChannelValue, out_value_b);
          notMoreThan(T::maxChannelValue, out_value_m);
          out_value.r = out_value_r;
          out_value.g = out_value_g;
          out_value.b = out_value_b;
          out_value.m = out_value_m;
        } else {
          // out_value = buffer_in[ref_u + ref_v * wrap_in];

          int pix_in_pos = ref_u + ref_v * wrap_in;
          int tone       = buffer_in[pix_in_pos].getTone();
          int paint      = buffer_in[pix_in_pos].getPaint();
          int ink        = buffer_in[pix_in_pos].getInk();

          if (tone == TPixelCM32::getMaxTone())
            out_value = Converter<T>::convert(paints[paint]);
          else if (tone == 0)
            out_value = Converter<T>::convert(inks[ink]);
          else
            out_value = Converter<T>::convert(blend(
                inks[ink], paints[paint], tone, TPixelCM32::getMaxTone()));
        }
      } else {
        out_value = default_value;
      }

      buffer_out[out_x + out_y * wrap_out] = out_value;
    }
  }

  if (calc) delete[] calc;
}

//---------------------------------------------------------------------------

void resample_cm32_rgbm(TRaster32P rout, const TRasterCM32P &rin,
                        const TAffine &aff_xy2uv, const TAffine &aff0_uv2fg,
                        int min_pix_ref_u, int min_pix_ref_v, int max_pix_ref_u,
                        int max_pix_ref_v, int n_pix, int *pix_ref_u,
                        int *pix_ref_v, int *pix_ref_f, int *pix_ref_g,
                        short *filter, TPalette *palette) {
  const TPixelCM32 *buffer_in;
  /*T*/ TPixel32 *buffer_out;
  int lu, lv, wrap_in, mu, mv;
  int lx, ly, wrap_out;
  int out_x, out_y;
  double out_x_, out_y_;
  double out_u_, out_v_;
  int ref_u, ref_v;
  int pix_u, pix_v;
  double ref_out_u_, ref_out_v_;
  double ref_out_f_, ref_out_g_;
  int ref_out_f, ref_out_g;
  int pix_out_f, pix_out_g;
  int inside_offset_u, inside_offset_v;
  UINT inside_limit_u, inside_limit_v;
  int inside_nonempty;
  double outside_min_u_, outside_min_v_;
  double outside_max_u_, outside_max_v_;
  UCHAR *calc;
  int calc_allocsize;
  int calc_bytewrap;
  UCHAR calc_value;
  bool must_calc;
  /*T*/ TPixel32 pix_value;
  /*T*/ TPixel32 default_value(0, 0, 0, 0);
  int weight;
  int sum_weights;
  double inv_sum_weights;
  int sum_contribs_r, sum_contribs_g, sum_contribs_b, sum_contribs_m;
  double out_fval_r, out_fval_g, out_fval_b, out_fval_m;
  int out_value_r, out_value_g, out_value_b, out_value_m;
  /*T*/ TPixel32 out_value;
  int i;

#ifdef USE_DOUBLE_TO_INT
  double d2iaux;
#endif

  if (n_pix >= 512 || /*T*/ TPixel32::maxChannelValue > 255) {
    assert(false);
    /*
resample_main_rgbm_bigradius<T>( rout, rin,
                                              aff_xy2uv,
                                            aff0_uv2fg,
                                              min_pix_ref_u, min_pix_ref_v,
                                              max_pix_ref_u, max_pix_ref_v,
                                              n_pix,
                                              pix_ref_u, pix_ref_v,
                                              pix_ref_f, pix_ref_g,
                                            filter );
*/
    return;
  }

  if (!(rout->getLx() > 0 && rout->getLy() > 0)) return;

  if (!(rin->getLx() > 0 && rin->getLy() > 0)) {
    resample_clear_rgbm</*T*/ TPixel32>(rout, default_value);
    return;
  }

  int colorCount = palette->getStyleCount();
  colorCount     = std::max(
      {colorCount, TPixelCM32::getMaxInk(), TPixelCM32::getMaxPaint()});

  std::vector<TPixel32> paints(colorCount);
  std::vector<TPixel32> inks(colorCount);
  for (i      = 0; i < palette->getStyleCount(); i++)
    paints[i] = inks[i] =
        ::premultiply(palette->getStyle(i)->getAverageColor());

  calc           = 0;
  calc_allocsize = 0;
  create_calc(rin, min_pix_ref_u, max_pix_ref_u, min_pix_ref_v, max_pix_ref_v,
              calc, calc_allocsize, calc_bytewrap);

  buffer_in  = rin->pixels();
  buffer_out = rout->pixels();
  lu         = rin->getLx();
  lx         = rout->getLx();
  lv         = rin->getLy();
  ly         = rout->getLy();
  wrap_in    = rin->getWrap();
  wrap_out   = rout->getWrap();
  mu         = lu - 1;
  mv         = lv - 1;

  inside_offset_u = -min_pix_ref_u;
  inside_offset_v = -min_pix_ref_v;
  inside_limit_u  = lu - max_pix_ref_u - inside_offset_u;
  inside_limit_v  = lv - max_pix_ref_v - inside_offset_v;
  inside_nonempty = (int)inside_limit_u > 0 && (int)inside_limit_v > 0;
  outside_min_u_  = -0.5;
  outside_min_v_  = -0.5;
  outside_max_u_  = lu - 0.5;
  outside_max_v_  = lv - 0.5;

  for (out_y = 0, out_y_ = 0.0; out_y < ly; out_y++, out_y_ += 1.0) {
    for (out_x = 0, out_x_ = 0.0; out_x < lx; out_x++, out_x_ += 1.0) {
      out_u_ = affMV1(aff_xy2uv, out_x_, out_y_);
      out_v_ = affMV2(aff_xy2uv, out_x_, out_y_);
      ref_u  = intLE(out_u_);
      ref_v  = intLE(out_v_);

      if (inside_nonempty && (UINT)(ref_u - inside_offset_u) < inside_limit_u &&
          (UINT)(ref_v - inside_offset_v) < inside_limit_v) {
        calc_value = calc[(ref_u >> 3) + ref_v * calc_bytewrap];
        if (calc_value && ((calc_value >> (ref_u & 7)) & 1)) {
          ref_out_u_     = ref_u - out_u_;
          ref_out_v_     = ref_v - out_v_;
          ref_out_f_     = aff0MV1(aff0_uv2fg, ref_out_u_, ref_out_v_);
          ref_out_g_     = aff0MV2(aff0_uv2fg, ref_out_u_, ref_out_v_);
          ref_out_f      = tround(ref_out_f_);
          ref_out_g      = tround(ref_out_g_);
          sum_weights    = 0;
          sum_contribs_r = 0;
          sum_contribs_g = 0;
          sum_contribs_b = 0;
          sum_contribs_m = 0;
          for (i = n_pix - 1; i >= 0; i--) {
            pix_out_f = pix_ref_f[i] + ref_out_f;
            pix_out_g = pix_ref_g[i] + ref_out_g;
            weight    = (filter[pix_out_f] * filter[pix_out_g]) >> 16;
            pix_u     = pix_ref_u[i] + ref_u;
            pix_v     = pix_ref_v[i] + ref_v;

            // pix_value = buffer_in[pix_u + pix_v * wrap_in];

            int pix_in_pos = pix_u + pix_v * wrap_in;
            int t          = buffer_in[pix_in_pos].getTone();
            int p          = buffer_in[pix_in_pos].getPaint();
            int i          = buffer_in[pix_in_pos].getInk();

            if (t == TPixelCM32::getMaxTone())
              pix_value = paints[p];
            else if (t == 0)
              pix_value = inks[i];
            else
              pix_value =
                  blend(inks[i], paints[p], t, TPixelCM32::getMaxTone());

            sum_contribs_r += (int)pix_value.r * weight;
            sum_contribs_g += (int)pix_value.g * weight;
            sum_contribs_b += (int)pix_value.b * weight;
            sum_contribs_m += (int)pix_value.m * weight;
            sum_weights += weight;
          }

          inv_sum_weights = 1.0 / sum_weights;
          out_fval_r      = sum_contribs_r * inv_sum_weights;
          out_fval_g      = sum_contribs_g * inv_sum_weights;
          out_fval_b      = sum_contribs_b * inv_sum_weights;
          out_fval_m      = sum_contribs_m * inv_sum_weights;
          notLessThan(0.0, out_fval_r);
          notLessThan(0.0, out_fval_g);
          notLessThan(0.0, out_fval_b);
          notLessThan(0.0, out_fval_m);
          out_value_r = troundp(out_fval_r);
          out_value_g = troundp(out_fval_g);
          out_value_b = troundp(out_fval_b);
          out_value_m = troundp(out_fval_m);
          notMoreThan(/*T*/ TPixel32::maxChannelValue, out_value_r);
          notMoreThan(/*T*/ TPixel32::maxChannelValue, out_value_g);
          notMoreThan(/*T*/ TPixel32::maxChannelValue, out_value_b);
          notMoreThan(/*T*/ TPixel32::maxChannelValue, out_value_m);
          out_value.r = out_value_r;
          out_value.g = out_value_g;
          out_value.b = out_value_b;
          out_value.m = out_value_m;
        } else {
          int pix_in_pos = ref_u + ref_v * wrap_in;
          int t          = buffer_in[pix_in_pos].getTone();
          int p          = buffer_in[pix_in_pos].getPaint();
          int i          = buffer_in[pix_in_pos].getInk();

          if (t == TPixelCM32::getMaxTone())
            out_value = paints[p];
          else if (t == 0)
            out_value = inks[i];
          else
            out_value = blend(inks[i], paints[p], t, TPixelCM32::getMaxTone());

          // out_value = buffer_in[ref_u + ref_v * wrap_in];
        }
      } else if (outside_min_u_ <= out_u_ && out_u_ <= outside_max_u_ &&
                 outside_min_v_ <= out_v_ && out_v_ <= outside_max_v_) {
        if ((UINT)ref_u >= (UINT)lu || (UINT)ref_v >= (UINT)lv)
          must_calc = true;
        else {
          calc_value = calc[(ref_u >> 3) + ref_v * calc_bytewrap];
          must_calc  = calc_value && ((calc_value >> (ref_u & 7)) & 1);
        }

        if (must_calc) {
          ref_out_u_     = ref_u - out_u_;
          ref_out_v_     = ref_v - out_v_;
          ref_out_f_     = aff0MV1(aff0_uv2fg, ref_out_u_, ref_out_v_);
          ref_out_g_     = aff0MV2(aff0_uv2fg, ref_out_u_, ref_out_v_);
          ref_out_f      = tround(ref_out_f_);
          ref_out_g      = tround(ref_out_g_);
          sum_weights    = 0;
          sum_contribs_r = 0;
          sum_contribs_g = 0;
          sum_contribs_b = 0;
          sum_contribs_m = 0;

          for (i = n_pix - 1; i >= 0; i--) {
            pix_out_f = pix_ref_f[i] + ref_out_f;
            pix_out_g = pix_ref_g[i] + ref_out_g;
            weight    = (filter[pix_out_f] * filter[pix_out_g]) >> 16;
            pix_u     = pix_ref_u[i] + ref_u;
            pix_v     = pix_ref_v[i] + ref_v;
            notLessThan(0, pix_u);
            notLessThan(0, pix_v);
            notMoreThan(mu, pix_u);
            notMoreThan(mv, pix_v);

            // pix_value = buffer_in[pix_u + pix_v * wrap_in];

            int pix_in_pos = pix_u + pix_v * wrap_in;
            int t          = buffer_in[pix_in_pos].getTone();
            int p          = buffer_in[pix_in_pos].getPaint();
            int i          = buffer_in[pix_in_pos].getInk();

            if (t == TPixelCM32::getMaxTone())
              pix_value = paints[p];
            else if (t == 0)
              pix_value = inks[i];
            else
              pix_value =
                  blend(inks[i], paints[p], t, TPixelCM32::getMaxTone());

            sum_contribs_r += (int)pix_value.r * weight;
            sum_contribs_g += (int)pix_value.g * weight;
            sum_contribs_b += (int)pix_value.b * weight;
            sum_contribs_m += (int)pix_value.m * weight;
            sum_weights += weight;
          }

          inv_sum_weights = 1.0 / sum_weights;
          out_fval_r      = sum_contribs_r * inv_sum_weights;
          out_fval_g      = sum_contribs_g * inv_sum_weights;
          out_fval_b      = sum_contribs_b * inv_sum_weights;
          out_fval_m      = sum_contribs_m * inv_sum_weights;
          notLessThan(0.0, out_fval_r);
          notLessThan(0.0, out_fval_g);
          notLessThan(0.0, out_fval_b);
          notLessThan(0.0, out_fval_m);
          out_value_r = troundp(out_fval_r);
          out_value_g = troundp(out_fval_g);
          out_value_b = troundp(out_fval_b);
          out_value_m = troundp(out_fval_m);
          notMoreThan(/*T*/ TPixel32::maxChannelValue, out_value_r);
          notMoreThan(/*T*/ TPixel32::maxChannelValue, out_value_g);
          notMoreThan(/*T*/ TPixel32::maxChannelValue, out_value_b);
          notMoreThan(/*T*/ TPixel32::maxChannelValue, out_value_m);
          out_value.r = out_value_r;
          out_value.g = out_value_g;
          out_value.b = out_value_b;
          out_value.m = out_value_m;
        } else {
          // out_value = buffer_in[ref_u + ref_v * wrap_in];

          int pix_in_pos = ref_u + ref_v * wrap_in;
          int t          = buffer_in[pix_in_pos].getTone();
          int p          = buffer_in[pix_in_pos].getPaint();
          int i          = buffer_in[pix_in_pos].getInk();

          if (t == TPixelCM32::getMaxTone())
            out_value = paints[p];
          else if (t == 0)
            out_value = inks[i];
          else
            out_value = blend(inks[i], paints[p], t, TPixelCM32::getMaxTone());
        }
      } else {
        out_value = default_value;
      }

      buffer_out[out_x + out_y * wrap_out] = out_value;
    }
  }

  if (calc) delete[] calc;
}

//---------------------------------------------------------------------------

template <class T>
void rop_resample_rgbm_2(TRasterPT<T> rout, const TRasterCM32P &rin,
                         const TAffine &aff, TRop::ResampleFilterType flt_type,
                         double blur, TPalette *palette) {
#define FILTER_RESOLUTION 1024
#define MAX_FILTER_VAL 32767

#ifdef USE_STATIC_VARS
  static TRop::ResampleFilterType current_flt_type = TRop::None;
  static std::unique_ptr<short[]> filter_array;
  static short *filter = 0;
  static int min_filter_fg, max_filter_fg;
  static int filter_array_size = 0;
  static int n_pix             = 0;
  static std::unique_ptr<int[]> pix_ref_u;
  static std::unique_ptr<int[]> pix_ref_v;
  static std::unique_ptr<int[]> pix_ref_f;
  static std::unique_ptr<int[]> pix_ref_g;
  static int current_max_n_pix = 0;
#else
  std::unique_ptr<short[]> filter_array;
  short *filter = 0;
  int min_filter_fg, max_filter_fg;
  int filter_array_size = 0;
  int n_pix             = 0;
  std::unique_ptr<int[]> pix_ref_u;
  std::unique_ptr<int[]> pix_ref_v;
  std::unique_ptr<int[]> pix_ref_f;
  std::unique_ptr<int[]> pix_ref_g;
  int current_max_n_pix = 0;
#endif

  int filter_st_radius;
  int filter_fg_radius;
  int filter_size;
  int f;
  double s_;
  double weight_;
  int weight;
  TAffine aff_uv2xy;
  TAffine aff_xy2uv;
  TAffine aff0_uv2xy;
  TAffine aff0_xy2st;
  TAffine aff0_uv2st;
  TAffine aff0_st2fg;
  TAffine aff0_uv2fg;
  TAffine aff0_fg2uv;
  double scale_x, scale_y;
  double inv_blur;
  int max_n_pix;
  double min_pix_out_u_, min_pix_out_v_;
  double max_pix_out_u_, max_pix_out_v_;
  int min_pix_ref_u, min_pix_ref_v;
  int max_pix_ref_u, max_pix_ref_v;
  int cur_pix_ref_u, cur_pix_ref_v;
  double cur_pix_ref_f_, cur_pix_ref_g_;
  int cur_pix_ref_f, cur_pix_ref_g;
  double min_ref_out_f_, min_ref_out_g_;
  double max_ref_out_f_, max_ref_out_g_;
  int min_ref_out_f, min_ref_out_g;
  int max_ref_out_f, max_ref_out_g;
  int min_pix_ref_f, min_pix_ref_g;
  int max_pix_ref_f, max_pix_ref_g;
  int min_pix_out_f, min_pix_out_g;
  int max_pix_out_f, max_pix_out_g;
  int min_pix_out_fg;
  int max_pix_out_fg;

#ifdef USE_DOUBLE_TO_INT
  double d2iaux;
#endif

  assert(flt_type != TRop::None);

  filter_st_radius = get_filter_radius(flt_type);
  filter_fg_radius = filter_st_radius * FILTER_RESOLUTION;

  aff_uv2xy  = aff;
  aff0_uv2xy = aff_uv2xy.place(0.0, 0.0, 0.0, 0.0);
  aff_xy2uv  = aff_uv2xy.inv();

  scale_x    = sqrt(sq(aff_uv2xy.a11) + sq(aff_uv2xy.a12));
  scale_y    = sqrt(sq(aff_uv2xy.a21) + sq(aff_uv2xy.a22));
  aff0_xy2st = TScale((scale_x > 1.0) ? 1.0 / scale_x : 1.0,
                      (scale_y > 1.0) ? 1.0 / scale_y : 1.0);

  if (blur > 1.0)  // per ora il blur e' 1.0
  {
    inv_blur   = 1.0 / blur;
    aff0_xy2st = TScale(inv_blur, inv_blur) * aff0_xy2st;
  }

  aff0_uv2st = aff0_xy2st * aff0_uv2xy;
  aff0_st2fg = TScale(FILTER_RESOLUTION, FILTER_RESOLUTION);
  aff0_uv2fg = aff0_st2fg * aff0_uv2st;
  aff0_fg2uv = aff0_uv2fg.inv();

  minmax(-filter_fg_radius, -filter_fg_radius, filter_fg_radius,
         filter_fg_radius, aff0_fg2uv, min_pix_out_u_, min_pix_out_v_,
         max_pix_out_u_, max_pix_out_v_);

  min_pix_ref_u = intGT(min_pix_out_u_);
  min_pix_ref_v = intGT(min_pix_out_v_);
  max_pix_ref_u = intLT(max_pix_out_u_) + 1;
  max_pix_ref_v = intLT(max_pix_out_v_) + 1;

  if (blur <= 1.0) {
    if (aff_uv2xy.a12 == 0.0 && aff_uv2xy.a21 == 0.0) {
      if (aff_uv2xy.a11 == 1.0 && isInt(aff_uv2xy.a13)) {
        min_pix_ref_u = 0;
        max_pix_ref_u = 0;
      }
      if (aff_uv2xy.a22 == 1.0 && isInt(aff_uv2xy.a23)) {
        min_pix_ref_v = 0;
        max_pix_ref_v = 0;
      }
    } else if (aff_uv2xy.a11 == 0.0 && aff_uv2xy.a22 == 0.0) {
      if (aff_uv2xy.a12 == 1.0 && isInt(aff_uv2xy.a13)) {
        min_pix_ref_v = 0;
        max_pix_ref_v = 0;
      }
      if (aff_uv2xy.a21 == 1.0 && isInt(aff_uv2xy.a23)) {
        min_pix_ref_u = 0;
        max_pix_ref_u = 0;
      }
    }
  }

  max_n_pix =
      (max_pix_ref_u - min_pix_ref_u + 1) * (max_pix_ref_v - min_pix_ref_v + 1);

  if (max_n_pix > current_max_n_pix) {
    current_max_n_pix = max_n_pix;
    pix_ref_u.reset(new int[current_max_n_pix]);
    pix_ref_v.reset(new int[current_max_n_pix]);
    pix_ref_f.reset(new int[current_max_n_pix]);
    pix_ref_g.reset(new int[current_max_n_pix]);
    assert(pix_ref_u && pix_ref_v && pix_ref_f && pix_ref_g);
  }

  minmax(-1, -1, 0, 0, aff0_uv2fg, min_ref_out_f_, min_ref_out_g_,
         max_ref_out_f_, max_ref_out_g_);

  min_ref_out_f = tround(min_ref_out_f_);
  min_ref_out_g = tround(min_ref_out_g_);
  max_ref_out_f = tround(max_ref_out_f_);
  max_ref_out_g = tround(max_ref_out_g_);
  min_pix_ref_f = -filter_fg_radius - max_ref_out_f;
  min_pix_ref_g = -filter_fg_radius - max_ref_out_g;
  max_pix_ref_f = filter_fg_radius - min_ref_out_f;
  max_pix_ref_g = filter_fg_radius - min_ref_out_g;

  min_pix_out_f = c_maxint;
  min_pix_out_g = c_maxint;
  max_pix_out_f = c_minint;
  max_pix_out_g = c_minint;
  n_pix         = 0;
  for (cur_pix_ref_v = min_pix_ref_v; cur_pix_ref_v <= max_pix_ref_v;
       cur_pix_ref_v++)
    for (cur_pix_ref_u = min_pix_ref_u; cur_pix_ref_u <= max_pix_ref_u;
         cur_pix_ref_u++) {
      cur_pix_ref_f_ = affMV1(aff0_uv2fg, cur_pix_ref_u, cur_pix_ref_v);
      cur_pix_ref_g_ = affMV2(aff0_uv2fg, cur_pix_ref_u, cur_pix_ref_v);
      cur_pix_ref_f  = tround(cur_pix_ref_f_);
      cur_pix_ref_g  = tround(cur_pix_ref_g_);
      if (min_pix_ref_f <= cur_pix_ref_f && cur_pix_ref_f <= max_pix_ref_f &&
          min_pix_ref_g <= cur_pix_ref_g && cur_pix_ref_g <= max_pix_ref_g) {
        pix_ref_u[n_pix] = cur_pix_ref_u;
        pix_ref_v[n_pix] = cur_pix_ref_v;
        pix_ref_f[n_pix] = cur_pix_ref_f;
        pix_ref_g[n_pix] = cur_pix_ref_g;
        notMoreThan(cur_pix_ref_f + min_ref_out_f, min_pix_out_f);
        notMoreThan(cur_pix_ref_g + min_ref_out_g, min_pix_out_g);
        notLessThan(cur_pix_ref_f + max_ref_out_f, max_pix_out_f);
        notLessThan(cur_pix_ref_g + max_ref_out_g, max_pix_out_g);
        n_pix++;
      }
    }
  assert(n_pix > 0);

#ifdef USE_STATIC_VARS
  if (flt_type != current_flt_type) {
    current_flt_type = flt_type;
#endif
    min_filter_fg = -filter_fg_radius - FILTER_RESOLUTION * 3 / 2;
    max_filter_fg = filter_fg_radius + FILTER_RESOLUTION * 3 / 2;
    filter_size   = max_filter_fg - min_filter_fg + 1;
    if (filter_size > filter_array_size) {
      filter_array.reset(new short[filter_size]);
      assert(filter_array);
      filter_array_size = filter_size;
    }
    filter    = filter_array.get() - min_filter_fg;
    filter[0] = MAX_FILTER_VAL;
    for (f = 1, s_ = 1.0 / FILTER_RESOLUTION; f < filter_fg_radius;
         f++, s_ += 1.0 / FILTER_RESOLUTION) {
      weight_    = get_filter_value(flt_type, s_) * (double)MAX_FILTER_VAL;
      weight     = tround(weight_);
      filter[f]  = weight;
      filter[-f] = weight;
    }
    for (f = filter_fg_radius; f <= max_filter_fg; f++) filter[f] = 0;
    for (f = -filter_fg_radius; f >= min_filter_fg; f--) filter[f] = 0;

#ifdef USE_STATIC_VARS
  }
#endif

  min_pix_out_fg = std::min(min_pix_out_f, min_pix_out_g);
  max_pix_out_fg = std::max(max_pix_out_f, max_pix_out_g);
  if (min_pix_out_fg < min_filter_fg || max_pix_out_fg > max_filter_fg) {
    filter_size = max_pix_out_fg - min_pix_out_fg + 1;
    if (filter_size > filter_array_size) {
      // controllare!!
      // TREALLOC (filter_array, filter_size)
      filter_array.reset(new short[filter_size]);

      assert(filter_array);
      filter_array_size = filter_size;
    }
    filter = filter_array.get() - min_filter_fg;
    if (min_pix_out_fg < min_filter_fg) {
      int delta = min_filter_fg - min_pix_out_fg;

      for (f              = max_filter_fg; f >= min_filter_fg; f--)
        filter[f + delta] = filter[f];
      filter += delta;
      for (f = min_filter_fg - 1; f >= min_pix_out_fg; f--) filter[f] = 0;
      min_filter_fg = min_pix_out_fg;
    }
    if (max_pix_out_fg > max_filter_fg) {
      for (f = max_filter_fg + 1; f <= max_pix_out_fg; f++) filter[f] = 0;
      max_filter_fg = max_pix_out_fg;
    }
  }

#ifdef _MSC_VER
  TRaster32P rout32 = rout;
  if ((TSystem::getCPUExtensions() & TSystem::CpuSupportsSse2) && rout32)
    resample_main_cm32_rgbm_SSE2<TPixel32>(
        rout32, rin, aff_xy2uv, aff0_uv2fg, min_pix_ref_u, min_pix_ref_v,
        max_pix_ref_u, max_pix_ref_v, n_pix, pix_ref_u.get(), pix_ref_v.get(),
        pix_ref_f.get(), pix_ref_g.get(), filter, palette);
  else
#endif
    resample_main_cm32_rgbm<T>(
        rout, rin, aff_xy2uv, aff0_uv2fg, min_pix_ref_u, min_pix_ref_v,
        max_pix_ref_u, max_pix_ref_v, n_pix, pix_ref_u.get(), pix_ref_v.get(),
        pix_ref_f.get(), pix_ref_g.get(), filter, palette);
}

//-----------------------------------------------------------------------------

}  // namespace

//-----------------------------------------------------------------------------
void TRop::resample(const TRasterP &out, const TRasterCM32P &in,
                    const TPaletteP palette, const TAffine &aff,
                    TRop::ResampleFilterType filterType, double blur) {
  TRasterP rin      = in;
  TRaster32P rout32 = out;
  in->lock();
  out->lock();
  if (rout32)
    rop_resample_rgbm_2<TPixel32>(rout32, rin, aff, filterType, blur,
                                  palette.getPointer());
  else {
    TRaster64P rout64 = out;
    if (rout64)
      rop_resample_rgbm_2<TPixel64>(rout64, rin, aff, filterType, blur,
                                    palette.getPointer());
    else {
      in->unlock();
      out->unlock();
      throw TRopException("unsupported pixel type");
      return;
    }
  }
  in->unlock();
  out->unlock();
}

#endif  // TNZCORE_LIGHT

void TRop::resample(const TRasterP &rout, const TRasterP &rin,
                    const TAffine &aff, ResampleFilterType filterType,
                    double blur) {
  rin->lock();
  rout->lock();

  if (filterType == ClosestPixel || filterType == Bilinear) {
    if ((TRaster64P)rout || (TRaster64P)rin)
      filterType = Triangle;
    else {
      quickResample(rout, rin, aff, filterType);
      rin->unlock();
      rout->unlock();
      return;
    }
  }

  TRaster32P rout32 = rout, rin32 = rin;
  if (rout32) {
    if (!rin32) {
      rin32 = TRaster32P(rin->getLx(), rin->getLy());
      TRop::convert(rin32, rin);
    }
    do_resample<TPixel32>(rout32, rin32, aff, filterType, blur);
  } else {
#ifndef TNZCORE_LIGHT
    TRasterCM32P routCM32 = rout, rinCM32 = rin;
    if (routCM32 && rinCM32)
      do_resample(routCM32, rinCM32, aff);
    else
#endif
    {
      TRaster64P rout64 = rout, rin64 = rin;
      if (rout64) {
        if (!rin64) {
          rin64 = TRaster64P(rin->getLx(), rin->getLy());
          TRop::convert(rin64, rin);
        }
        do_resample<TPixel64>(rout64, rin64, aff, filterType, blur);
      } else {
        TRasterGR8P routGR8 = rout, rinGR8 = rin;
        TRaster32P rin32 = rin;
        if (routGR8 && rinGR8)
          do_resample(routGR8, rinGR8, aff, filterType, blur);
        else if (routGR8 && rin32)
          do_resample(routGR8, rin32, aff, filterType, blur);
        else {
          rin->unlock();
          rout->unlock();
          throw TRopException("unsupported pixel type");
        }
      }
    }
  }
  rin->unlock();
  rout->unlock();
}

//-----------------------------------------------------------------------------
