

#include "stdfx.h"
#include "tfxparam.h"
#include "tpixelutils.h"
#include "trop.h"
#include "tparamset.h"

//****************************************************************************
//    Local namespace stuff
//****************************************************************************

namespace {
inline void pixelConvert(TPixel32 &dst, const TPixel32 &src) { dst = src; }
inline void pixelConvert(TPixel64 &dst, const TPixel32 &src) {
  dst = toPixel64(src);
}

inline TRect gridAlign(const TRect &rect, TPoint origin, double step) {
  TRect result(rect);
  result -= origin;

  result.x0 = step * tfloor(result.x0 / step);
  result.y0 = step * tfloor(result.y0 / step);
  result.x1 = step * tceil((result.x1 + 1) / step) - 1;
  result.y1 = step * tceil((result.y1 + 1) / step) - 1;

  result += origin;
  return result;
}

inline TRectD gridAlign(const TRectD &rect, TPointD origin, double step) {
  TRectD result(rect);
  result -= origin;

  result.x0 = step * tfloor(result.x0 / step);
  result.y0 = step * tfloor(result.y0 / step);
  result.x1 = step * tceil(result.x1 / step);
  result.y1 = step * tceil(result.y1 / step);

  result += origin;
  return result;
}
}

//****************************************************************************
//    Mosaic Namespace
//****************************************************************************

namespace mosaic {
/*!
    The mosaic::CellBuilder class is the virtual base class used by MosaicFx
    to render a Mosaic cell with supplied colors.
  */
template <typename PIXEL>
class CellBuilder {
protected:
  int m_lx, m_ly;
  double m_radius;
  int m_wrap;

public:
  CellBuilder(int cellLx, int cellLy, double radius, int wrap)
      : m_lx(cellLx), m_ly(cellLy), m_radius(radius), m_wrap(wrap) {}
  virtual ~CellBuilder() {}

  virtual void doCell(PIXEL *cellBuffer, const PIXEL &cellColor,
                      const PIXEL &bgColor, int x0, int y0, int x1, int y1) = 0;
};

}  // namespace mosaic

//****************************************************************************
//    CellBuilder implementations
//****************************************************************************

namespace mosaic {

template <typename PIXEL, typename GRAY>
class MaskCellBuilder : public CellBuilder<PIXEL> {
protected:
  TRasterPT<GRAY> m_mask;

public:
  MaskCellBuilder(int cellLx, int cellLy, double radius, int wrap)
      : CellBuilder<PIXEL>(cellLx, cellLy, radius, wrap) {}

  void doCell(PIXEL *cellBuffer, const PIXEL &cellColor, const PIXEL &bgColor,
              int x0, int y0, int x1, int y1) override {
    // Apply the mask to the cell. 0 pixels are bgColored, GRAY::maxChannelValue
    // ones are cellColored.
    PIXEL *pix, *line    = cellBuffer, *lineEnd;
    GRAY *grPix, *grLine = m_mask->pixels(y0) + x0, *grLineEnd;
    int x, y, grWrap = m_mask->getWrap(), lx = x1 - x0;
    for (y = y0; y < y1; ++y, line += this->m_wrap, grLine += grWrap) {
      lineEnd   = line + lx;
      grLineEnd = grLine + lx;
      for (x = x0, pix = line, grPix = grLine; x < x1; ++x, ++pix, ++grPix)
        *pix = blend(bgColor, cellColor,
                     grPix->value / (double)GRAY::maxChannelValue);
    }
  }
};

template <typename PIXEL, typename GRAY>
class SquareBuilder final : public MaskCellBuilder<PIXEL, GRAY> {
public:
  SquareBuilder(int cellLx, int cellLy, double radius, int wrap)
      : MaskCellBuilder<PIXEL, GRAY>(cellLx, cellLy, radius, wrap) {
    // Build the mask corresponding to a square of passed radius
    GRAY *pix, *pixRev, *line, *lineEnd, *lineRev;
    this->m_mask = TRasterPT<GRAY>(cellLx, cellLy);

    // For each pixel in the lower-left quadrant, fill in the corresponding mask
    // value.
    // The other ones are filled by mirroring.
    int i, j;
    double lxHalf = 0.5 * cellLx, lyHalf = 0.5 * cellLy;
    int lxHalfI = tceil(lxHalf), lyHalfI = tceil(lyHalf);
    double val, addValX = radius - lxHalf + 1, addValY = radius - lyHalf + 1;

    for (i = 0; i < lyHalfI; ++i) {
      line    = this->m_mask->pixels(i),
      lineRev = this->m_mask->pixels(cellLy - i - 1);
      lineEnd = line + cellLx;
      for (j = 0, pix = line, pixRev = lineEnd - 1; j < lxHalfI;
           ++j, ++pix, --pixRev) {
        val  = tcrop(addValX + i, 0.0, 1.0) * tcrop(addValY + j, 0.0, 1.0);
        *pix = *pixRev = val * GRAY::maxChannelValue;
      }

      memcpy(lineRev, line, cellLx * sizeof(GRAY));
    }
  }
};

template <typename PIXEL, typename GRAY>
class CircleBuilder final : public MaskCellBuilder<PIXEL, GRAY> {
public:
  CircleBuilder(int cellLx, int cellLy, double radius, int wrap)
      : MaskCellBuilder<PIXEL, GRAY>(cellLx, cellLy, radius, wrap) {
    // Build the mask corresponding to a square of passed radius
    GRAY *pix, *pixRev, *line, *lineEnd, *lineRev;
    this->m_mask = TRasterPT<GRAY>(cellLx, cellLy);

    // For each pixel in the lower-left quadrant, fill in the corresponding mask
    // value.
    // The other ones are filled by mirroring.
    int i, j;
    double lxHalf = 0.5 * cellLx, lyHalf = 0.5 * cellLy;
    int lxHalfI = tceil(lxHalf), lyHalfI = tceil(lyHalf);
    double val, addValX = 0.5 - lxHalf, addValY = 0.5 - lyHalf;

    for (i = 0; i < lyHalfI; ++i) {
      line    = this->m_mask->pixels(i),
      lineRev = this->m_mask->pixels(cellLy - i - 1);
      lineEnd = line + cellLx;
      for (j = 0, pix = line, pixRev = lineEnd - 1; j < lxHalfI;
           ++j, ++pix, --pixRev) {
        val = tcrop(radius - sqrt(sq(i + addValX) + sq(j + addValY)), 0.0, 1.0);
        *pix = *pixRev = val * GRAY::maxChannelValue;
      }

      memcpy(lineRev, line, cellLx * sizeof(GRAY));
    }
  }
};

}  // namespace mosaic

