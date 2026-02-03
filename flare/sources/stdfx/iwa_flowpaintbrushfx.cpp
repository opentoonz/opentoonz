#include "iwa_flowpaintbrushfx.h"
#include "tparamuiconcept.h"
#include "tlevel.h"
#include "trop.h"

#include <random>

#include "tgl.h"
#include <QOpenGLFramebufferObject>
#include <QOffscreenSurface>
#include <QSurfaceFormat>
#include <QOpenGLContext>
#include <QImage>
#include <QOpenGLTexture>
#include <QColor>
#include <QPainter>

namespace {

inline double lerp(DoublePair range, double t) {
  return range.first * (1.0 - t) + range.second * t;
}

bool strokeStackGraterThan(const BrushStroke &stroke1,
                           const BrushStroke &stroke2) {
  return stroke1.stack > stroke2.stack;
}

}  // namespace

//------------------------------------------------------------
// obtain raster data of brush tips ブラシタッチのラスターデータを取得
void Iwa_FlowPaintBrushFx::getBrushRasters(std::vector<TRasterP> &brushRasters,
                                           TDimension &b_size, int &lastFrame,
                                           TTile &tile,
                                           const TRenderSettings &ri) {
  // ブラシテクスチャ情報
  TPointD b_offset;
  const TFxTimeRegion &tr = m_brush->getTimeRegion();
  lastFrame               = tr.getLastFrame() + 1;
  TLevelP partLevel       = new TLevel();
  partLevel->setName(m_brush->getAlias(0, ri));

  // The particles offset must be calculated without considering the
  // affine's translational component
  TRenderSettings riZero(ri);
  riZero.m_affine.a13 = riZero.m_affine.a23 = 0;

  // Calculate the bboxes union
  TRectD brushBox;
  for (int t = 0; t < lastFrame; ++t) {
    TRectD inputBox;
    m_brush->getBBox(t, inputBox, riZero);
    brushBox += inputBox;
  }
  if (brushBox.isEmpty()) {
    lastFrame = 0;
    return;
  }
  if (brushBox == TConsts::infiniteRectD) brushBox *= ri.m_cameraBox;

  b_size.lx = (int)brushBox.getLx() + 1;
  b_size.ly = (int)brushBox.getLy() + 1;
  b_offset  = TPointD(0.5 * (brushBox.x0 + brushBox.x1),
                      0.5 * (brushBox.y0 + brushBox.y1));

  for (int t = 0; t < lastFrame; ++t) {
    TRasterP ras;
    std::string alias;
    TRasterImageP rimg;
    rimg = partLevel->frame(t);
    if (rimg) {
      ras = rimg->getRaster();
    } else {
      alias = "BRUSH: " + m_brush->getAlias(t, ri);
      rimg  = TImageCache::instance()->get(alias, false);
      if (rimg) {
        ras = rimg->getRaster();

        // Check that the raster resolution is sufficient for our purposes
        if (ras->getLx() < b_size.lx || ras->getLy() < b_size.ly)
          ras = 0;
        else
          b_size = TDimension(ras->getLx(), ras->getLy());
      }
    }
    if (!ras) {
      TTile auxTile;
      TRenderSettings auxRi(ri);
      auxRi.m_bpp = 32;
      m_brush->allocateAndCompute(auxTile, brushBox.getP00(), b_size, 0, t,
                                  auxRi);
      ras = auxTile.getRaster();
      addRenderCache(alias, TRasterImageP(ras));
    }
    if (ras) brushRasters.push_back(ras);
  }
}

//------------------------------------------------------------

template <typename RASTER, typename PIXEL>
void Iwa_FlowPaintBrushFx::setFlowTileToBuffer(const RASTER flowRas,
                                               double2 *buf) {
  double2 *buf_p = buf;
  for (int j = 0; j < flowRas->getLy(); j++) {
    PIXEL *pix = flowRas->pixels(j);
    for (int i = 0; i < flowRas->getLx(); i++, pix++, buf_p++) {
      double val = double(pix->r) / double(PIXEL::maxChannelValue);
      (*buf_p).x = val * 2.0 - 1.0;
      val        = double(pix->g) / double(PIXEL::maxChannelValue);
      (*buf_p).y = val * 2.0 - 1.0;
    }
  }
}

//------------------------------------------------------------

template <typename RASTER, typename PIXEL>
void Iwa_FlowPaintBrushFx::setAreaTileToBuffer(const RASTER areaRas,
                                               double *buf) {
  double *buf_p = buf;
  for (int j = 0; j < areaRas->getLy(); j++) {
    PIXEL *pix = areaRas->pixels(j);
    for (int i = 0; i < areaRas->getLx(); i++, pix++, buf_p++) {
      (*buf_p) = double(pix->m) / double(PIXEL::maxChannelValue);
    }
  }
}

//------------------------------------------------------------

template <typename RASTER, typename PIXEL>
void Iwa_FlowPaintBrushFx::setColorTileToBuffer(const RASTER colorRas,
                                                colorRGBA *buf) {
  colorRGBA *buf_p = buf;
  for (int j = 0; j < colorRas->getLy(); j++) {
    PIXEL *pix = colorRas->pixels(j);
    for (int i = 0; i < colorRas->getLx(); i++, pix++, buf_p++) {
      (*buf_p).r = double(pix->r) / double(PIXEL::maxChannelValue);
      (*buf_p).g = double(pix->g) / double(PIXEL::maxChannelValue);
      (*buf_p).b = double(pix->b) / double(PIXEL::maxChannelValue);
      (*buf_p).a = double(pix->m) / double(PIXEL::maxChannelValue);
    }
  }
}

