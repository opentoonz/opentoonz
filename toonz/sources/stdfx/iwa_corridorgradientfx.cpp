#include "iwa_corridorgradientfx.h"

#include "trop.h"
#include "tparamuiconcept.h"
#include "tspectrumparam.h"
#include "gradients.h"

#include <QPolygonF>

#include <array>
#include <algorithm>

//------------------------------------------------------------

Iwa_CorridorGradientFx::Iwa_CorridorGradientFx()
    : m_shape(new TIntEnumParam(0, "Quadrangle"))
    , m_innerColor(TPixel32::White)
    , m_outerColor(TPixel32::Black)
    , m_curveType(new TIntEnumParam()) {
  for (int inout = 0; inout < 2; inout++) {
    double size           = (inout == 0) ? 50. : 400.;
    std::string inout_str = (inout == 0) ? "_in" : "_out";

    for (int c = 0; c < 4; c++) {
      Qt::Corner corner = (Qt::Corner)c;
      TPointD basePos(1, 1);
      if (corner == Qt::TopLeftCorner || corner == Qt::BottomLeftCorner)
        basePos.x *= -1;
      if (corner == Qt::BottomLeftCorner || corner == Qt::BottomRightCorner)
        basePos.y *= -1;

      m_points[inout][corner] = basePos * size;

      m_points[inout][corner]->getX()->setMeasureName("fxLength");
      m_points[inout][corner]->getY()->setMeasureName("fxLength");

      std::string TB_str =
          (corner == Qt::TopLeftCorner || corner == Qt::TopRightCorner)
              ? "top"
              : "bottom";
      std::string LR_str =
          (corner == Qt::TopLeftCorner || corner == Qt::BottomLeftCorner)
              ? "_left"
              : "_right";

      bindParam(this, TB_str + LR_str + inout_str, m_points[inout][corner]);
    }
  }

  m_shape->addItem(1, "Circle");
  bindParam(this, "shape", m_shape);

  m_curveType->addItem(EaseInOut, "Ease In-Out");
  m_curveType->addItem(Linear, "Linear");
  m_curveType->addItem(EaseIn, "Ease In");
  m_curveType->addItem(EaseOut, "Ease Out");
  m_curveType->setDefaultValue(Linear);
  m_curveType->setValue(Linear);
  bindParam(this, "curveType", m_curveType);

  bindParam(this, "inner_color", m_innerColor);
  bindParam(this, "outer_color", m_outerColor);
}

//------------------------------------------------------------

bool Iwa_CorridorGradientFx::doGetBBox(double frame, TRectD &bBox,
                                       const TRenderSettings &ri) {
  bBox = TConsts::infiniteRectD;
  return true;
}

//------------------------------------------------------------