//****************************************************************************
//    Mosaic Fx
//****************************************************************************

class MosaicFx final : public TStandardRasterFx {
  FX_PLUGIN_DECLARATION(MosaicFx)

  TRasterFxPort m_input;
  TDoubleParamP m_size;
  TDoubleParamP m_distance;
  TPixelParamP m_bgcolor;
  TIntEnumParamP m_shape;

public:
  MosaicFx()
      : m_size(10.0)
      , m_distance(10.0)
      , m_bgcolor(TPixel32::Transparent)
      , m_shape(new TIntEnumParam(0, "Square")) {
    m_size->setMeasureName("fxLength");
    m_distance->setMeasureName("fxLength");
    bindParam(this, "size", m_size);
    bindParam(this, "distance", m_distance);
    bindParam(this, "bg_color", m_bgcolor);
    bindParam(this, "shape", m_shape);
    addInputPort("Source", m_input);
    m_size->setValueRange(0.0, (std::numeric_limits<double>::max)());
    m_distance->setValueRange(0.0, (std::numeric_limits<double>::max)());
    m_shape->addItem(1, "Round");
  }

  ~MosaicFx(){};

  bool doGetBBox(double frame, TRectD &bBox,
                 const TRenderSettings &info) override;

  void doDryCompute(TRectD &rect, double frame,
                    const TRenderSettings &ri) override;
  void doCompute(TTile &tile, double frame, const TRenderSettings &ri) override;
  int getMemoryRequirement(const TRectD &rect, double frame,
                           const TRenderSettings &info) override;

  bool canHandle(const TRenderSettings &info, double frame) override {
    // We'll return the handled affine only through handledAffine().
    if ((m_size->getValue(frame) + m_distance->getValue(frame)) == 0.0)
      return true;
    return false;
  }

  TAffine handledAffine(const TRenderSettings &info, double frame) override {
    // Return the default implementation: only the scale part of the affine
    // can be handled. Rotations and other distortions would need us have to
    // implement diagonal grid lines - and we don't want to!
    // Also, no translational component will be dealt for the same reason.
    TAffine scalePart(TRasterFx::handledAffine(info, frame));

    // Plus, we want to avoid dealing with antialiases even on plain orthogonal
    // lines! So, we ensure that the step size will be flattened to integer.
    double stepSize = m_size->getValue(frame) + m_distance->getValue(frame);
    double scale    = tceil(stepSize * scalePart.a11) / stepSize;

    return TScale(scale, scale);
  }
};

//-------------------------------------------------------------------