//------------------------------------------------------------
template <typename RASTER, typename PIXEL>
void Iwa_FlowPaintBrushFx::setOutRaster(const RASTER outRas, double *buf) {
  double *buf_p = buf;
  for (int j = 0; j < outRas->getLy(); j++) {
    PIXEL *pix = outRas->pixels(j);
    for (int i = 0; i < outRas->getLx(); i++, pix++, buf_p++) {
      typename PIXEL::Channel val =
          (typename PIXEL::Channel)(*buf_p * double(PIXEL::maxChannelValue));
      pix->r = val;
      pix->g = val;
      pix->b = val;
      pix->m = PIXEL::maxChannelValue;
    }
  }
}

//------------------------------------------------------------
Iwa_FlowPaintBrushFx::Iwa_FlowPaintBrushFx()
    : m_h_density(10.0)
    , m_v_density(10.0)
    , m_pos_randomness(1.0)
    , m_pos_wobble(0.0)
    , m_tip_width(DoublePair(0.02, 0.05))
    , m_tip_length(DoublePair(0.08, 0.2))
    , m_tip_alpha(DoublePair(0.8, 1.0))
    , m_tip_joints(3)
    , m_bidirectional(true)
    , m_width_randomness(0.0)
    , m_length_randomness(0.0)
    , m_angle_randomness(0.0)
    , m_sustain_width_to_skew(0.0)
    , m_anti_jaggy(false)
    , m_origin_pos(TPointD(0.0, 0.0))
    , m_horizontal_pos(TPointD(100.0, 0.0))
    , m_vertical_pos(TPointD(0.0, 100.0))
    , m_curve_point(TPointD(0.0, 0.0))
    , m_fill_gap_size(0.0)
    , m_reference_frame(0.0)
    , m_reference_prevalence(0.0)
    , m_random_seed(1)
    , m_sortBy(new TIntEnumParam(NoSort, "None")) {
  addInputPort("Brush", m_brush);
  addInputPort("Flow", m_flow);
  addInputPort("Area", m_area);
  addInputPort("Color", m_color);

  bindParam(this, "h_density", m_h_density);
  bindParam(this, "v_density", m_v_density);
  bindParam(this, "pos_randomness", m_pos_randomness);
  bindParam(this, "pos_wobble", m_pos_wobble);
  bindParam(this, "tip_width", m_tip_width);
  bindParam(this, "tip_length", m_tip_length);
  bindParam(this, "tip_alpha", m_tip_alpha);
  bindParam(this, "tip_joints", m_tip_joints);
  bindParam(this, "bidirectional", m_bidirectional);

  bindParam(this, "width_randomness", m_width_randomness);
  bindParam(this, "length_randomness", m_length_randomness);
  bindParam(this, "angle_randomness", m_angle_randomness);
  bindParam(this, "sustain_width_to_skew", m_sustain_width_to_skew);
  bindParam(this, "anti_jaggy", m_anti_jaggy);

  bindParam(this, "origin_pos", m_origin_pos);
  bindParam(this, "horizontal_pos", m_horizontal_pos);
  bindParam(this, "vertical_pos", m_vertical_pos);
  bindParam(this, "curve_point", m_curve_point);
  bindParam(this, "fill_gap_size", m_fill_gap_size);

  bindParam(this, "reference_frame", m_reference_frame);
  bindParam(this, "reference_prevalence", m_reference_prevalence);

  bindParam(this, "random_seed", m_random_seed);
  bindParam(this, "sort_by", m_sortBy);

  m_h_density->setValueRange(1.0, 300.0);
  m_v_density->setValueRange(1.0, 300.0);
  m_pos_randomness->setValueRange(0.0, 2.0);
  m_pos_wobble->setValueRange(0.0, 1.0);
  m_tip_width->getMin()->setValueRange(0.0, 1.0);
  m_tip_width->getMax()->setValueRange(0.0, 1.0);
  m_tip_length->getMin()->setValueRange(0.0, 1.0);
  m_tip_length->getMax()->setValueRange(0.0, 1.0);
  m_tip_alpha->getMin()->setValueRange(0.0, 1.0);
  m_tip_alpha->getMax()->setValueRange(0.0, 1.0);
  m_tip_joints->setValueRange(0, 20);

  m_width_randomness->setValueRange(0.0, 0.9);
  m_length_randomness->setValueRange(0.0, 0.9);
  m_angle_randomness->setValueRange(0.0, 180.0);  // degree

  m_sustain_width_to_skew->setValueRange(0.0, 1.0);

  m_origin_pos->getX()->setMeasureName("fxLength");
  m_origin_pos->getY()->setMeasureName("fxLength");
  m_horizontal_pos->getX()->setMeasureName("fxLength");
  m_horizontal_pos->getY()->setMeasureName("fxLength");
  m_vertical_pos->getX()->setMeasureName("fxLength");
  m_vertical_pos->getY()->setMeasureName("fxLength");
  m_curve_point->getX()->setValueRange(-0.5, 0.5);
  m_curve_point->getY()->setValueRange(-0.5, 0.5);

  m_fill_gap_size->setMeasureName("fxLength");
  m_fill_gap_size->setValueRange(0.0, 50.0);

  m_reference_frame->setValueRange(0, (std::numeric_limits<double>::max)());
  m_reference_prevalence->setValueRange(0.0, 1.0);
  m_random_seed->setValueRange(0, (std::numeric_limits<int>::max)());
  m_sortBy->addItem(Smaller, "Size - Smaller on Top");
  m_sortBy->addItem(Larger, "Size - Larger on Top");
  m_sortBy->addItem(Darker, "Brightness - Darker on Top");
  m_sortBy->addItem(Brighter, "Brightness - Brighter on Top");
  m_sortBy->addItem(Random, "Random");

  // Version 1 (development version) : strokes are placed upward direction if
  // there is no Flow input.
  // Version 2: strokes are directed parallel to the
  // vertical vector of the parallelogram if there is no Flow input.
  setFxVersion(2);
}

