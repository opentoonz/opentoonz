

#include "warp.h"
#include "toonz/tdistort.h"
#include "timage_io.h"  //For debug use only

//-------------------------------------------------------------------

namespace {

// Local inlines

template <typename T>
inline double convert(const T &pixel);

template <>
inline double convert<TPixel32>(const TPixel32 &pixel) {
  return TPixelGR8::from(pixel).value;
}

template <>
inline double convert<TPixel64>(const TPixel64 &pixel) {
  return TPixelGR16::from(pixel).value;
}

template <>
inline double convert<TPixelF>(const TPixelF &pixel) {
  // clamp between 0 and 1
  return std::min(1.f, std::max(0.f, TPixelGRF::from(pixel).value));
}
}  // namespace

/*-----------------------------------------------------------------*/

template <typename T>
class Warper final : public TDistorter {
public:
  TRasterPT<T> m_rin;
  TRasterPT<T> m_warper;
  TRasterPT<T> m_rout;
  TPointD m_rinPos;
  TPointD m_warperPos;
  TDimension m_oriDim;
  int m_shrink;
  double m_warperScale;
  double m_intensity;
  bool m_sharpen;
  Lattice m_lattice;

  Warper(TPointD rinPos, TPointD warperPos, const TRasterPT<T> &rin,
         const TRasterPT<T> &warper, TRasterPT<T> &rout,
         const WarpParams &params)
      : m_rinPos(rinPos)
      , m_warperPos(warperPos)
      , m_rin(rin)
      , m_warper(warper)
      , m_rout(rout)
      , m_intensity(1.5 * 1.5 * params.m_intensity / 100)
      , m_shrink(params.m_shrink)
      , m_warperScale(params.m_warperScale)
      , m_oriDim(rin->getSize())
      , m_sharpen(params.m_sharpen) {}

  ~Warper() {}

  void createLattice();
  void shepardWarp();
  TPointD map(const TPointD &p) const override;
  int invMap(const TPointD &p, TPointD *invs) const override;
  int maxInvCount() const override { return 1; }
};

/*---------------------------------------------------------------------------*/

template <typename T>
void Warper<T>::createLattice() {
  int ori_lx, ori_ly, i, j, lx, ly, incr;
  double fac;

  ori_lx = m_shrink * (m_warper->getLx() - 1) + 1;
  ori_ly = m_shrink * (m_warper->getLy() - 1) + 1;

  lx = m_lattice.m_width = ori_lx;
  ly = m_lattice.m_height = ori_ly;

  TRasterPT<T> aux = m_warper;

  if (!m_sharpen) TRop::blur(aux, aux, 6.0, 0, 0);

  m_lattice.coords = new LPoint[lx * ly];

  LPoint *coord = m_lattice.coords;

  for (j = 0; j < ly; ++j) {
    if (j >= ly - 1) j = ori_ly - 1;
    coord->s.y = (coord + lx - 1)->s.y = 0;
    coord->d.y = (coord + lx - 1)->d.y = j;

    (coord + lx - 1)->s.x = 0;
    (coord + lx - 1)->d.x = ori_lx - 1;
    coord += lx;
  }

  coord = m_lattice.coords;
  incr  = (ly - 1) * lx;

  for (i = 0; i < lx; ++i) {
    if (i >= (lx - 1)) i = ori_lx - 1;
    coord->s.x = (coord + incr)->s.x = 0;
    coord->d.x = (coord + incr)->d.x = i;
    (coord + incr)->s.y              = 0;
    (coord + incr)->d.y              = ori_ly - 1;
    coord++;
  }

  fac = m_intensity * (TPixel32::maxChannelValue / (double)T::maxChannelValue);
  aux->lock();
  T *buffer = (T *)aux->getRawData();
  T *pixIn;
  int auxWrap = aux->getWrap();

  for (j = 1; j < ly - 1; j++) {
    pixIn = buffer + j * auxWrap;
    coord = &(m_lattice.coords[j * lx]);

    for (i = 1; i < lx - 1; i++) {
      ++pixIn;
      ++coord;
      coord->d.x = i;
      coord->d.y = j;

      // FOR A FUTURE RELEASE: We should not make the diffs below between +1 and
      // -1, BUT 0 and -1!!

      coord->s.x = fac * (convert(*(pixIn + 1)) - convert(*(pixIn - 1)));
      coord->s.y =
          fac * (convert(*(pixIn + auxWrap)) - convert(*(pixIn - auxWrap)));
    }
  }

  aux->unlock();

  // Finally, we scale the lattice according to the m_scale parameter.
  coord  = m_lattice.coords;
  int wh = m_lattice.m_width * m_lattice.m_height;
  for (i = 0; i < wh; ++i, ++coord) {
    coord->d = m_warperPos + m_warperScale * coord->d;
    coord->s = m_warperScale * coord->s;
  }
}

//---------------------------------------------------------------------------

template <typename T>
void Warper<T>::shepardWarp() {
  assert(m_rin.getPointer() != m_rout.getPointer());

  m_rin->lock();
  m_rout->lock();

  TRasterP rasIn(m_rin);
  TRasterP rasOut(m_rout);
  distort(rasOut, rasIn, *this, -convert(m_rinPos), TRop::Bilinear);

  m_rout->unlock();
  m_rin->unlock();
}

//---------------------------------------------------------------------------

template <typename T>
TPointD Warper<T>::map(const TPointD &p) const {
  return TPointD();  // Not truly necessary
}

//---------------------------------------------------------------------------