template <typename PIXEL>
void doMosaic(TRasterPT<PIXEL> ras, TRasterPT<PIXEL> cellsRas, int step,
              const TPoint &pos, TPixel32 bgcolor,
              mosaic::CellBuilder<PIXEL> &cellBuilder) {
  // Perform the mosaic's flattening operation on each grid cell.
  // Cells are identified directly as we traverse the raster.

  int lx = ras->getLx(), ly = ras->getLy();
  int wrap = ras->getWrap();

  int cellLx = cellsRas->getLx(), cellLy = cellsRas->getLy(),
      cellWrap = cellsRas->getWrap();
  int cellX, cellY;
  int x0, y0, x1, y1;

  PIXEL actualBgColor;
  pixelConvert(actualBgColor, bgcolor);

  ras->lock();
  cellsRas->lock();

  PIXEL *buffer   = ras->pixels(0);
  PIXEL *cellsBuf = cellsRas->pixels(0);
  for (cellY = 0; cellY < cellLy; ++cellY)
    for (cellX = 0; cellX < cellLx; ++cellX) {
      // Build the cell geometry
      x0 = pos.x + cellX * step, y0 = pos.y + cellY * step;
      x1 = x0 + step, y1 = y0 + step;

      // Retrieve the cell buffer position and its eventual adjustment
      int u0 = std::max(x0, 0), v0 = std::max(y0, 0);
      int u1 = std::min(x1, lx), v1 = std::min(y1, ly);

      PIXEL *cb = buffer + u0 + v0 * wrap;

      u0 -= x0, v0 -= y0, u1 -= x0, v1 -= y0;

      // Retrieve the cell color
      PIXEL *color = cellsBuf + cellX + cellY * cellWrap;

      PIXEL cellBGColor = actualBgColor;
      double mFactor    = color->m / (double)PIXEL::maxChannelValue;
      cellBGColor.r *= mFactor;
      cellBGColor.g *= mFactor;
      cellBGColor.b *= mFactor;
      cellBGColor.m *= mFactor;

      cellBuilder.doCell(cb, *color, cellBGColor, u0, v0, u1, v1);
    }

  cellsRas->unlock();
  ras->unlock();
}

//-------------------------------------------------------------------

bool MosaicFx::doGetBBox(double frame, TRectD &bBox,
                         const TRenderSettings &info) {
  // Remember: info.m_affine MUST NOT BE CONSIDERED in doGetBBox's
  // implementation

  // Retrieve the input bbox without applied affines.

  if (!(m_input.getFx() && m_input->doGetBBox(frame, bBox, info))) return false;

  // Now, the grid has a given step size; the fx result is almost the same,
  // except that it must be ceiled to the grid step.
  double step = m_size->getValue(frame) + m_distance->getValue(frame);
  if (!step) return true;
  bBox = ::gridAlign(bBox, TPointD(), step);

  return true;
}

//-------------------------------------------------------------------

void MosaicFx::doDryCompute(TRectD &rect, double frame,
                            const TRenderSettings &ri) {
  if (!m_input.isConnected()) return;

  double scale =
      ri.m_affine.a11;  // Legitimate due to handledAffine's implementation

  double stepD =
      (m_size->getValue(frame) + m_distance->getValue(frame)) * scale;
  int step = tround(stepD);
  if (step <= 0) return;

  TRectD bboxD;
  m_input->getBBox(frame, bboxD, ri);

  TRectD tileBoxD(::gridAlign(rect * bboxD, TPointD(), stepD));

  TRectD srcRectD(tround(tileBoxD.x0 / stepD), tround(tileBoxD.y0 / stepD),
                  tround(tileBoxD.x1 / stepD), tround(tileBoxD.y1 / stepD));

  int enlargement =
      (ri.m_quality == TRenderSettings::StandardResampleQuality)
          ? 1.0
          : (ri.m_quality == TRenderSettings::ImprovedResampleQuality)
                ? 2.0
                : (ri.m_quality == TRenderSettings::HighResampleQuality) ? 3.0
                                                                         : 0.0;

  srcRectD = srcRectD.enlarge(enlargement);

  TRenderSettings riLow(ri);
  riLow.m_affine = TScale(1.0 / stepD) * ri.m_affine;

  m_input->dryCompute(srcRectD, frame, riLow);
}

//-------------------------------------------------------------------