//------------------------------------------------------------
bool Iwa_FlowPaintBrushFx::doGetBBox(double frame, TRectD &bBox,
                                     const TRenderSettings &info) {
  if (!m_brush.isConnected()) {
    bBox = TRectD();
    return false;
  }
  TPointD origin_pos   = m_origin_pos->getValue(frame);
  TPointD horiz_pos    = m_horizontal_pos->getValue(frame);
  TPointD vert_pos     = m_vertical_pos->getValue(frame);
  TPointD opposite_pos = horiz_pos + vert_pos - origin_pos;
  bBox = boundingBox(origin_pos, horiz_pos, vert_pos, opposite_pos);
  return true;
}

//--------------------------------------------------------------
double Iwa_FlowPaintBrushFx::getSizePixelAmount(const double val,
                                                const TAffine affine) {
  /*--- Convert to vector --- */
  TPointD vect;
  vect.x = val;
  vect.y = 0.0;
  /*--- Apply geometrical transformation ---*/
  // For the following lines I referred to lines 586-592 of
  // sources/stdfx/motionblurfx.cpp
  TAffine aff(affine);
  aff.a13 = aff.a23 = 0; /* ignore translation */
  vect              = aff * vect;
  /*--- return the length of the vector ---*/
  return sqrt(vect.x * vect.x + vect.y * vect.y);
}
//------------------------------------------------------------

FlowPaintBrushFxParam Iwa_FlowPaintBrushFx::getParam(
    TTile &tile, double frame, const TRenderSettings &ri) {
  FlowPaintBrushFxParam p;

  TAffine aff          = ri.m_affine;
  p.origin_pos         = aff * m_origin_pos->getValue(frame);
  p.horiz_pos          = aff * m_horizontal_pos->getValue(frame);
  p.vert_pos           = aff * m_vertical_pos->getValue(frame);
  TPointD opposite_pos = p.horiz_pos + p.vert_pos - p.origin_pos;
  p.bbox   = boundingBox(p.origin_pos, p.horiz_pos, p.vert_pos, opposite_pos);
  p.dim.lx = (int)p.bbox.getLx() + 1;
  p.dim.ly = (int)p.bbox.getLy() + 1;
  p.origin_pos -= p.bbox.getP00();
  p.horiz_pos -= p.bbox.getP00();
  p.vert_pos -= p.bbox.getP00();

  p.fill_gap_size =
      (int)getSizePixelAmount(m_fill_gap_size->getValue(frame), aff);

  p.h_density      = m_h_density->getValue(frame);
  p.v_density      = m_v_density->getValue(frame);
  p.pos_randomness = m_pos_randomness->getValue(frame);
  p.pos_wobble     = m_pos_wobble->getValue(frame);

  p.random_seed   = m_random_seed->getValue();
  p.tipLength     = m_tip_length->getValue(frame);
  p.tipWidth      = m_tip_width->getValue(frame);
  p.tipAlpha      = m_tip_alpha->getValue(frame);
  p.width_random  = m_width_randomness->getValue(frame);
  p.length_random = m_length_randomness->getValue(frame);
  p.angle_random  = m_angle_randomness->getValue(frame);

  p.reso       = m_tip_joints->getValue();
  p.anti_jaggy = m_anti_jaggy->getValue();

  p.hVec      = p.horiz_pos - p.origin_pos;
  p.vVec      = p.vert_pos - p.origin_pos;
  p.vVec_unit = double2{normalize(p.vVec).x, normalize(p.vVec).y};

  p.brushAff = TAffine(p.hVec.x, p.vVec.x, p.origin_pos.x, p.hVec.y, p.vVec.y,
                       p.origin_pos.y);
  return p;
}

//------------------------------------------------------------

void Iwa_FlowPaintBrushFx::fillGapByDilateAndErode(double *buf,
                                                   const TDimension &dim,
                                                   const int fill_gap_size) {
  TRasterGR8P tmp_buf_ras = TRasterGR8P(dim.lx * dim.ly * sizeof(double), 1);
  tmp_buf_ras->lock();
  double *tmp_buf = (double *)tmp_buf_ras->getRawData();

  // dilate, then erode
  for (int mode = 0; mode < 2; mode++) {
    for (int i = 0; i < fill_gap_size; i++) {
      double *fromBuf = (i % 2 == 0) ? buf : tmp_buf;
      double *toBuf   = (i % 2 == 0) ? tmp_buf : buf;

      double *to_p  = toBuf;
      double *frm_p = fromBuf;
      for (int y = 0; y < dim.ly; y++) {
        double *n_up =
            (y == 0) ? &fromBuf[y * dim.lx] : &fromBuf[(y - 1) * dim.lx];
        double *n_dn = (y == dim.ly - 1) ? &fromBuf[y * dim.lx]
                                         : &fromBuf[(y + 1) * dim.lx];
        for (int x = 0; x < dim.lx; x++, n_up++, n_dn++, to_p++, frm_p++) {
          if (mode == 0) {
            *to_p = std::max(*frm_p, *n_up);
            *to_p = std::max(*to_p, *n_dn);
            if (x != 0) *to_p = std::max(*to_p, *(frm_p - 1));
            if (x != dim.lx - 1) *to_p = std::max(*to_p, *(frm_p + 1));
          } else {
            *to_p = std::min(*frm_p, *n_up);
            *to_p = std::min(*to_p, *n_dn);
            if (x != 0) *to_p = std::min(*to_p, *(frm_p - 1));
            if (x != dim.lx - 1) *to_p = std::min(*to_p, *(frm_p + 1));
          }
        }
      }
    }
  }

  // 奇数回のフィルタリングの場合、計算結果はtmp_bufに入っているのでbufにコピーする
  if (fill_gap_size % 2 == 1) {
    memcpy(buf, tmp_buf, dim.lx * dim.ly * sizeof(double));
  }

  tmp_buf_ras->unlock();
}