namespace {

QPointF toQPointF(const TPointD &p) { return QPointF(p.x, p.y); }

double WedgeProduct(const TPointD v, const TPointD w) {
  return v.x * w.y - v.y * w.x;
}

struct BilinearParam {
  TPointD p0, b1, b2, b3;
};

//------------------------------------------------------------

double getFactor(const TPointD &p, const BilinearParam &param,
                 const GradientCurveType type) {
  double t;
  TPointD q = p - param.p0;
  // Set up quadratic formula
  float A = WedgeProduct(param.b2, param.b3);
  float B = WedgeProduct(param.b3, q) - WedgeProduct(param.b1, param.b2);
  float C = WedgeProduct(param.b1, q);

  // Solve for v
  if (std::abs(A) < 0.001) {
    // Linear form
    t = -C / B;
  } else {
    // Quadratic form
    float discrim = B * B - 4 * A * C;
    t             = 0.5 * (-B - std::sqrt(discrim)) / A;
  }
  double factor;
  switch (type) {
  case Linear:
    factor = t;
    break;
  case EaseIn:
    factor = t * t;
    break;
  case EaseOut:
    factor = 1.0 - (1.0 - t) * (1.0 - t);
    break;
  case EaseInOut:
  default:
    factor = (-2 * t + 3) * (t * t);
    break;
  }
  return factor;
}

//------------------------------------------------------------

template <typename RASTER, typename PIXEL>
void doQuadrangleT(RASTER ras, TDimensionI dim, TPointD pos[2][4],
                   const TSpectrumT<PIXEL> &spectrum, GradientCurveType type) {
  auto buildPolygon = [&](QPolygonF &pol, Qt::Corner c1, Qt::Corner c2) {
    pol << toQPointF(pos[0][(int)c1]) << toQPointF(pos[1][(int)c1])
        << toQPointF(pos[1][(int)c2]) << toQPointF(pos[0][(int)c2]);
  };

  auto buildBilinearParam = [&](BilinearParam &bp, Qt::Corner c1,
                                Qt::Corner c2) {
    bp.p0 = pos[0][(int)c1];
    bp.b1 = pos[0][(int)c2] - pos[0][(int)c1];
    bp.b2 = pos[1][(int)c1] - pos[0][(int)c1];
    bp.b3 =
        pos[0][(int)c1] - pos[0][(int)c2] - pos[1][(int)c1] + pos[1][(int)c2];
  };

  std::array<QPolygonF, 4> polygons;
  std::array<BilinearParam, 4> params;

  // Top
  buildPolygon(polygons[0], Qt::TopLeftCorner, Qt::TopRightCorner);
  buildBilinearParam(params[0], Qt::TopLeftCorner, Qt::TopRightCorner);
  // Left
  buildPolygon(polygons[1], Qt::BottomLeftCorner, Qt::TopLeftCorner);
  buildBilinearParam(params[1], Qt::BottomLeftCorner, Qt::TopLeftCorner);
  // Bottom
  buildPolygon(polygons[2], Qt::BottomRightCorner, Qt::BottomLeftCorner);
  buildBilinearParam(params[2], Qt::BottomRightCorner, Qt::BottomLeftCorner);
  // Right
  buildPolygon(polygons[3], Qt::TopRightCorner, Qt::BottomRightCorner);
  buildBilinearParam(params[3], Qt::TopRightCorner, Qt::BottomRightCorner);

  QPolygonF innerPolygon;
  innerPolygon << toQPointF(pos[0][Qt::TopLeftCorner])
               << toQPointF(pos[0][Qt::TopRightCorner])
               << toQPointF(pos[0][Qt::BottomRightCorner])
               << toQPointF(pos[0][Qt::BottomLeftCorner]);

  ras->lock();
  for (int j = 0; j < ras->getLy(); j++) {
    PIXEL *pix    = ras->pixels(j);
    PIXEL *endPix = pix + ras->getLx();
    int i         = 0;
    while (pix < endPix) {
      QPointF p(i, j);

      double factor;
      bool found = false;
      for (int edge = 0; edge < 4; edge++) {
        if (polygons[edge].containsPoint(p, Qt::WindingFill)) {
          factor = getFactor(TPointD(i, j), params.at(edge), type);
          found  = true;
          break;
        }
      }
      if (!found) {
        if (innerPolygon.containsPoint(p, Qt::WindingFill))
          factor = 0.0;
        else
          factor = 1.0;
      }

      *pix++ = spectrum.getPremultipliedValue(factor);
      i++;
    }
  }
  ras->unlock();
}

//------------------------------------------------------------

template <typename RASTER, typename PIXEL>
void doCircleT(RASTER ras, TDimensionI dim, TPointD pos[2][4],
               const TSpectrumT<PIXEL> &spectrum, GradientCurveType type) {
  auto lerp = [](TPointD p1, TPointD p2, double f) {
    return p1 * (1 - f) + p2 * f;
  };
  auto bilinearPos = [&](TPointD uv, int inout) {
    return lerp(lerp(pos[inout][Qt::BottomLeftCorner],
                     pos[inout][Qt::BottomRightCorner], uv.x),
                lerp(pos[inout][Qt::TopLeftCorner],
                     pos[inout][Qt::TopRightCorner], uv.x),
                uv.y);
  };

  const int DIVNUM = 36;

  std::array<TPointD, DIVNUM> innerPos;
  std::array<TPointD, DIVNUM> outerPos;
  double tmpRadius = std::sqrt(2.0) / 2.0;
  for (int div = 0; div < DIVNUM; div++) {
    double angle = 2.0 * M_PI * (double)div / (double)DIVNUM;
    // circle position in uv coordinate
    TPointD uv(tmpRadius * std::cos(angle) + 0.5,
               tmpRadius * std::sin(angle) + 0.5);
    // compute inner and outer circle positions by bilinear interpolation
    // using uv coordinate values.
    innerPos[div] = bilinearPos(uv, 0);
    outerPos[div] = bilinearPos(uv, 1);
  }

  // - - - - - - - -

  auto buildPolygon = [&](QPolygonF &pol, int id1, int id2) {
    pol << toQPointF(innerPos[id2]) << toQPointF(outerPos[id2])
        << toQPointF(outerPos[id1]) << toQPointF(innerPos[id1]);
  };

  auto buildBilinearParam = [&](BilinearParam &bp, int id1, int id2) {
    bp.p0 = innerPos[id2];
    bp.b1 = innerPos[id1] - innerPos[id2];
    bp.b2 = outerPos[id2] - innerPos[id2];
    bp.b3 = innerPos[id2] - innerPos[id1] - outerPos[id2] + outerPos[id1];
  };
  std::array<QPolygonF, DIVNUM> polygons;
  std::array<BilinearParam, DIVNUM> params;
  QPolygonF innerPolygon;
  for (int div = 0; div < DIVNUM; div++) {
    int next_div = (div == DIVNUM - 1) ? 0 : div + 1;
    // create polygon and bilinear parameters for each piece surrounding the
    // circle
    buildPolygon(polygons[div], div, next_div);
    buildBilinearParam(params[div], div, next_div);
    // create inner circle polygon
    innerPolygon << toQPointF(innerPos[div]);
  }

  // - - - ok, ready to render

  ras->lock();

  for (int j = 0; j < ras->getLy(); j++) {
    PIXEL *pix    = ras->pixels(j);
    PIXEL *endPix = pix + ras->getLx();
    int i         = 0;
    while (pix < endPix) {
      QPointF p(i, j);
      double factor;
      bool found = false;
      for (int div = 0; div < DIVNUM; div++) {
        // check if the point is inside of the surrounding pieces
        if (polygons[div].containsPoint(p, Qt::WindingFill)) {
          // compute factor by invert bilinear interpolation
          factor = getFactor(TPointD(i, j), params.at(div), type);
          found  = true;
          break;
        }
      }
      if (!found) {
        if (innerPolygon.containsPoint(p, Qt::WindingFill))
          factor = 0.0;
        else
          factor = 1.0;
      }

      *pix++ = spectrum.getPremultipliedValue(factor);

      i++;
    }
  }
  ras->unlock();
}

};  // namespace