void MosaicFx::doCompute(TTile &tile, double frame, const TRenderSettings &ri) {
  if (!m_input.isConnected()) return;

  // ri's affine is the one that has already been carried due to upstream geom
  // fxs.
  // It is handled; so, we can transfer geometrical data directly.

  double scale =
      ri.m_affine.a11;  // Legitimate due to handledAffine's implementation

  double stepD =
      (m_size->getValue(frame) + m_distance->getValue(frame)) * scale;
  int step = stepD = tround(stepD);
  if (step <= 0) {
    // No blur will be done. The underlying fx may pass by.
    m_input->compute(tile, frame, ri);
    return;
  }

  double radius = 0.5 * tcrop(m_size->getValue(frame) * scale, 0.0, stepD);

  TPixel32 bgcolor = m_bgcolor->getPremultipliedValue(frame);

  // Build the tile rect.
  TDimension tileSize(tile.getRaster()->getSize());
  TRectD tileRectD(tile.m_pos, TDimensionD(tileSize.lx, tileSize.ly));

  // Observe that TRasterFx's documentation ensures
  // that passed tile's position is always integer, even if it's double-defined.

  // Retrieve the input bbox
  TRectD bboxD;
  m_input->getBBox(frame, bboxD, ri);

  // Build the grid geometry. We require that the source tile is enlarged to
  // cover
  // a full grid step.
  TRectD tileBoxD(::gridAlign(tileRectD * bboxD, TPointD(), stepD));

  // Now, we'll extract cell colors by computing tileBox with a res-reducing
  // affine.
  // So, the source tile will be
  TRect srcRect(tround(tileBoxD.x0 / stepD), tround(tileBoxD.y0 / stepD),
                tround(tileBoxD.x1 / stepD) - 1,
                tround(tileBoxD.y1 / stepD) - 1);

  // The calculated source rect must be enlarged by a small amount, since the
  // resample algrotihms add some transparency to the edges.
  int enlargement =
      (ri.m_quality == TRenderSettings::StandardResampleQuality)
          ? 1
          : (ri.m_quality == TRenderSettings::ImprovedResampleQuality)
                ? 2
                : (ri.m_quality == TRenderSettings::HighResampleQuality) ? 3
                                                                         : 0;
  assert(enlargement > 0);

  srcRect = srcRect.enlarge(enlargement);

  TRenderSettings riLow(ri);
  riLow.m_affine = TScale(1.0 / stepD) * ri.m_affine;

  // Compute the input tile.
  TRasterP srcRas(tile.getRaster()->create(srcRect.getLx(), srcRect.getLy()));
  TTile inTile(srcRas, TPointD(srcRect.x0, srcRect.y0));

  m_input->compute(inTile, frame, riLow);

  // Eliminate the enlargement performed above
  TRect r(enlargement, enlargement, srcRas->getLx() - 1 - enlargement,
          srcRas->getLy() - 1 - enlargement);
  srcRas = srcRas->extract(r);

  // Now, discriminate the tile's pixel size
  TRaster32P srcRas32(srcRas);
  TRaster64P srcRas64(srcRas);

  TPoint pos(tround(tileBoxD.x0 - tileRectD.x0),
             tround(tileBoxD.y0 - tileRectD.y0));
  if (srcRas32) {
    typedef mosaic::CellBuilder<TPixel32> cb;
    cb *cellsBuilder =
        (m_shape->getValue() == 0)
            ? (cb *)new mosaic::SquareBuilder<TPixel32, TPixelGR8>(
                  step, step, radius, tile.getRaster()->getWrap())
            : (cb *)new mosaic::CircleBuilder<TPixel32, TPixelGR8>(
                  step, step, radius, tile.getRaster()->getWrap());

    doMosaic<TPixel32>(tile.getRaster(), srcRas32, step, pos, bgcolor,
                       *cellsBuilder);

    delete cellsBuilder;
  } else if (srcRas64) {
    typedef mosaic::CellBuilder<TPixel64> cb;
    cb *cellsBuilder =
        (m_shape->getValue() == 0)
            ? (cb *)new mosaic::SquareBuilder<TPixel64, TPixelGR16>(
                  step, step, radius, tile.getRaster()->getWrap())
            : (cb *)new mosaic::CircleBuilder<TPixel64, TPixelGR16>(
                  step, step, radius, tile.getRaster()->getWrap());

    doMosaic<TPixel64>(tile.getRaster(), srcRas64, step, pos, bgcolor,
                       *cellsBuilder);

    delete cellsBuilder;
  } else
    assert(false);
}

//------------------------------------------------------------------

int MosaicFx::getMemoryRequirement(const TRectD &rect, double frame,
                                   const TRenderSettings &info) {
  double scale = info.m_affine.a11;
  double stepD =
      (m_size->getValue(frame) + m_distance->getValue(frame)) * scale;

  TRectD srcRect(rect.x0 / stepD, rect.y0 / stepD, rect.x1 / stepD,
                 rect.y1 / stepD);

  return TRasterFx::memorySize(srcRect, info.m_bpp);
}

//------------------------------------------------------------------

FX_PLUGIN_IDENTIFIER(MosaicFx, "mosaicFx");