//------------------------------------------------------------
void Iwa_FlowPaintBrushFx::computeBrushVertices(
    QVector<BrushVertex> &brushVertices, QList<BrushStroke> &brushStrokes,
    FlowPaintBrushFxParam &p, TTile &tile, double frame,
    const TRenderSettings &ri) {
  // 固定するタッチ：基準フレームのArea、Colorを用いて生成、カレントフレームのFlowを用いて流す
  // 動かすタッチ：Area、Color、Flowすべてカレントフレームを用いる。
  int referenceFrame = (int)std::round(m_reference_frame->getValue(frame)) - 1;
  double reference_prevalence = m_reference_prevalence->getValue(frame);
  bool bidirectional          = m_bidirectional->getValue();

  FlowPaintBrushFxParam pivP =
      getParam(tile, referenceFrame, ri);  // TODO: ここ、二度手間防げる
  pivP.lastFrame                  = p.lastFrame;
  FlowPaintBrushFxParam *param[2] = {&p, &pivP};

  double ref_frame[2]     = {frame, (double)referenceFrame};
  double2 *flow_buf[2]    = {nullptr};
  double *area_buf[2]     = {nullptr};
  colorRGBA *color_buf[2] = {nullptr};
  TRasterGR8P flow_buf_ras[2], area_buf_ras[2], color_buf_ras[2];

  TRaster32P ras32 = tile.getRaster();
  TRaster64P ras64 = tile.getRaster();

  // それぞれの参照画像を取得
  for (int f = 0; f < 2; f++) {
    // Referenceフレームはフレームが-1又はprevalenceが０の場合計算しない
    int tmp_f = ref_frame[f];
    if (f == 1 && (tmp_f < 0 || reference_prevalence == 0.0)) continue;

    // まず、Flowを計算
    if (m_flow.isConnected()) {
      // obtain Flow memory buffer (XY)
      TTile flowTile;
      m_flow->allocateAndCompute(flowTile, param[f]->bbox.getP00(),
                                 param[f]->dim, tile.getRaster(), tmp_f, ri);
      // allocate buffer
      flow_buf_ras[f] =
          TRasterGR8P(param[f]->dim.lx * param[f]->dim.ly * sizeof(double2), 1);
      flow_buf_ras[f]->lock();
      flow_buf[f] = (double2 *)flow_buf_ras[f]->getRawData();
      if (ras32)
        setFlowTileToBuffer<TRaster32P, TPixel32>(flowTile.getRaster(),
                                                  flow_buf[f]);
      else if (ras64)
        setFlowTileToBuffer<TRaster64P, TPixel64>(flowTile.getRaster(),
                                                  flow_buf[f]);
    }
    // カレントフレームはprevalenceが１の場合Flowのみ計算
    if (f == 0 && reference_prevalence == 1.0) continue;
    // AreaとColorを計算
    if (m_area.isConnected()) {
      TTile areaTile;
      m_area->allocateAndCompute(areaTile, param[f]->bbox.getP00(),
                                 param[f]->dim, tile.getRaster(), tmp_f, ri);
      // allocate buffer
      area_buf_ras[f] =
          TRasterGR8P(param[f]->dim.lx * param[f]->dim.ly * sizeof(double), 1);
      area_buf_ras[f]->lock();
      area_buf[f] = (double *)area_buf_ras[f]->getRawData();
      if (ras32)
        setAreaTileToBuffer<TRaster32P, TPixel32>(areaTile.getRaster(),
                                                  area_buf[f]);
      else if (ras64)
        setAreaTileToBuffer<TRaster64P, TPixel64>(areaTile.getRaster(),
                                                  area_buf[f]);

      // 間をうめる
      if (param[f]->fill_gap_size > 0)
        fillGapByDilateAndErode(area_buf[f], param[f]->dim,
                                param[f]->fill_gap_size);
    }
    if (m_color.isConnected()) {
      TTile colorTile;
      m_color->allocateAndCompute(colorTile, param[f]->bbox.getP00(),
                                  param[f]->dim, tile.getRaster(), tmp_f, ri);
      // allocate buffer
      color_buf_ras[f] = TRasterGR8P(
          param[f]->dim.lx * param[f]->dim.ly * sizeof(colorRGBA), 1);
      color_buf_ras[f]->lock();
      color_buf[f] = (colorRGBA *)color_buf_ras[f]->getRawData();
      if (ras32)
        setColorTileToBuffer<TRaster32P, TPixel32>(colorTile.getRaster(),
                                                   color_buf[f]);
      else if (ras64)
        setColorTileToBuffer<TRaster64P, TPixel64>(colorTile.getRaster(),
                                                   color_buf[f]);
    }
  }

  enum FRAMETYPE { CURRENT = 0, REFERENCE };

  auto getFlow = [&](TPointD pos, FRAMETYPE fType) {
    if (!flow_buf[fType]) {
      if (getFxVersion() == 1)
        return double2{0, 1};
      else
        return param[CURRENT]->vVec_unit;
    }
    int u = (int)std::round(pos.x);
    int v = (int)std::round(pos.y);
    if (u < 0 || u >= param[fType]->dim.lx || v < 0 ||
        v >= param[fType]->dim.ly) {
      if (getFxVersion() == 1)
        return double2{0, 1};
      else
        return param[CURRENT]->vVec_unit;
    }
    return flow_buf[fType][v * param[fType]->dim.lx + u];
  };
  auto getArea = [&](TPointD pos, FRAMETYPE fType) {
    if (!area_buf[fType]) return 1.0;
    int u = (int)std::round(pos.x);
    int v = (int)std::round(pos.y);
    if (u < 0 || u >= param[fType]->dim.lx || v < 0 ||
        v >= param[fType]->dim.ly)
      return 0.0;
    return area_buf[fType][v * param[fType]->dim.lx + u];
  };
  auto getColor = [&](TPointD pos, FRAMETYPE fType) {
    if (!color_buf[fType]) return colorRGBA{1.0, 1.0, 1.0, 1.0};
    int u = (int)std::round(pos.x);
    int v = (int)std::round(pos.y);
    if (u < 0 || u >= param[fType]->dim.lx || v < 0 ||
        v >= param[fType]->dim.ly)
      return colorRGBA{1.0, 1.0, 1.0, 0.0};
    return color_buf[fType][v * param[fType]->dim.lx + u];
  };

  // ランダムの設定
  std::mt19937_64 mt, mt_cur;  // , mt_ref;
  mt.seed(p.random_seed);
  mt_cur.seed(
      (unsigned int)(p.random_seed +
                     (int)std::round(frame)));  // フレーム毎にバラつかせる動き
  std::uniform_real_distribution<> random01(0.0, 1.0);
  std::uniform_int_distribution<> random_textureId(0, p.lastFrame - 1);
  std::uniform_real_distribution<> random_plusminus1(-1.0, 1.0);

  // タッチの頂点座標を登録していく
  double v_incr        = 1.0 / p.v_density;
  double h_incr        = 1.0 / p.h_density;
  double v_base_pos    = v_incr * 0.5;
  double segmentAmount = (double)p.reso * 2 - 1;

  int id = 0;  // Mapのキーになる通し番号
  for (int v = 0; v < (int)p.v_density; v++, v_base_pos += v_incr) {
    double h_base_pos = h_incr * 0.5;
    for (int h = 0; h < (int)p.h_density; h++, h_base_pos += h_incr, id++) {
      BrushStroke brushStroke;

      // referenceかカレントか
      FRAMETYPE frameType =
          (referenceFrame >= 0 && random01(mt) <= reference_prevalence)
              ? REFERENCE
              : CURRENT;

      double pos_x = h_base_pos;
      double pos_y = v_base_pos;
      if (frameType == CURRENT) {
        pos_x += (random01(mt) - 0.5) * p.pos_randomness * h_incr;
        pos_y += (random01(mt) - 0.5) * p.pos_randomness * v_incr;
      } else {  // REFERENCE case
        pos_x += (random01(mt) - 0.5) * pivP.pos_randomness * h_incr;
        pos_y += (random01(mt) - 0.5) * pivP.pos_randomness * v_incr;
      }
      if (pos_x < 0.0 || pos_x > 1.0 || pos_y < 0.0 || pos_y > 1.0) {
        mt.discard(7);  // ここ、この後のランダム生成回数分が入る
        mt_cur.discard(2);
        continue;
      }

      //  Area値を拾う座標
      TPointD areaSamplePos =
          param[frameType]->brushAff * TPointD(pos_x, pos_y);
      // 生成範囲の参照画像の輝度を取得
      double areaVal = getArea(areaSamplePos, frameType);
      // 発生するかしないか
      if (random01(mt) > areaVal) {
        mt.discard(6);  // ここにも、この後のランダム生成回数分が入る
        mt_cur.discard(2);
        continue;
      }

      double2 wobble;
      wobble.x = (random01(mt_cur) - 0.5) * p.pos_wobble * h_incr;
      wobble.y = (random01(mt_cur) - 0.5) * p.pos_wobble * v_incr;
      // 現在のセグメントのレンダリング座標
      brushStroke.originPos = TPointD(pos_x + wobble.x, pos_y + wobble.y);

      brushStroke.color = getColor(areaSamplePos, frameType);
      double alpha      = lerp(param[frameType]->tipAlpha, areaVal);
      // premultiply なのでRGB値にもかける
      brushStroke.color.r *= alpha;
      brushStroke.color.g *= alpha;
      brushStroke.color.b *= alpha;
      brushStroke.color.a *= alpha;

      brushStroke.length =
          lerp(param[frameType]->tipLength, areaVal) *
          (1.0 + random_plusminus1(mt) * param[frameType]->length_random);
      brushStroke.widthHalf =
          lerp(param[frameType]->tipWidth, areaVal) *
          (1.0 + random_plusminus1(mt) * param[frameType]->width_random) * 0.5;

      if (p.anti_jaggy) brushStroke.widthHalf *= 3.0;

      brushStroke.angle =
          random_plusminus1(mt) * param[frameType]->angle_random;
      TAffine flow_rot = TRotation(brushStroke.angle);

      brushStroke.textureId = random_textureId(mt);
      brushStroke.randomVal = random01(mt);

      bool inv           = random01(mt) < 0.5;
      brushStroke.invert = (bidirectional) ? inv : false;

      // まず、芯になる部分の座標を計算する
      QVector<TPointD> centerPosHalf[2];

      double segLen =
          brushStroke.length /
          segmentAmount;  // セグメントひとつ分の長さ。（ブラシ座標系）

      bool tooCurve[2] = {false, false};
      TPointD originFlow;
      // 前後方向
      for (int dir = 0; dir < 2; dir++) {
        // curPosを初期位置に
        TPointD curPos(brushStroke.originPos);
        TPointD preFlowUV;
        TPointD startFlow;
        for (int s = 0; s < param[frameType]->reso; s++) {
          // 　現在のセグメントのレンダリング座標
          TPointD samplePos = param[CURRENT]->brushAff * curPos;

          // レンダリング座標から流れベクトル場を取得
          double2 tmp_flow = getFlow(samplePos, CURRENT);

          // 　流れベクトル場をブラシ座標系に変換
          TPointD flow_uv =
              param[CURRENT]->brushAff.inv() * TPointD(tmp_flow.x, tmp_flow.y) -
              param[CURRENT]->brushAff.inv() * TPointD(0., 0.);
          if (s == 0 && dir == 0) originFlow = flow_uv;

          // ベクトルを正規化
          flow_uv = normalize(flow_uv);

          // 戻る方向のときはベクトルを反転
          if (dir == 1) flow_uv = -flow_uv;

          // 流れの方向を回転
          flow_uv = flow_rot * flow_uv;

          if (s != 0) {
            double dot = flow_uv.x * preFlowUV.x + flow_uv.y * preFlowUV.y;
            if (dot < 0.0) flow_uv = -flow_uv;

            double dotVsStart =
                startFlow.x * flow_uv.x + startFlow.y * flow_uv.y;
            if (dotVsStart < 0.3) {
              tooCurve[dir] = true;
              break;
            }
          }
          preFlowUV = flow_uv;

          if (s == 0) startFlow = flow_uv;

          // ベクトルをセグメント長分のばし、次の点とする。最初のセグメントは半分の長さ。
          TPointD nextPos = curPos + flow_uv * segLen * ((s == 0) ? 0.5 : 1.0);

          // 頂点を格納
          centerPosHalf[dir].push_back(nextPos);

          // curPosを進める
          curPos = nextPos;
        }
      }

      if (tooCurve[0] && tooCurve[1]) continue;
      // 両方滑らかな場合
      if (!tooCurve[0] && !tooCurve[1]) {
        brushStroke.centerPos = centerPosHalf[1];
        for (auto pos : centerPosHalf[0]) brushStroke.centerPos.push_front(pos);
      }
      // 片方だけ急カーブの場合、滑らかな方のカーブを反対側に反転して延長する
      else {
        int OkId = (tooCurve[0]) ? 1 : 0;
        TPointD origin(pos_x, pos_y);
        TPointD okInitialVec = centerPosHalf[OkId].at(0) - origin;
        double thetaDeg = std::atan2(okInitialVec.y, okInitialVec.x) / M_PI_180;
        TAffine reflectAff = TTranslation(origin) * TRotation(thetaDeg) *
                             TScale(-1.0, 1.0) * TRotation(-thetaDeg) *
                             TTranslation(-origin);
        brushStroke.centerPos = centerPosHalf[OkId];
        for (auto pos : centerPosHalf[OkId]) {
          TPointD refPos = reflectAff * pos;
          brushStroke.centerPos.push_front(reflectAff * pos);
        }
      }

      // ここで、基準フレームの流れの向きに従ってテクスチャの向きをそろえてみる
      if (referenceFrame >= 0) {
        TPointD referenceSamplePos =
            param[REFERENCE]->brushAff * TPointD(pos_x, pos_y);
        double2 reference_flow = getFlow(referenceSamplePos, REFERENCE);
        // 　流れベクトル場をブラシ座標系に変換
        TPointD reference_flow_uv =
            param[REFERENCE]->brushAff.inv() *
                TPointD(reference_flow.x, reference_flow.y) -
            param[REFERENCE]->brushAff.inv() * TPointD(0., 0.);
        // 内積を取り、反転するか判断する
        double dot = originFlow.x * reference_flow_uv.x +
                     originFlow.y * reference_flow_uv.y;
        if (dot < 0.0) {
          brushStroke.invert = !brushStroke.invert;
        }
      }
      // 登録
      brushStrokes.append(brushStroke);
    }
  }

  // ラスターメモリ解放
  for (int f = 0; f < 2; f++) {
    if (flow_buf[f]) flow_buf_ras[f]->unlock();
    if (area_buf[f]) area_buf_ras[f]->unlock();
    if (color_buf[f]) color_buf_ras[f]->unlock();
  }

  // ソート
  StackMode stackMode = (StackMode)m_sortBy->getValue();
  if (stackMode != NoSort) {
    // 大きい方が配列で前、すなわち先（下）に描かれるようにする
    for (auto &stroke : brushStrokes) {
      switch (stackMode) {
      case Smaller:
        stroke.stack = stroke.length * stroke.widthHalf;
        break;
      case Larger:
        stroke.stack = -stroke.length * stroke.widthHalf;
        break;
      // Value = 0.3R 0.59G 0.11B
      case Darker:
        stroke.stack = 0.3 * stroke.color.r + 0.59 * stroke.color.g +
                       0.11 * stroke.color.b;
        break;
      case Brighter:
        stroke.stack = -(0.3 * stroke.color.r + 0.59 * stroke.color.g +
                         0.11 * stroke.color.b);
        break;
      case Random:
        stroke.stack = stroke.randomVal;
        break;
      }
    }
    std::sort(brushStrokes.begin(), brushStrokes.end(), strokeStackGraterThan);
  }

  // タッチをせん断に対してこっち向けて太くする割合
  double sustain_width_to_skew = m_sustain_width_to_skew->getValue(frame);
  TPointD curve_point          = m_curve_point->getValue(frame);
  // 頂点座標を登録していく
  for (auto stroke : brushStrokes) {
    int jointCount = stroke.centerPos.size();

    // 頂点位置をカーブさせてゆがめるテスト
    if (curve_point != TPointD()) {
      for (int s = 0; s < stroke.centerPos.size(); s++) {
        TPointD p = stroke.centerPos[s];
        TPointD A = (1.0 - p.x) * (1.0 - p.x) * TPointD(0.0, 0.5) +
                    2.0 * (1.0 - p.x) * p.x *
                        TPointD(0.5 + curve_point.x, 0.5 + curve_point.y) +
                    p.x * p.x * TPointD(1.0, 0.5);
        stroke.centerPos[s] = (1.0 - p.y) * (1.0 - p.y) * TPointD(p.x, 0.0) +
                              2.0 * (1.0 - p.y) * p.y * TPointD(A.x, A.y) +
                              p.y * p.y * TPointD(p.x, 1.0);
      }
    }

    for (int s = 0; s < jointCount; s++) {
      // 前後の点
      TPointD back = (s == 0) ? stroke.centerPos[0] : stroke.centerPos[s - 1];
      TPointD fore = (s == jointCount - 1) ? stroke.centerPos[jointCount - 1]
                                           : stroke.centerPos[s + 1];
      TPointD vec  = normalize(fore - back);
      TPointD n(-vec.y * stroke.widthHalf, vec.x * stroke.widthHalf);

      // ここで、テクスチャをこっちに向ける処理をいれる
      if (sustain_width_to_skew > 0.0) {
        // 画面座標に変換
        TPointD nr = param[CURRENT]->brushAff * n -
                     param[CURRENT]->brushAff * TPointD(0, 0);
        TPointD vr = param[CURRENT]->brushAff * vec -
                     param[CURRENT]->brushAff * TPointD(0, 0);
        // nr と vrが垂直にちかづくように、nrを回転させる
        double nr_len = norm(nr);

        nr = (1.0 / nr_len) * nr;
        vr = normalize(vr);

        // 内積
        double theta     = std::acos(nr * vr);
        double new_theta = sustain_width_to_skew * M_PI * 0.5 +
                           (1.0 - sustain_width_to_skew) * theta;
        // 外積の正負から、nrがvrのどっち側にあるか判断
        if (cross(vr, nr) < 0) new_theta = -new_theta;
        TPointD new_nr(vr.x * cos(new_theta) - vr.y * sin(new_theta),
                       vr.x * sin(new_theta) + vr.y * cos(new_theta));
        new_nr = nr_len * new_nr;

        // ブラシ座標にもどす
        n = param[CURRENT]->brushAff.inv() *
            (new_nr + param[CURRENT]->brushAff * TPointD(0, 0));
      }

      // はじっこ
      if (p.anti_jaggy && s == 0) {
        TPointD edgePos = stroke.centerPos[0] * 2.0 - stroke.originPos;
        brushVertices.append(
            BrushVertex(edgePos + n, 0.0, (stroke.invert) ? 1.0 : 0.0));
        brushVertices.append(
            BrushVertex(edgePos - n, 1.0, (stroke.invert) ? 1.0 : 0.0));
      }

      double texCoord_v = (stroke.invert) ? 1.0 - (double(s) / segmentAmount)
                                          : double(s) / segmentAmount;

      if (p.anti_jaggy) texCoord_v = 0.25 + 0.5 * texCoord_v;

      brushVertices.append(
          BrushVertex(stroke.centerPos[s] + n, 0.0, texCoord_v));
      brushVertices.append(
          BrushVertex(stroke.centerPos[s] - n, 1.0, texCoord_v));

      // 反対のはじっこ
      if (p.anti_jaggy && s == jointCount - 1) {
        TPointD edgePos =
            stroke.centerPos[jointCount - 1] * 2.0 - stroke.originPos;
        brushVertices.append(
            BrushVertex(edgePos + n, 0.0, (stroke.invert) ? 0.0 : 1.0));
        brushVertices.append(
            BrushVertex(edgePos - n, 1.0, (stroke.invert) ? 0.0 : 1.0));
      }
    }
  }
}
//------------------------------------------------------------
void Iwa_FlowPaintBrushFx::doCompute(TTile &tile, double frame,
                                     const TRenderSettings &ri) {
  if (!m_brush.isConnected()) {
    tile.getRaster()->clear();
    return;
  }

  TDimension b_size(0, 0);
  int lastFrame = 0;
  std::vector<TRasterP> brushRasters;
  // ブラシタッチのラスターデータを取得
  getBrushRasters(brushRasters, b_size, lastFrame, tile, ri);

  if (lastFrame == 0) {
    tile.getRaster()->clear();
    return;
  }

  // パラメータ取得
  FlowPaintBrushFxParam p = getParam(tile, frame, ri);
  p.lastFrame             = lastFrame;

  int vCount = p.reso * 4;
  // 上下にマージン用の頂点を追加
  if (p.anti_jaggy) vCount += 4;

  QVector<BrushVertex> brushVertices;
  QList<BrushStroke> brushStrokes;
  computeBrushVertices(brushVertices, brushStrokes, p, tile, frame, ri);

  GLfloat m[16] = {(float)p.hVec.x,
                   (float)p.hVec.y,
                   0.f,
                   0.f,
                   (float)p.vVec.x,
                   (float)p.vVec.y,
                   0.f,
                   0.f,
                   0.f,
                   0.f,
                   1.f,
                   0.f,
                   (float)p.origin_pos.x,
                   (float)p.origin_pos.y,
                   0.f,
                   1.f};

  QOpenGLContext *context = new QOpenGLContext();
  if (QOpenGLContext::currentContext())
    context->setShareContext(QOpenGLContext::currentContext());
  context->setFormat(QSurfaceFormat::defaultFormat());
  context->create();
  context->makeCurrent(ri.m_offScreenSurface.get());

  // テクスチャの確保
  std::vector<QOpenGLTexture *> brushTextures;
  for (auto texRas : brushRasters) {
    QImage texImg(texRas->getRawData(), b_size.lx, b_size.ly,
                  QImage::Format_RGBA8888);
    // 横幅を3倍にする（左右にx1のマージン）
    if (p.anti_jaggy) {
      QImage resizedImage(texImg.width() * 3, texImg.height() * 2,
                          QImage::Format_RGBA8888);
      QPainter painter(&resizedImage);
      painter.setCompositionMode(QPainter::CompositionMode_Source);
      painter.fillRect(resizedImage.rect(), Qt::transparent);
      painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
      painter.drawImage(QPoint(texImg.width(), texImg.height() / 2), texImg);
      painter.end();
      texImg = resizedImage;
    }

    QOpenGLTexture *texture = new QOpenGLTexture(texImg.rgbSwapped());
    texture->setMinificationFilter(QOpenGLTexture::LinearMipMapLinear);
    texture->setMagnificationFilter(QOpenGLTexture::Linear);
    brushTextures.push_back(texture);
  }

  TDimensionI outDim = tile.getRaster()->getSize();

  // 描画
  {
    std::unique_ptr<QOpenGLFramebufferObject> fb(
        new QOpenGLFramebufferObject(p.dim.lx, p.dim.ly));

    fb->bind();

    glViewport(0, 0, p.dim.lx, p.dim.ly);
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0, p.dim.lx, 0, p.dim.ly);

    glMatrixMode(GL_MODELVIEW);

    glLoadIdentity();
    glLoadMatrixf(m);

    glDisable(GL_POLYGON_SMOOTH);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_TEXTURE_2D);

    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    {
      glVertexPointer(2, GL_DOUBLE, sizeof(BrushVertex), brushVertices.data());
      glTexCoordPointer(2, GL_DOUBLE, sizeof(BrushVertex),
                        (double *)brushVertices.data() + 2);
    }

    int first = 0;

    for (auto stroke : brushStrokes) {
      brushTextures[stroke.textureId]->bind();

      glColor4d(stroke.color.r, stroke.color.g, stroke.color.b, stroke.color.a);
      glDrawArrays(GL_TRIANGLE_STRIP, first, vCount);
      first += vCount;
    }

    glFlush();

    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);

    QImage img =
        fb->toImage().scaled(QSize(p.dim.lx, p.dim.ly), Qt::IgnoreAspectRatio,
                             Qt::SmoothTransformation);
    // y座標は上下反転していることに注意！！
    QRect subRect(tile.m_pos.x - (int)std::round(p.bbox.getP00().x),
                  (int)std::round(p.bbox.getP01().y) - tile.m_pos.y - outDim.ly,
                  outDim.lx, outDim.ly);
    QImage subImg = img.copy(subRect);

    TRaster32P ras32 = tile.getRaster();
    TRaster64P ras64 = tile.getRaster();
    TRaster32P resultRas;
    if (ras32)
      resultRas = ras32;
    else if (ras64) {
      resultRas = TRaster32P(outDim.lx, outDim.ly);
      resultRas->lock();
    }

    for (int y = 0; y < outDim.ly; y++) {
      QRgb *rgb_p =
          reinterpret_cast<QRgb *>(subImg.scanLine(outDim.ly - 1 - y));
      TPixel32 *dst_p = resultRas->pixels(y);
      for (int x = 0; x < outDim.lx; x++, rgb_p++, dst_p++) {
        (*dst_p).r = (unsigned char)qRed(*rgb_p);
        (*dst_p).g = (unsigned char)qGreen(*rgb_p);
        (*dst_p).b = (unsigned char)qBlue(*rgb_p);
        (*dst_p).m = (unsigned char)qAlpha(*rgb_p);
      }
    }

    if (ras64) {
      TRop::convert(ras64, resultRas);
      resultRas->unlock();
    }

    fb->release();
  }
  context->deleteLater();

  for (auto tex : brushTextures) {
    delete tex;
  }
}