//------------------------------------------------------------

void Iwa_CorridorGradientFx::doCompute(TTile &tile, double frame,
                                       const TRenderSettings &ri) {
  if (!((TRaster32P)tile.getRaster()) && !((TRaster64P)tile.getRaster())) {
    throw TRopException("unsupported input pixel type");
  }

  // convert shape position to render region coordinate
  TPointD pos[2][4];
  TAffine aff = ri.m_affine;
  TDimensionI dimOut(tile.getRaster()->getLx(), tile.getRaster()->getLy());
  TPointD dimOffset((float)dimOut.lx / 2.0f, (float)dimOut.ly / 2.0f);
  for (int inout = 0; inout < 2; inout++) {
    for (int c = 0; c < 4; c++) {
      TPointD _point = m_points[inout][c]->getValue(frame);
      pos[inout][c]  = aff * _point -
                      (tile.m_pos + tile.getRaster()->getCenterD()) + dimOffset;
    }
  }

  std::vector<TSpectrum::ColorKey> colors = {
      TSpectrum::ColorKey(0, m_innerColor->getValue(frame)),
      TSpectrum::ColorKey(1, m_outerColor->getValue(frame))};
  TSpectrumParamP m_colors = TSpectrumParamP(colors);

  tile.getRaster()->clear();
  TRaster32P outRas32 = (TRaster32P)tile.getRaster();
  TRaster64P outRas64 = (TRaster64P)tile.getRaster();
  if (m_shape->getValue() == 0) {  // Quadrangle
    if (outRas32)
      doQuadrangleT<TRaster32P, TPixel32>(
          outRas32, dimOut, pos, m_colors->getValue(frame),
          (GradientCurveType)m_curveType->getValue());
    else if (outRas64)
      doQuadrangleT<TRaster64P, TPixel64>(
          outRas64, dimOut, pos, m_colors->getValue64(frame),
          (GradientCurveType)m_curveType->getValue());
  } else {  // m_shape == 1 : Circle
    if (outRas32)
      doCircleT<TRaster32P, TPixel32>(
          outRas32, dimOut, pos, m_colors->getValue(frame),
          (GradientCurveType)m_curveType->getValue());
    else if (outRas64)
      doCircleT<TRaster64P, TPixel64>(
          outRas64, dimOut, pos, m_colors->getValue64(frame),
          (GradientCurveType)m_curveType->getValue());
  }
}

//------------------------------------------------------------

void Iwa_CorridorGradientFx::getParamUIs(TParamUIConcept *&concepts,
                                         int &length) {
  concepts = new TParamUIConcept[length = 6];

  int vectorUiIdOffset = 2;

  std::array<Qt::Corner, 4> loopIds{Qt::TopLeftCorner, Qt::TopRightCorner,
                                    Qt::BottomRightCorner,
                                    Qt::BottomLeftCorner};

  for (int inout = 0; inout < 2; inout++) {
    concepts[inout].m_type = TParamUIConcept::QUAD;

    for (int c = 0; c < 4; c++) {
      Qt::Corner corner = loopIds[c];

      // quad ui
      concepts[inout].m_params.push_back(m_points[inout][(int)corner]);
      concepts[inout].m_label = (inout == 0) ? " In" : " Out";

      // vector ui
      if (inout == 0)
        concepts[vectorUiIdOffset + (int)corner].m_type =
            TParamUIConcept::VECTOR;
      concepts[vectorUiIdOffset + (int)corner].m_params.push_back(
          m_points[inout][(int)corner]);
    }
  }
}

//------------------------------------------------------------

FX_PLUGIN_IDENTIFIER(Iwa_CorridorGradientFx, "iwa_CorridorGradientFx");