template <typename T>
int Warper<T>::invMap(const TPointD &p, TPointD *invs) const {
  // Make a Shepard interpolant of grid points
  const double maxDist = 2 * m_warperScale;

  TPointD pos(p + m_rinPos);

  // First, bisect for the interesting maxDist-from-p region
  int i, j;
  double xStart = pos.x - maxDist;
  double yStart = pos.y - maxDist;
  double xEnd   = pos.x + maxDist;
  double yEnd   = pos.y + maxDist;

  int a = 0, b = m_lattice.m_width;
  while (a + 1 < b) {
    i = (a + b) / 2;
    if (m_lattice.coords[i].d.x < xStart)
      a = i;
    else
      b = i;
  }
  i = a;

  a = 0, b = m_lattice.m_height;
  while (a + 1 < b) {
    j = (a + b) / 2;
    if (m_lattice.coords[j * m_lattice.m_width].d.y < yStart)
      a = j;
    else
      b = j;
  }
  j = a;

  // Then, build the interpolation
  int u, v;
  double w, wsum = 0;
  double xDistSq, yDistSq;
  double distSq, maxDistSq = sq(maxDist);
  TPointD result;

  for (v = j; v < m_lattice.m_height; ++v) {
    int vidx = v * m_lattice.m_width;
    if (m_lattice.coords[vidx].d.y > yEnd) break;

    yDistSq = sq(pos.y - m_lattice.coords[vidx].d.y);

    LPoint *coord = &m_lattice.coords[vidx + i];
    for (u = i; u < m_lattice.m_width; ++u, ++coord) {
      xDistSq = sq(pos.x - m_lattice.coords[u].d.x);
      if (m_lattice.coords[u].d.x > xEnd) break;

      distSq = xDistSq + yDistSq;
      if (distSq > maxDistSq) continue;

      w = maxDist - sqrt(distSq);
      wsum += w;
      result += w * coord->s;
    }
  }

  if (wsum)
    invs[0] = p + TPointD(result.x / wsum, result.y / wsum);
  else
    invs[0] = p;

  return 1;
}

//---------------------------------------------------------------------------

//! Calculates the geometry we need for this node computation, given
//! the known warped bbox, the requested rect, and the warp params.
void getWarpComputeRects(TRectD &outputComputeRect, TRectD &warpedComputeRect,
                         const TRectD &warpedBox, const TRectD &requestedRect,
                         const WarpParams &params) {
  if (requestedRect.isEmpty() || warpedBox.isEmpty()) {
    warpedComputeRect.empty();
    outputComputeRect.empty();
    return;
  }

  // We are to find out the geometry that is useful for the fx computation.
  // There are some rules to follow:
  //  0) At this stage, we are definitely not aware of what lies in the warper
  //     image. Therefore, we must assume the maximum warp factor allowed by the
  //     warp params for each of its points - see getWarpRadius().
  //  2) Pixels contributing to any output are necessarily part of warpedBox -
  //  and only
  //     those which are warpable into the requestedRect are useful to us
  //     (i.e. pixels contained in its enlargement by the warp radius).

  double warpRadius = getWarpRadius(params) * params.m_warperScale;
  TRectD enlargedOut(requestedRect.enlarge(warpRadius));
  TRectD enlargedBox(warpedBox.enlarge(warpRadius));

  warpedComputeRect = enlargedOut * warpedBox;
  outputComputeRect = enlargedBox * requestedRect;

  // Finally, make sure that the result is coherent with the requestedRect's P00
  warpedComputeRect -= requestedRect.getP00();
  warpedComputeRect.x0 = tfloor(warpedComputeRect.x0);
  warpedComputeRect.y0 = tfloor(warpedComputeRect.y0);
  warpedComputeRect.x1 = tceil(warpedComputeRect.x1);
  warpedComputeRect.y1 = tceil(warpedComputeRect.y1);
  warpedComputeRect += requestedRect.getP00();

  outputComputeRect -= requestedRect.getP00();
  outputComputeRect.x0 = tfloor(outputComputeRect.x0);
  outputComputeRect.y0 = tfloor(outputComputeRect.y0);
  outputComputeRect.x1 = tceil(outputComputeRect.x1);
  outputComputeRect.y1 = tceil(outputComputeRect.y1);
  outputComputeRect += requestedRect.getP00();
}

//---------------------------------------------------------------------------

//! Deals with raster tiles and invokes warper functions.
//!\b NOTE: \b tileRas's size should be \b warper's one multiplied by
//! params.m_scale.
void warp(TRasterP &tileRas, const TRasterP &rasIn, TRasterP &warper,
          TPointD rasInPos, TPointD warperPos, const WarpParams &params) {
  TRaster32P rasIn32   = rasIn;
  TRaster32P tileRas32 = tileRas;
  TRaster32P warper32  = warper;
  TRaster64P rasIn64   = rasIn;
  TRaster64P tileRas64 = tileRas;
  TRaster64P warper64  = warper;
  TRasterFP rasInF     = rasIn;
  TRasterFP tileRasF   = tileRas;
  TRasterFP warperF    = warper;

  if (rasIn32 && tileRas32 && warper32) {
    Warper<TPixel32> warper(rasInPos, warperPos, rasIn32, warper32, tileRas32,
                            params);
    warper.createLattice();
    warper.shepardWarp();
  } else if (rasIn64 && tileRas64 && warper64) {
    Warper<TPixel64> warper(rasInPos, warperPos, rasIn64, warper64, tileRas64,
                            params);
    warper.createLattice();
    warper.shepardWarp();
  } else if (rasInF && tileRasF && warperF) {
    Warper<TPixelF> warper(rasInPos, warperPos, rasInF, warperF, tileRasF,
                           params);
    warper.createLattice();
    warper.shepardWarp();
  } else
    throw TRopException("warp: unsupported raster types");
}