//------------------------------------------------------------

void Iwa_FlowPaintBrushFx::getParamUIs(TParamUIConcept *&concepts,
                                       int &length) {
  concepts = new TParamUIConcept[length = 4];

  concepts[0].m_type  = TParamUIConcept::POINT;
  concepts[0].m_label = "Origin";
  concepts[0].m_params.push_back(m_origin_pos);

  concepts[1].m_type  = TParamUIConcept::POINT;
  concepts[1].m_label = "Horizontal Range";
  concepts[1].m_params.push_back(m_horizontal_pos);

  concepts[2].m_type  = TParamUIConcept::POINT;
  concepts[2].m_label = "Vertical Range";
  concepts[2].m_params.push_back(m_vertical_pos);

  concepts[3].m_type = TParamUIConcept::PARALLELOGRAM;
  concepts[3].m_params.push_back(m_origin_pos);
  concepts[3].m_params.push_back(m_horizontal_pos);
  concepts[3].m_params.push_back(m_vertical_pos);
  concepts[3].m_params.push_back(m_curve_point);
}

//------------------------------------------------------------

std::string Iwa_FlowPaintBrushFx::getAlias(double frame,
                                           const TRenderSettings &info) const {
  double refFrame   = m_reference_frame->getValue(frame);
  double prevalence = m_reference_prevalence->getValue(frame);
  if (refFrame < 0 || prevalence == 0.0) {
    // check if the brush is wobbled
    double wobble     = m_pos_wobble->getValue(frame);
    std::string alias = TStandardRasterFx::getAlias(frame, info);
    if (areAlmostEqual(wobble, 0.0))
      return alias;
    else {
      alias.insert(getFxType().length() + 1, std::to_string(frame) + ",");
      return alias;
    }
  }

  std::string alias = getFxType();
  alias += "[";

  // alias degli effetti connessi alle porte di input separati da virgole
  // una porta non connessa da luogo a un alias vuoto (stringa vuota)
  for (int i = 0; i < getInputPortCount(); ++i) {
    TFxPort *port = getInputPort(i);
    if (port->isConnected()) {
      TRasterFxP ifx = port->getFx();
      assert(ifx);
      alias += ifx->getAlias(frame, info);
      // add alias of flow, area and color map in the reference frame
      if (getInputPortName(i) != "Brush") {
        alias += ",";
        alias += ifx->getAlias(refFrame, info);
      }
    }
    alias += ",";
  }

  std::string paramalias("");
  for (int i = 0; i < getParams()->getParamCount(); ++i) {
    TParam *param = getParams()->getParam(i);
    paramalias += param->getName() + "=" + param->getValueAlias(frame, 3);
  }

  return alias + std::to_string(frame) + "," + std::to_string(getIdentifier()) +
         paramalias + "]";
}

FX_PLUGIN_IDENTIFIER(Iwa_FlowPaintBrushFx, "iwa_FlowPaintBrushFx")
