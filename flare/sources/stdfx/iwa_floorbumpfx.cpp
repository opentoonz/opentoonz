#include "iwa_floorbumpfx.h"

#include "tparamuiconcept.h"
#include "iwa_fresnel.h"

#include <QMatrix>

namespace {

inline double cross(const QPointF a, const QPointF b) {
  return a.x() * b.y() - a.y() * b.x();
};

inline float lerp(const float v1, const float v2, const float r) {
  return v1 * (1.0f - r) + v2 * r;
};
inline QPointF lerpUV(const QPointF p1, const QPointF p2, const float r) {
  return p1 * (1.0 - r) + p2 * r;
};
float4 lerp4(const float4 v1, const float4 v2, const float r) {
  return float4{v1.x * (1.0f - r) + v2.x * r, v1.y * (1.0f - r) + v2.y * r,
                v1.z * (1.0f - r) + v2.z * r, v1.w * (1.0f - r) + v2.w * r};
};

inline QVector3D lerpPos(const QVector3D v1, const QVector3D v2,
                         const float r) {
  return (v1 * (1.0 - r) + v2 * r);
};

inline QVector3D lerpNormal(const QVector3D v1, const QVector3D v2,
                            const float r) {
  return lerpPos(v1, v2, r).normalized();
};

inline float fresnelFactor(const float radian) {
  float degree = radian / M_PI_180;
  if (degree < 0.0f)
    return fresnel[0];
  else if (degree >= 90.0f)
    return fresnel[90];
  int id      = int(std::floor(degree));
  float ratio = degree - float(id);
  return lerp(fresnel[id], fresnel[id + 1], ratio);
};

inline bool isTextureUsed(int renderMode) {
  return renderMode == Iwa_FloorBumpFx::TextureMode         // Texture
         || renderMode == Iwa_FloorBumpFx::RefractionMode   // Refraction
         || renderMode == Iwa_FloorBumpFx::ReflectionMode;  // Reflection
}

// pixel height projected on the projection plane
inline double getVPos(const QPointF &zyPos,
                      const Iwa_FloorBumpFx::FloorBumpVars &vars) {
  QPointF a1   = QPointF(0, vars.P_y) - zyPos;
  QPointF a2   = vars.A - vars.B;
  QPointF b1   = vars.A - zyPos;
  QPointF b2   = vars.B - zyPos;
  double s1    = cross(b2, a1) / 2.0;
  double s2    = cross(a1, b1) / 2.0;
  double ratio = s1 / (s1 + s2);
  return vars.H * ratio;
};

inline float4 getSourcePix(QPointF p, float4 *source_host,
                           const Iwa_FloorBumpFx::FloorBumpVars &vars) {
  auto source = [&](QPoint p) {
    return source_host[p.y() * vars.sourceDim.lx + p.x()];
  };

  QPointF uv = (p + QPointF(vars.margin, vars.margin)) * vars.precision;
  int inc_x = 1, inc_y = 1;
  if (uv.x() < 0.0)
    uv.setX(0.0);
  else if (uv.x() >= vars.sourceDim.lx - 1) {
    uv.setX(vars.sourceDim.lx - 1);
    inc_x = 0;
  }
  if (uv.y() < 0.0)
    uv.setY(0.0);
  else if (uv.y() >= vars.sourceDim.ly - 1) {
    uv.setY(vars.sourceDim.ly - 1);
    inc_y = 0;
  }
  QPoint p00(int(std::floor(uv.x())), int(std::floor(uv.y())));
  QPointF frac = uv - QPointF(p00);
  if (frac.isNull()) return source(p00);
  QPoint p01 = p00 + QPoint(inc_x, 0);
  QPoint p10 = p00 + QPoint(0, inc_y);
  QPoint p11 = p00 + QPoint(inc_x, inc_y);

  return lerp4(lerp4(source(p00), source(p01), frac.x()),
               lerp4(source(p10), source(p11), frac.x()), frac.y());
};

inline float getMapValue(QPointF p, float *map_host,
                         const Iwa_FloorBumpFx::FloorBumpVars &vars) {
  auto map = [&](QPoint p) { return map_host[p.y() * vars.refDim.lx + p.x()]; };
  QPointF uv = p + QPointF(vars.margin, vars.margin);
  int inc_x = 1, inc_y = 1;
  if (uv.x() < 0.0)
    uv.setX(0.0);
  else if (uv.x() >= vars.refDim.lx - 1) {
    uv.setX(vars.refDim.lx - 1);
    inc_x = 0;
  }
  if (uv.y() < 0.0)
    uv.setY(0.0);
  else if (uv.y() >= vars.refDim.ly - 1) {
    uv.setY(vars.refDim.ly - 1);
    inc_y = 0;
  }
  QPoint p00(int(std::floor(uv.x())), int(std::floor(uv.y())));
  QPointF frac = uv - QPointF(p00);
  if (frac.isNull()) return map(p00);
  QPoint p01 = p00 + QPoint(inc_x, 0);
  QPoint p10 = p00 + QPoint(0, inc_y);
  QPoint p11 = p00 + QPoint(inc_x, inc_y);

  return lerp(lerp(map(p00), map(p01), frac.x()),
              lerp(map(p10), map(p11), frac.x()), frac.y());
};

// compute horizontal position of the projected point on the projection
// plane
inline double getXPos(QVector3D pos,
                      const Iwa_FloorBumpFx::FloorBumpVars &vars) {
  double angle_Q = vars.angle_el - atan((vars.P_y - pos.y()) / pos.z());
  double dist =
      vars.d_PT * cos(vars.angle_el - angle_Q) / (cos(angle_Q) * pos.z());
  return pos.x() * dist + double(vars.resultDim.lx) * 0.5;
};

inline float4 getTextureOffsetColor(double preTexOff, double texOff,
                                    float ratio) {
  float val = lerp(preTexOff, texOff, ratio);
  return float4{val, val, val, 1.0};
};
inline float4 getDiffuseColor(QVector3D n,
                              const Iwa_FloorBumpFx::FloorBumpVars &vars) {
  float val = QVector3D::dotProduct(n, vars.sunVec);
  if (val < 0.0) val = 0.0;
  return float4{val, val, val, 1.0};
};
inline float4 getDiffuseDifferenceColor(
    QVector3D n, QVector3D base_n, const Iwa_FloorBumpFx::FloorBumpVars &vars) {
  float val      = QVector3D::dotProduct(n, vars.sunVec);
  float base_val = QVector3D::dotProduct(base_n, vars.sunVec);
  val            = (val - base_val) * 0.5 + 0.5;
  if (val < 0.0)
    val = 0.0;
  else if (val > 1.0)
    val = 1.0;
  return float4{val, val, val, 1.0};
};
inline float4 getSpecularColor(QVector3D n, QVector3D pos,
                               const Iwa_FloorBumpFx::FloorBumpVars &vars) {
  QVector3D halfVector =
      ((vars.eyePos - pos).normalized() + vars.sunVec).normalized();
  float val = QVector3D::dotProduct(n, halfVector);
  if (val < 0.0) val = 0.0;
  val = std::pow(val, 50.0);
  return float4{val, val, val, 1.0};
};
inline float4 getSpecularDifferenceColor(
    QVector3D n, QVector3D base_n, QVector3D pos, QVector3D base_pos,
    const Iwa_FloorBumpFx::FloorBumpVars &vars) {
  QVector3D halfVector =
      ((vars.eyePos - pos).normalized() + vars.sunVec).normalized();
  float ang = std::acos(QVector3D::dotProduct(n, halfVector));
  halfVector =
      ((vars.eyePos - base_pos).normalized() + vars.sunVec).normalized();
  float base_ang = std::acos(QVector3D::dotProduct(base_n, halfVector));
  float val      = std::abs(ang - base_ang) / M_PI_2;
  if (val > 1.0) val = 1.0;
  return float4{val, val, val, 1.0};
};
inline float4 getFresnelColor(QVector3D n, QVector3D pos,
                              const Iwa_FloorBumpFx::FloorBumpVars &vars) {
  float angle =
      acos(QVector3D::dotProduct(n, (vars.eyePos - pos).normalized()));
  float val = (fresnelFactor(angle) - vars.base_fresnel_ref) /
              (1.0f - vars.base_fresnel_ref);
  if (val < 0.0) val = 0.0;
  return float4{val, val, val, 1.0};
};
inline float4 getFresnelDifferenceColor(
    QVector3D n, QVector3D base_n, QVector3D pos,
    const Iwa_FloorBumpFx::FloorBumpVars &vars) {
  float val =
      getFresnelColor(n, pos, vars).x - getFresnelColor(base_n, pos, vars).x;
  if (val < 0.0) val = 0.0;
  return float4{val, val, val, 1.0};
};
// currently the texture is assumed as an image of the bottom WITHOUT WATER.
// would it be nice to have a mode to handle the texture as
// an image of the bottom refracted on the surface WITHOUT WAVES?
inline float4 getRefractionColor(QVector3D n, QVector3D pos,
                                 float4 *source_host,
                                 const Iwa_FloorBumpFx::FloorBumpVars &vars) {
  QVector3D eyeVec = (vars.eyePos - pos).normalized();
  // incident angle
  float n_eye       = QVector3D::dotProduct(n, eyeVec);
  double angle_inci = acos(n_eye);
  // refraction angle
  double angle_refr = asin(sin(angle_inci) / vars.r_index);
  QVector3D refrRay =
      sin(angle_refr) * (n_eye * n - eyeVec).normalized() - cos(angle_refr) * n;
  double travelLength = -(vars.depth + pos.y()) / refrRay.y();
  QVector3D bottomPos = pos + refrRay * travelLength;
  return getSourcePix(
      QPointF(getXPos(bottomPos, vars),
              getVPos(QPointF(bottomPos.z(), bottomPos.y()), vars)),
      source_host, vars);
};

inline float4 getReflectionColor(QVector3D n, QVector3D pos,
                                 float4 *source_host,
                                 const Iwa_FloorBumpFx::FloorBumpVars &vars) {
  // draw nothing behind the reflected object
  if (vars.distance > 0.0 && pos.z() >= vars.distance)
    return float4{0.0, 0.0, 0.0, 0.0};
  QVector3D eyeVec = (vars.eyePos - pos).normalized();
  QVector3D refVec = -eyeVec + 2.0 * QVector3D::dotProduct(eyeVec, n) * n;
  if (refVec.z() <= 0.0) return float4{0.0, 0.0, 0.0, 0.0};
  // if the object is at infinite length, compute the reflected UV by
  // using only the angles of reflection vector
  if (vars.distance < 0.0) {
    double angle_ref_azim = asin(refVec.x());
    double angle_ref_elev = asin(refVec.y());
    double ref_v =
        vars.d_PT * tan(vars.angle_el - angle_ref_elev) + vars.H / 2.0;
    double ref_u =
        vars.d_PT * tan(angle_ref_azim) / cos(vars.angle_el - angle_ref_elev) +
        vars.W / 2.0;
    return getSourcePix(QPointF(ref_u, ref_v), source_host, vars);
  }
  // compute the reflected UV
  double length      = (vars.distance - pos.z()) / refVec.z();
  QVector3D boardPos = pos + length * refVec;
  // invert Y coordinate since the texture image is already vertically
  // "reflected"
  boardPos.setY(-boardPos.y());
  return getSourcePix(
      QPointF(getXPos(boardPos, vars),
              getVPos(QPointF(boardPos.z(), boardPos.y()), vars)),
      source_host, vars);
};

inline float4 getColor(QVector3D pre_n, QVector3D cur_n, QVector3D pre_p,
                       QVector3D cur_p, float ratio, float4 *source_host,
                       const Iwa_FloorBumpFx::FloorBumpVars &vars,
                       QVector3D pre_base_n   = QVector3D(0, 1, 0),
                       QVector3D cur_base_n   = QVector3D(0, 1, 0),
                       QVector3D pre_base_pos = QVector3D(),
                       QVector3D cur_base_pos = QVector3D()) {
  if (vars.renderMode == Iwa_FloorBumpFx::DiffuseMode) {
    if (vars.differenceMode)
      return getDiffuseDifferenceColor(
          lerpNormal(pre_n, cur_n, ratio),
          lerpNormal(pre_base_n, cur_base_n, ratio), vars);
    else
      return getDiffuseColor(lerpNormal(pre_n, cur_n, ratio), vars);
  } else if (vars.renderMode == Iwa_FloorBumpFx::SpecularMode) {
    if (vars.differenceMode)
      return getSpecularDifferenceColor(
          lerpNormal(pre_n, cur_n, ratio),
          lerpNormal(pre_base_n, cur_base_n, ratio),
          lerpPos(pre_p, cur_p, ratio),
          lerpPos(pre_base_pos, cur_base_pos, ratio), vars);
    else
      return getSpecularColor(lerpNormal(pre_n, cur_n, ratio),
                              lerpPos(pre_p, cur_p, ratio), vars);
  } else if (vars.renderMode == Iwa_FloorBumpFx::FresnelMode) {
    if (vars.differenceMode)
      return getFresnelDifferenceColor(
          lerpNormal(pre_n, cur_n, ratio),
          lerpNormal(pre_base_n, cur_base_n, ratio),
          lerpPos(pre_p, cur_p, ratio), vars);
    else
      return getFresnelColor(lerpNormal(pre_n, cur_n, ratio),
                             lerpPos(pre_p, cur_p, ratio), vars);
  } else if (vars.renderMode == Iwa_FloorBumpFx::RefractionMode)
    return getRefractionColor(lerpNormal(pre_n, cur_n, ratio),
                              lerpPos(pre_p, cur_p, ratio), source_host, vars);
  else if (vars.renderMode == Iwa_FloorBumpFx::ReflectionMode)
    return getReflectionColor(lerpNormal(pre_n, cur_n, ratio),
                              lerpPos(pre_p, cur_p, ratio), source_host, vars);
  return float4();
};

QList<QPointF> getSubPointsList(int subAmount,
                                const Iwa_FloorBumpFx::FloorBumpVars &vars) {
  QList<QPointF> ret;
  if (!areAlmostEqual(vars.textureOffsetAmount, 0.0)) {
    for (int su = -subAmount; su <= subAmount; su++) {
      double sub_u = float(su) / float(subAmount);
      for (int sv = -subAmount; sv <= subAmount; sv++) {
        double sub_v = float(sv) / float(subAmount);
        // 円の外ならcontinue
        if (sub_u * sub_u + sub_v * sub_v > 1.0) continue;
        if (su == 0 && sv == 0) continue;
        ret.append(QPointF(vars.spread * sub_u, vars.spread * sub_v));
      }
    }
  }
  return ret;
}

}  // namespace

//------------------------------------

Iwa_FloorBumpFx::Iwa_FloorBumpFx()
    : m_renderMode(new TIntEnumParam(TextureMode, "Texture"))
    , m_eyeLevel(0.0)
    , m_drawLevel(-50.0)
    , m_fov(30)
    , m_cameraAltitude(0.0)
    , m_waveHeight(10.0)
    , m_differenceMode(false)
    , m_textureOffsetAmount(0.0)
    , m_textureOffsetSpread(10.0)
    , m_sourcePrecision(300.0 / 162.5)
    , m_souceMargin(0.0)
    , m_displacement(0.0)
    , m_lightAzimuth(-135.0)
    , m_lightElevation(30.0)  // default angle_elev will shade horizontal plane
                              // 50% gray (cos(60)=0.5)
    , m_depth(30.0)
    , m_refractiveIndex(1.33333)
    , m_distanceLevel(-100.0) {
  addInputPort("Height", m_heightRef);
  addInputPort("Texture", m_texture);
  addInputPort("Displacement", m_dispRef);
  bindParam(this, "renderMode", m_renderMode);
  bindParam(this, "fov", m_fov);
  bindParam(this, "cameraAltitude", m_cameraAltitude);
  bindParam(this, "eyeLevel", m_eyeLevel);
  bindParam(this, "drawLevel", m_drawLevel);
  bindParam(this, "waveHeight", m_waveHeight);
  bindParam(this, "differenceMode", m_differenceMode);
  bindParam(this, "textureOffsetAmount", m_textureOffsetAmount);
  bindParam(this, "textureOffsetSpread", m_textureOffsetSpread);
  bindParam(this, "sourcePrecision", m_sourcePrecision);
  bindParam(this, "souceMargin", m_souceMargin);
  bindParam(this, "displacement", m_displacement);
  bindParam(this, "lightAzimuth", m_lightAzimuth);
  bindParam(this, "lightElevation", m_lightElevation);
  bindParam(this, "depth", m_depth);
  bindParam(this, "refractiveIndex", m_refractiveIndex);
  bindParam(this, "distanceLevel", m_distanceLevel);

  m_renderMode->addItem(DiffuseMode, "Diffuse");
  m_renderMode->addItem(SpecularMode, "Specular");
  m_renderMode->addItem(FresnelMode, "Fresnel reflectivity");
  m_renderMode->addItem(RefractionMode, "Refraction");
  m_renderMode->addItem(ReflectionMode, "Reflection");

  m_fov->setValueRange(10, 90);
  m_cameraAltitude->setMeasureName("fxLength");
  m_cameraAltitude->setValueRange(0.0, 300.0);
  m_eyeLevel->setMeasureName("fxLength");
  m_drawLevel->setMeasureName("fxLength");

  m_waveHeight->setMeasureName("fxLength");
  m_waveHeight->setValueRange(-1000.0, 1000.0);
  m_textureOffsetAmount->setMeasureName("fxLength");
  m_textureOffsetAmount->setValueRange(-2000.0, 2000.0);
  m_textureOffsetSpread->setMeasureName("fxLength");
  m_textureOffsetSpread->setValueRange(1.0, 300.0);

  m_sourcePrecision->setValueRange(1.0, 2.0);
  m_souceMargin->setMeasureName("fxLength");
  m_souceMargin->setValueRange(0.0, 100.0);
  m_displacement->setMeasureName("fxLength");
  m_displacement->setValueRange(-50.0, 50.0);

  m_lightAzimuth->setValueRange(-360.0, 360.0);
  m_lightElevation->setValueRange(0.0, 90.0);

  m_depth->setMeasureName("fxLength");
  m_depth->setValueRange(0.0, 1000.0);
  m_refractiveIndex->setValueRange(1.0, 3.0);

  m_distanceLevel->setMeasureName("fxLength");
}

//------------------------------------

bool Iwa_FloorBumpFx::doGetBBox(double frame, TRectD &bBox,
                                const TRenderSettings &info) {
  if (m_heightRef.isConnected()) {
    bool ret = m_heightRef->doGetBBox(frame, bBox, info);
    if (ret) bBox = TConsts::infiniteRectD;
    return ret;
  }
  return false;
}

//------------------------------------

bool Iwa_FloorBumpFx::canHandle(const TRenderSettings &info, double frame) {
  return false;
}

//------------------------------------------------------------
// convert output values (in float4) to channel value
//------------------------------------------------------------
template <typename RASTER, typename PIXEL>
void Iwa_FloorBumpFx::setOutputRaster(float4 *srcMem, const RASTER dstRas,
                                      TDimensionI dim, int drawLevel) {
  typename PIXEL::Channel halfChan =
      (typename PIXEL::Channel)(PIXEL::maxChannelValue / 2);

  dstRas->fill(PIXEL::Transparent);

  for (int j = 0; j < drawLevel; j++) {
    if (j >= dstRas->getLy()) break;

    PIXEL *pix     = dstRas->pixels(j);
    float4 *chan_p = &srcMem[j];
    for (int i = 0; i < dstRas->getLx(); i++, chan_p += drawLevel, pix++) {
      float val;
      val    = (*chan_p).x * (float)PIXEL::maxChannelValue + 0.5f;
      pix->r = (typename PIXEL::Channel)((val > (float)PIXEL::maxChannelValue)
                                             ? (float)PIXEL::maxChannelValue
                                             : val);
      val    = (*chan_p).y * (float)PIXEL::maxChannelValue + 0.5f;
      pix->g = (typename PIXEL::Channel)((val > (float)PIXEL::maxChannelValue)
                                             ? (float)PIXEL::maxChannelValue
                                             : val);
      val    = (*chan_p).z * (float)PIXEL::maxChannelValue + 0.5f;
      pix->b = (typename PIXEL::Channel)((val > (float)PIXEL::maxChannelValue)
                                             ? (float)PIXEL::maxChannelValue
                                             : val);
      val    = (*chan_p).w * (float)PIXEL::maxChannelValue + 0.5f;
      pix->m = (typename PIXEL::Channel)((val > (float)PIXEL::maxChannelValue)
                                             ? (float)PIXEL::maxChannelValue
                                             : val);
    }
  }
}

//------------------------------------

inline void Iwa_FloorBumpFx::initVars(FloorBumpVars &vars, TTile &tile,
                                      const TRenderSettings &settings,
                                      double frame) {
  TAffine aff       = settings.m_affine;
  double factor     = sqrt(std::abs(settings.m_affine.det()));
  double eyeLevel   = m_eyeLevel->getValue(frame) * factor;
  double refLevel   = m_drawLevel->getValue(frame) * factor;
  vars.waveHeight   = m_waveHeight->getValue(frame) * factor;
  vars.displacement = m_displacement->getValue(frame) * factor;
  if (eyeLevel - 1.0 < refLevel) refLevel = eyeLevel - 1.0;
  int drawHeight = std::max(refLevel, vars.waveHeight) - tile.m_pos.y;
  vars.refHeight = int(refLevel - tile.m_pos.y);
  // convert the pixel coordinate from the bottom-left of the camera box
  eyeLevel = eyeLevel - (tile.m_pos.y + tile.getRaster()->getCenterD().y) +
             settings.m_cameraBox.getLy() / 2.0;
  refLevel = refLevel - (tile.m_pos.y + tile.getRaster()->getCenterD().y) +
             settings.m_cameraBox.getLy() / 2.0;

  TRectD rectOut(tile.m_pos, TDimensionD(tile.getRaster()->getLx(),
                                         tile.getRaster()->getLy()));
  vars.outDim    = TDimensionI(rectOut.getLx(), rectOut.getLy());
  vars.resultDim = TDimensionI(rectOut.getLx(),
                               std::min(drawHeight, tile.getRaster()->getLy()));

  // get texture image
  vars.margin    = tceil(m_souceMargin->getValue(frame) * factor);
  vars.precision = m_sourcePrecision->getValue(frame);

  // add margins to all ends and multiply by precision value
  vars.sourceDim = TDimensionI(
      tceil(vars.precision * double(vars.resultDim.lx + vars.margin * 2)),
      tceil(vars.precision * double(vars.resultDim.ly + vars.margin * 2)));
  // only add margins for height image
  vars.refDim = TDimensionI(vars.resultDim.lx + vars.margin * 2,
                            vars.refHeight + vars.margin * 2);

  // collecting parameters
  double angle_halfFov = m_fov->getValue(frame) * M_PI_180 / 2.0;
  vars.textureOffsetAmount =
      100.0 * m_textureOffsetAmount->getValue(frame) * factor * factor;
  vars.spread = m_textureOffsetSpread->getValue(frame) * factor;
  if (vars.spread < 1.0) vars.spread = 1.0;
  vars.camAltitude    = m_cameraAltitude->getValue(frame) * factor;
  vars.differenceMode = m_differenceMode->getValue();

  // making pixels in gray128 to be zero level height
  // ( 128/255. IT'S NOT 0.5! )
  vars.zeroLevel = double(128) / double(TPixel32::maxChannelValue);

  vars.H = settings.m_cameraBox.getLy();
  vars.W = settings.m_cameraBox.getLx();

  double el = eyeLevel - vars.H / 2.0;
  // angle between the optical axis and the horizontal axis
  vars.angle_el =
      std::atan(2.0 * el * tan(angle_halfFov) / double(vars.outDim.ly));
  // distance between the Eye (P) and
  // the center of bottom edge of the projection plane (B)
  double d_PB = double(vars.outDim.ly) / (2.0 * sin(angle_halfFov));
  // Y coordinate of the Eye position (P)
  vars.P_y = vars.camAltitude + d_PB * sin((angle_halfFov) + vars.angle_el);
  // distance from the Eye (P) to the center of the projection plane (T)
  vars.d_PT = vars.H / (2.0 * tan(angle_halfFov));

  QMatrix cam_tilt;
  cam_tilt.rotate(-vars.angle_el / M_PI_180);
  // Z-Y position of the center of top edge of the projection plane (A)
  vars.A =
      cam_tilt.map(QPointF(vars.d_PT, vars.H / 2.0)) + QPointF(0, vars.P_y);
  // Z-Y position of the center of bottom edge of the projection plane (B)
  vars.B =
      cam_tilt.map(QPointF(vars.d_PT, -vars.H / 2.0)) + QPointF(0, vars.P_y);

  vars.C_z    = vars.P_y / tan(angle_halfFov + vars.angle_el);
  vars.eyePos = QVector3D(0.0, vars.P_y, 0.0);

  switch (vars.renderMode) {
    // for shading modes ( diffuse & specular )
  case DiffuseMode:
  case SpecularMode: {
    double angle_azim = m_lightAzimuth->getValue(frame) * M_PI_180;
    double angle_elev = m_lightElevation->getValue(frame) * M_PI_180;
    vars.sunVec = QVector3D(sin(angle_azim) * cos(angle_elev), sin(angle_elev),
                            cos(angle_azim) * cos(angle_elev));
    break;
  }
  case FresnelMode: {  // for fresnel mode
    double angle_base     = 0.5 * M_PI - (angle_halfFov + vars.angle_el);
    vars.base_fresnel_ref = fresnelFactor(angle_base);
    break;
  }
  case RefractionMode: {
    // for refraction mode
    vars.depth   = m_depth->getValue(frame) * factor;
    vars.r_index = m_refractiveIndex->getValue(frame);
    break;
  }
  case ReflectionMode: {
    // for reflection mode
    double distanceLevel = m_distanceLevel->getValue(frame) * factor;
    double angle_dl      = atan(distanceLevel / vars.d_PT);
    // Z-axis distance to the reflected object.
    // distance = -1 means the object is at infinity
    vars.distance = (angle_dl >= vars.angle_el)
                        ? -1
                        : vars.P_y / tan(vars.angle_el - angle_dl);
    break;
  }
  }
}
//------------------------------------

void Iwa_FloorBumpFx::doCompute_CPU(TTile &tile, const double frame,
                                    const TRenderSettings &settings,
                                    const FloorBumpVars &vars,
                                    float4 *source_host, float *ref_host,
                                    float4 *result_host) {
  //-----------
  // Texture Mode
  if (vars.renderMode == TextureMode) {
    // テクスチャの勾配を調べるサンプリング点
    int subAmount = 10;  // これ、パラメータ化するか…？
    QList<QPointF> subPoints = getSubPointsList(subAmount, vars);

    // render pixels in column-major order, from bottom to top
    for (int i = 0; i < vars.resultDim.lx; i++) {
      // cancel check
      if (settings.m_isCanceled && *settings.m_isCanceled) return;

      double maxVPos         = 0.0;  // maximum height of drawn pixels
      double preVPos         = 0.0;  // height of the previous segment
      QPointF preUV          = QPointF(i, -vars.margin);
      float4 *result_p       = &result_host[i * vars.resultDim.ly];
      float4 *end_p          = &result_host[(i + 1) * vars.resultDim.ly - 1];
      float preTextureOffset = 0.0;

      bool backled   = false;
      double preGrad = 100.0;

      // get height pixels from bottom to top
      for (int j = -vars.margin; j < vars.refHeight; j++) {
        // angle between the light axis and the line between (Q) and the eye (P)
        // (Q) is a point on the projection plane at distance of j from (B)
        double angle_Q =
            std::atan((float(j) - (vars.outDim.ly / 2.0)) / vars.d_PT);
        // (R) is an intersection between the XZ plane and the line P->Q
        double R_z = vars.P_y / tan(vars.angle_el - angle_Q);

        // compute (S), a point on the bumped surface in Y-axis direction from
        // (R)
        float refVal =
            getMapValue(QPointF(double(i), double(j)), ref_host, vars);
        QPointF S(R_z, vars.waveHeight * (refVal - vars.zeroLevel) * 2.0);
        // height of (S) projected on the projection plane
        double vPos = getVPos(S, vars);

        // UV coordinate of the texture image
        QPointF currentUV;
        float currentTextureOffset = 0.0;
        if (areAlmostEqual(vars.textureOffsetAmount, 0.0))
          currentUV = QPointF(double(i), getVPos(QPointF(R_z, 0), vars));
        else {
          // length of (1,0,0) vector on the XZ plane, projected on the
          // projection plane
          double dist = (vars.d_PT * sin(vars.angle_el - angle_Q)) / vars.P_y *
                        cos(angle_Q);
          // approximate the gradient by difference of the heights of neighbor
          // points

          // compute gradient of the height image
          QPointF offset;
          for (const QPointF &subPoint : subPoints) {
            // この地点のRefの値を取る
            QPointF subRefPos(double(i) + dist * subPoint.x(),
                              getVPos(QPointF(R_z + subPoint.y(), 0), vars));

            int sign_u = (subPoint.x() > 0) - (subPoint.x() < 0);  // -1, 0, 1
            int sign_v = (subPoint.y() > 0) - (subPoint.y() < 0);  // -1, 0, 1

            float subRefValue = getMapValue(subRefPos, ref_host, vars);
            offset += subRefValue * QPointF(sign_u, sign_v);
          }

          offset *= 1.0 / float(subPoints.count() - subAmount);  // サンプル数
          offset *= vars.textureOffsetAmount /
                    (vars.spread * (0.5 + 0.5 / float(subAmount)));  // 距離
          currentUV = QPointF(double(i) + offset.x() * dist,
                              getVPos(QPointF(R_z + offset.y(), 0), vars));
          if (vars.differenceMode)
            currentTextureOffset =
                std::sqrt(offset.x() * offset.x() + offset.y() * offset.y()) /
                vars.textureOffsetAmount;
        }

        // continue if the current point is behind the forward bumps
        if (vPos <= maxVPos) {
          preVPos          = vPos;
          preUV            = currentUV;
          preTextureOffset = currentTextureOffset;
          continue;
        }

        // putting colors on pixels
        int currentY       = int(std::floor(maxVPos));
        float current_frac = maxVPos - float(currentY);
        int toY            = int(std::floor(vPos));
        float to_frac      = vPos - float(toY);
        // if the entire current segment is in the same pixel,
        // then add colors with percentage of the fraction part
        if (currentY == toY) {
          float ratio = to_frac - current_frac;
          if (vars.differenceMode)
            *result_p += getTextureOffsetColor(preTextureOffset,
                                               currentTextureOffset, 0.5) *
                         ratio;
          else
            *result_p +=
                getSourcePix(lerpUV(preUV, currentUV, 0.5), source_host, vars) *
                ratio;
          preVPos          = vPos;
          maxVPos          = vPos;
          preUV            = currentUV;
          preTextureOffset = currentTextureOffset;
          continue;
        }

        // fill fraction part of the start pixel (currentY)
        float k =
            (maxVPos + (1.0 - current_frac) / 2.0 - preVPos) / (vPos - preVPos);
        float ratio = 1.0 - current_frac;
        if (vars.differenceMode)
          *result_p +=
              getTextureOffsetColor(preTextureOffset, currentTextureOffset, k) *
              ratio;
        else
          *result_p +=
              getSourcePix(lerpUV(preUV, currentUV, k), source_host, vars) *
              ratio;

        if (result_p == end_p) break;
        result_p++;

        // fill pixels between currentY and toY
        for (int y = currentY + 1; y < toY; y++) {
          k = (float(y) + 0.5 - preVPos) / (vPos - preVPos);
          if (vars.differenceMode)
            *result_p += getTextureOffsetColor(preTextureOffset,
                                               currentTextureOffset, k);
          else
            *result_p =
                getSourcePix(lerpUV(preUV, currentUV, k), source_host, vars);
          if (result_p == end_p) break;
          result_p++;
        }

        // fill fraction part of the end pixel (toY)
        k = (float(toY) + to_frac * 0.5 - preVPos) / (vPos - preVPos);
        if (vars.differenceMode)
          *result_p +=
              getTextureOffsetColor(preTextureOffset, currentTextureOffset, k) *
              to_frac;
        else
          *result_p +=
              getSourcePix(lerpUV(preUV, currentUV, k), source_host, vars) *
              to_frac;

        preVPos          = vPos;
        maxVPos          = vPos;
        preUV            = currentUV;
        preTextureOffset = currentTextureOffset;
      }
    }
  }
  // Other modes
  // (Diffuse, Specular, Fresnel, Refraction, Reflection)
  else {
    // render pixels in column-major order, from bottom to top
    for (int i = 0; i < vars.resultDim.lx; i++) {
      // cancel check
      if (settings.m_isCanceled && *settings.m_isCanceled) return;

      double maxVPos = 0.0;  // maximum height of drawn pixels
      double preVPos = 0.0;  // height of the previous segment
      QVector3D preNormal(0, 1, 0);
      QVector3D prePos(i, 0,
                       vars.C_z);  // actually the x coordinate is not i at the
                                   // initial position.., but no problem.
      float4 *result_p = &result_host[i * vars.resultDim.ly];
      float4 *end_p    = &result_host[(i + 1) * vars.resultDim.ly - 1];
      // get height pixels from bottom to top
      for (int j = -vars.margin; j < vars.refHeight; j++) {
        // angle between the light axis and the line between (Q) and the eye (P)
        // (Q) is a point on the projection plane at distance of j from (B)
        double angle_Q =
            std::atan((float(j) - (vars.outDim.ly / 2.0)) / vars.d_PT);
        // (R) is an intersection between the XZ plane and the line P->Q
        double R_z = vars.P_y / tan(vars.angle_el - angle_Q);
        // In reflection mode, do not draw surface behind the Distance Level
        if (vars.renderMode == ReflectionMode && vars.distance > 0.0 &&
            R_z >= vars.distance)
          break;
        // compute normal vector from cross-product of surface vectors
        QVector3D currentNormal;
        // length of (1,0,0) vector on the XZ plane, projected on the projection
        // plane
        double dist = (vars.d_PT * sin(vars.angle_el - angle_Q)) / vars.P_y *
                      cos(angle_Q);
        double xRefGrad =
            getMapValue(QPointF(double(i) + dist, double(j)), ref_host, vars) -
            getMapValue(QPointF(double(i) - dist, double(j)), ref_host, vars);
        double zRefGrad =
            getMapValue(QPointF(double(i), getVPos(QPointF(R_z + 1, 0), vars)),
                        ref_host, vars) -
            getMapValue(QPointF(double(i), getVPos(QPointF(R_z - 1, 0), vars)),
                        ref_host, vars);
        currentNormal.setX(-xRefGrad * vars.waveHeight * 2.0);
        currentNormal.setY(4.0);
        currentNormal.setZ(-zRefGrad * vars.waveHeight * 2.0);
        currentNormal.normalize();

        // compute (S), a point on the bumped surface in Y-axis direction from
        // (R)
        float refVal =
            getMapValue(QPointF(double(i), double(j)), ref_host, vars);
        QPointF S(R_z, vars.waveHeight * (refVal - vars.zeroLevel) * 2.0);
        // height of (S) projected on the projection plane
        double vPos = getVPos(S, vars);
        QVector3D currentPos(double(i - vars.resultDim.lx / 2) / dist, S.y(),
                             S.x());

        // continue if the current point is behind the forward bumps
        if (vPos <= maxVPos) {
          preVPos   = vPos;
          preNormal = currentNormal;
          prePos    = currentPos;
          continue;
        }

        // putting colors on pixels
        int currentY       = int(std::floor(maxVPos));
        float current_frac = maxVPos - float(currentY);
        int toY            = int(std::floor(vPos));
        float to_frac      = vPos - float(toY);
        // if the entire current segment is in the same pixel,
        // then add colors with percentage of the fraction part
        if (currentY == toY) {
          float ratio = to_frac - current_frac;
          *result_p += getColor(preNormal, currentNormal, prePos, currentPos,
                                0.5, source_host, vars) *
                       ratio;
          preVPos   = vPos;
          maxVPos   = vPos;
          preNormal = currentNormal;
          prePos    = currentPos;
          continue;
        }

        // fill fraction part of the start pixel (currentY)
        float k =
            (maxVPos + (1.0 - current_frac) / 2.0 - preVPos) / (vPos - preVPos);
        float4 color = getColor(preNormal, currentNormal, prePos, currentPos, k,
                                source_host, vars);
        float ratio  = 1.0 - current_frac;
        *result_p += color * ratio;

        if (result_p == end_p) break;
        result_p++;

        // fill pixels between currentY and toY
        for (int y = currentY + 1; y < toY; y++) {
          k         = (float(y) + 0.5 - preVPos) / (vPos - preVPos);
          *result_p = getColor(preNormal, currentNormal, prePos, currentPos, k,
                               source_host, vars);
          if (result_p == end_p) break;
          result_p++;
        }

        // fill fraction part of the end pixel (toY)
        k     = (float(toY) + to_frac * 0.5 - preVPos) / (vPos - preVPos);
        color = getColor(preNormal, currentNormal, prePos, currentPos, k,
                         source_host, vars);
        *result_p += color * to_frac;

        preVPos   = vPos;
        maxVPos   = vPos;
        preNormal = currentNormal;
        prePos    = currentPos;
      }
    }
  }
}

//------------------------------------

void Iwa_FloorBumpFx::doCompute_with_Displacement(
    TTile &tile, const double frame, const TRenderSettings &settings,
    const FloorBumpVars &vars, float4 *source_host, float *ref_host,
    float *disp_host, float4 *result_host) {
  //----- Lambdas ------

  // テクスチャの勾配を調べるサンプリング点
  int subAmount            = 10;  // これ、パラメータ化するか…？
  QList<QPointF> subPoints = getSubPointsList(subAmount, vars);

  //-----------
  // Texture Mode
  if (vars.renderMode == TextureMode) {
    // render pixels in column-major order, from bottom to top
    for (int i = 0; i < vars.resultDim.lx; i++) {
      // cancel check
      if (settings.m_isCanceled && *settings.m_isCanceled) return;

      double maxVPos         = 0.0;  // maximum height of drawn pixels
      double preVPos         = 0.0;  // height of the previous segment
      QPointF preUV          = QPointF(i, -vars.margin);
      float4 *result_p       = &result_host[i * vars.resultDim.ly];
      float4 *end_p          = &result_host[(i + 1) * vars.resultDim.ly - 1];
      float preTextureOffset = 0.0;
      // get height pixels from bottom to top
      for (int j = -vars.margin; j < vars.refHeight; j++) {
        // angle between the light axis and the line between (Q) and the eye (P)
        // (Q) is a point on the projection plane at distance of j from (B)
        double angle_Q =
            std::atan((float(j) - (vars.outDim.ly / 2.0)) / vars.d_PT);
        // (R) is an intersection between the XZ plane and the line P->Q
        double R_z = vars.P_y / tan(vars.angle_el - angle_Q);

        // compute (S), a point on the bumped surface in Y-axis direction from
        // (R)
        float refVal =
            getMapValue(QPointF(double(i), double(j)), ref_host, vars);
        QPointF S(R_z, vars.waveHeight * (refVal - vars.zeroLevel) * 2.0);

        // length of (1,0,0) vector on the XZ plane, projected on the
        // projection plane
        double dist = (vars.d_PT * sin(vars.angle_el - angle_Q)) / vars.P_y *
                      cos(angle_Q);

        // UV coordinate of the texture image
        QVector3D uvOffset;
        float currentTextureOffset = 0.0;
        if (vars.textureOffsetAmount == 0.0) {
          // currentUV = QPointF(double(i), getVPos(QPointF(R_z, 0)));
        } else {
          // approximate the gradient by difference of the heights of neighbor
          // points

          // compute gradient of the height image
          QPointF offset;
          for (const QPointF &subPoint : subPoints) {
            // この地点のRefの値を取る
            QPointF subRefPos(double(i) + dist * subPoint.x(),
                              getVPos(QPointF(R_z + subPoint.y(), 0), vars));

            int sign_u = (subPoint.x() > 0) - (subPoint.x() < 0);  // -1, 0, 1
            int sign_v = (subPoint.y() > 0) - (subPoint.y() < 0);  // -1, 0, 1

            float subRefValue = getMapValue(subRefPos, ref_host, vars);
            offset += subRefValue * QPointF(sign_u, sign_v);
          }
          offset *= 1.0 / float(subPoints.count() - subAmount);  // サンプル数
          offset *= vars.textureOffsetAmount /
                    (vars.spread * (0.5 + 0.5 / float(subAmount)));  // 距離

          uvOffset.setX(offset.x() * dist);
          uvOffset.setZ(offset.y());
          // currentUV = QPointF(double(i) + offset.x() * dist,
          //   getVPos(QPointF(R_z + offset.y(), 0)));
          if (vars.differenceMode)
            currentTextureOffset =
                1000 *
                std::sqrt(offset.x() * offset.x() + offset.y() * offset.y()) /
                vars.textureOffsetAmount;
        }
        QPointF currentDispUV =
            QPointF(double(i) + uvOffset.x(),
                    getVPos(QPointF(R_z + uvOffset.z(), 0), vars));

        // この地点の変位の値を取得する
        float dispValue  = getMapValue(currentDispUV, disp_host, vars);
        float dispHeight = dispValue * vars.displacement;

        // この地点の法線を取得
        QVector3D currentNormal;
        double xRefGrad =
            getMapValue(QPointF(double(i) + dist, double(j)), ref_host, vars) -
            getMapValue(QPointF(double(i) - dist, double(j)), ref_host, vars);
        double zRefGrad =
            getMapValue(QPointF(double(i), getVPos(QPointF(R_z + 1, 0), vars)),
                        ref_host, vars) -
            getMapValue(QPointF(double(i), getVPos(QPointF(R_z - 1, 0), vars)),
                        ref_host, vars);
        currentNormal.setX(-xRefGrad * vars.waveHeight * 2.0);
        currentNormal.setY(4.0);
        currentNormal.setZ(-zRefGrad * vars.waveHeight * 2.0);
        currentNormal.normalize();

        // 変位後の表面座標 (M)
        // ※法線のX成分は強引にY座標の変位に持ち込んで近似する
        float dispXY = std::sqrt(currentNormal.x() * currentNormal.x() +
                                 currentNormal.y() * currentNormal.y());
        QPointF M    = S + QPointF(currentNormal.z(), dispXY) * dispHeight;
        // float dispXRatio = std::sqrt(currentNormal.x()*currentNormal.x() +
        // currentNormal.y()*currentNormal.y()) / currentNormal.y(); QPointF M =
        // S + QPointF(currentNormal.z(), currentNormal.y()* dispXRatio) *
        // dispHeight;

        // height of (M) projected on the projection plane
        double vPos = getVPos(M, vars);

        uvOffset.setY(dispHeight);
        QPointF currentUV =
            QPointF(double(i) + uvOffset.x(),
                    getVPos(QPointF(R_z + uvOffset.z(), uvOffset.y()), vars));

        // 手前のDisplacementで隠されている部分かどうかの判定
        bool isBehind = false;

        if (preUV.y() > currentUV.y()) isBehind = true;
        /*
        if ((vars.displacement > 0.0 && dispValue < 1.0) ||
            (vars.displacement < 0.0 && dispValue > 0.0)) {
          QPointF dUV     = (currentDispUV - currentUV) * 0.1;
          QPointF tmpUV   = currentUV + dUV;
          float dHeight   = dispHeight * 0.1;
          float tmpHeight = dHeight;
          for (int k = 1; k < 10; k++, tmpUV += dUV, tmpHeight += dHeight) {
            if (getMapValue(tmpUV, disp_host, vars) * vars.displacement >
                tmpHeight) {
              isBehind = true;
              break;
            }
          }
        }*/

        // continue if the current point is behind the forward bumps
        if (vPos <= maxVPos) {
          preVPos = vPos;
          if (!isBehind) preUV = currentUV;
          preTextureOffset = currentTextureOffset;
          continue;
        }

        // putting colors on pixels
        int currentY       = int(std::floor(maxVPos));
        float current_frac = maxVPos - float(currentY);
        int toY            = int(std::floor(vPos));
        float to_frac      = vPos - float(toY);
        // if the entire current segment is in the same pixel,
        // then add colors with percentage of the fraction part
        if (currentY == toY) {
          if (!isBehind) {
            float ratio = to_frac - current_frac;
            if (vars.differenceMode)
              *result_p += getTextureOffsetColor(preTextureOffset,
                                                 currentTextureOffset, 0.5) *
                           ratio;
            else
              *result_p += getSourcePix(lerpUV(preUV, currentUV, 0.5),
                                        source_host, vars) *
                           ratio;
            preUV = currentUV;
          }
          preVPos          = vPos;
          maxVPos          = vPos;
          preTextureOffset = currentTextureOffset;
          continue;
        }

        // fill fraction part of the start pixel (currentY)
        if (!isBehind) {
          float k = (maxVPos + (1.0 - current_frac) / 2.0 - preVPos) /
                    (vPos - preVPos);
          float ratio = 1.0 - current_frac;
          if (vars.differenceMode)
            *result_p += getTextureOffsetColor(preTextureOffset,
                                               currentTextureOffset, k) *
                         ratio;
          else
            *result_p +=
                getSourcePix(lerpUV(preUV, currentUV, k), source_host, vars) *
                ratio;
        }

        if (result_p == end_p) break;
        result_p++;

        // fill pixels between currentY and toY
        for (int y = currentY + 1; y < toY; y++) {
          if (!isBehind) {
            float k = (float(y) + 0.5 - preVPos) / (vPos - preVPos);
            if (vars.differenceMode)
              *result_p += getTextureOffsetColor(preTextureOffset,
                                                 currentTextureOffset, k);
            else
              *result_p =
                  getSourcePix(lerpUV(preUV, currentUV, k), source_host, vars);
          }
          if (result_p == end_p) break;
          result_p++;
        }

        // fill fraction part of the end pixel (toY)
        if (!isBehind) {
          float k = (float(toY) + to_frac * 0.5 - preVPos) / (vPos - preVPos);
          if (vars.differenceMode)
            *result_p += getTextureOffsetColor(preTextureOffset,
                                               currentTextureOffset, k) *
                         to_frac;
          else
            *result_p +=
                getSourcePix(lerpUV(preUV, currentUV, k), source_host, vars) *
                to_frac;
          preUV = currentUV;
        }

        preVPos          = vPos;
        maxVPos          = vPos;
        preTextureOffset = currentTextureOffset;
      }
    }
  }
  // Other modes
  // (Diffuse, Specular, Fresnel, Refraction, Reflection)
  else {
    // render pixels in column-major order, from bottom to top
    for (int i = 0; i < vars.resultDim.lx; i++) {
      // cancel check
      if (settings.m_isCanceled && *settings.m_isCanceled) return;

      double maxVPos = 0.0;  // maximum height of drawn pixels
      double preVPos = 0.0;  // height of the previous segment
      QVector3D preDispNormal(0, 1, 0);
      QVector3D preBaseNormal(0, 1, 0);
      QVector3D prePos(i, 0,
                       vars.C_z);  // actually the x coordinate is not i at the
                                   // initial position.., but no problem.
      QVector3D preBasePos(i, 0, vars.C_z);

      float4 *result_p = &result_host[i * vars.resultDim.ly];
      float4 *end_p    = &result_host[(i + 1) * vars.resultDim.ly - 1];
      // get height pixels from bottom to top
      for (int j = -vars.margin; j < vars.refHeight; j++) {
        // angle between the light axis and the line between (Q) and the eye (P)
        // (Q) is a point on the projection plane at distance of j from (B)
        double angle_Q =
            std::atan((float(j) - (vars.outDim.ly / 2.0)) / vars.d_PT);
        // (R) is an intersection between the XZ plane and the line P->Q
        double R_z = vars.P_y / tan(vars.angle_el - angle_Q);
        // In reflection mode, do not draw surface behind the Distance Level
        if (vars.renderMode == ReflectionMode && vars.distance > 0.0 &&
            R_z >= vars.distance)
          break;

        // compute (S), a point on the bumped surface in Y-axis direction from
        // (R)
        float refVal =
            getMapValue(QPointF(double(i), double(j)), ref_host, vars);
        QPointF S(R_z, vars.waveHeight * (refVal - vars.zeroLevel) * 2.0);
        // length of (1,0,0) vector on the XZ plane, projected on the projection
        // plane
        double dist = (vars.d_PT * sin(vars.angle_el - angle_Q)) / vars.P_y *
                      cos(angle_Q);

        // UV coordinate of the texture image
        QVector3D uvOffset;
        if (vars.textureOffsetAmount == 0.0) {
          // currentUV = QPointF(double(i), getVPos(QPointF(R_z, 0)));
        } else {
          // approximate the gradient by difference of the heights of neighbor
          // points

          // compute gradient of the height image
          QPointF offset;
          for (const QPointF &subPoint : subPoints) {
            // この地点のRefの値を取る
            QPointF subRefPos(double(i) + dist * subPoint.x(),
                              getVPos(QPointF(R_z + subPoint.y(), 0), vars));

            int sign_u = (subPoint.x() > 0) - (subPoint.x() < 0);  // -1, 0, 1
            int sign_v = (subPoint.y() > 0) - (subPoint.y() < 0);  // -1, 0, 1

            float subRefValue = getMapValue(subRefPos, ref_host, vars);
            offset += subRefValue * QPointF(sign_u, sign_v);
          }
          offset *= 1.0 / float(subPoints.count() - subAmount);  // サンプル数
          offset *= vars.textureOffsetAmount /
                    (vars.spread * (0.5 + 0.5 / float(subAmount)));  // 距離

          uvOffset.setX(offset.x() * dist);
          uvOffset.setZ(offset.y());
          // currentUV = QPointF(double(i) + offset.x() * dist,
          //   getVPos(QPointF(R_z + offset.y(), 0)));
        }
        QPointF currentDispUV =
            QPointF(double(i) + uvOffset.x(),
                    getVPos(QPointF(R_z + uvOffset.z(), 0), vars));

        // この地点の変位の値を取得する
        float dispHeight =
            getMapValue(currentDispUV, disp_host, vars) * vars.displacement;

        // compute normal vector from cross-product of surface vectors
        QVector3D rightPos(
            1.0,
            getMapValue(QPointF(double(i) + dist, double(j)), ref_host, vars) *
                vars.waveHeight,
            0.0);
        QVector3D leftPos(
            -1.0,
            getMapValue(QPointF(double(i) - dist, double(j)), ref_host, vars) *
                vars.waveHeight,
            0.0);
        QVector3D farPos(
            0.0,
            getMapValue(QPointF(double(i), getVPos(QPointF(R_z + 1, 0), vars)),
                        ref_host, vars) *
                vars.waveHeight,
            1.0);
        QVector3D nearPos(
            0.0,
            getMapValue(QPointF(double(i), getVPos(QPointF(R_z - 1, 0), vars)),
                        ref_host, vars) *
                vars.waveHeight,
            -1.0);
        // 左手系座標なので外積を反転する
        QVector3D currentNormal =
            -QVector3D::crossProduct(rightPos - leftPos, farPos - nearPos);
        currentNormal.normalize();

        // 変位後の表面座標 (M)
        // ※法線のX成分は強引にY座標の変位に持ち込んで近似する
        float dispXY = std::sqrt(currentNormal.x() * currentNormal.x() +
                                 currentNormal.y() * currentNormal.y());
        QPointF M    = S + QPointF(currentNormal.z(), dispXY) * dispHeight;

        // 前後左右の座標に変位を加える
        QPointF rightDispUV = currentDispUV + QPointF(dist, 0);
        QPointF leftDispUV  = currentDispUV + QPointF(-dist, 0);
        QPointF farDispUV(currentDispUV.x(),
                          getVPos(QPointF(R_z + uvOffset.z() + 1, 0), vars));
        QPointF nearDispUV(currentDispUV.x(),
                           getVPos(QPointF(R_z + uvOffset.z() - 1, 0), vars));
        float rightDispHeight =
            getMapValue(rightDispUV, disp_host, vars) * vars.displacement;
        float leftDispHeight =
            getMapValue(leftDispUV, disp_host, vars) * vars.displacement;
        float farDispHeight =
            getMapValue(farDispUV, disp_host, vars) * vars.displacement;
        float nearDispHeight =
            getMapValue(nearDispUV, disp_host, vars) * vars.displacement;

        rightPos += currentNormal * rightDispHeight;
        leftPos += currentNormal * leftDispHeight;
        farPos += currentNormal * farDispHeight;
        nearPos += currentNormal * nearDispHeight;

        // 外積で法線を求める 左手系座標なので外積を反転する
        QVector3D dispNormal =
            -QVector3D::crossProduct(rightPos - leftPos, farPos - nearPos);
        dispNormal.normalize();

        // height of (M) projected on the projection plane
        double vPos = getVPos(M, vars);
        QVector3D currentPos(double(i - vars.resultDim.lx / 2) / dist, M.y(),
                             M.x());

        // Diffuse差分取得のために、変形無しの状態の法線を取得する
        QVector3D baseNormal;
        // Specular差分取得のために、変形無しの状態の位置も取得
        QVector3D basePos;
        if (vars.differenceMode) {
          baseNormal = -QVector3D::crossProduct(
              QVector3D(2.0, rightDispHeight - leftDispHeight, 0.0),
              QVector3D(0.0, farDispHeight - nearDispHeight, 2.0));
          basePos = QVector3D(currentPos.x() + uvOffset.x() / dist, dispHeight,
                              currentPos.z() + uvOffset.z());
        }

        // continue if the current point is behind the forward bumps
        if (vPos <= maxVPos) {
          preVPos       = vPos;
          preDispNormal = dispNormal;
          preBaseNormal = baseNormal;
          prePos        = currentPos;
          preBasePos    = basePos;
          continue;
        }

        // putting colors on pixels
        int currentY       = int(std::floor(maxVPos));
        float current_frac = maxVPos - float(currentY);
        int toY            = int(std::floor(vPos));
        float to_frac      = vPos - float(toY);
        // if the entire current segment is in the same pixel,
        // then add colors with percentage of the fraction part
        if (currentY == toY) {
          float ratio = to_frac - current_frac;
          *result_p += getColor(preDispNormal, dispNormal, prePos, currentPos,
                                0.5, source_host, vars, preBaseNormal,
                                baseNormal, preBasePos, basePos) *
                       ratio;
          preVPos       = vPos;
          maxVPos       = vPos;
          preDispNormal = dispNormal;
          preBaseNormal = baseNormal;
          prePos        = currentPos;
          preBasePos    = basePos;
          continue;
        }

        // fill fraction part of the start pixel (currentY)
        float k =
            (maxVPos + (1.0 - current_frac) / 2.0 - preVPos) / (vPos - preVPos);
        float4 color = getColor(preDispNormal, dispNormal, prePos, currentPos,
                                k, source_host, vars, preBaseNormal, baseNormal,
                                preBasePos, basePos);
        float ratio  = 1.0 - current_frac;
        *result_p += color * ratio;

        if (result_p == end_p) break;
        result_p++;

        // fill pixels between currentY and toY
        for (int y = currentY + 1; y < toY; y++) {
          k         = (float(y) + 0.5 - preVPos) / (vPos - preVPos);
          *result_p = getColor(preDispNormal, dispNormal, prePos, currentPos, k,
                               source_host, vars, preBaseNormal, baseNormal,
                               preBasePos, basePos);
          if (result_p == end_p) break;
          result_p++;
        }

        // fill fraction part of the end pixel (toY)
        k     = (float(toY) + to_frac * 0.5 - preVPos) / (vPos - preVPos);
        color = getColor(preDispNormal, dispNormal, prePos, currentPos, k,
                         source_host, vars, preBaseNormal, baseNormal,
                         preBasePos, basePos);
        *result_p += color * to_frac;

        preVPos       = vPos;
        maxVPos       = vPos;
        preDispNormal = dispNormal;
        preBaseNormal = baseNormal;
        prePos        = currentPos;
        preBasePos    = basePos;
      }
    }
  }
}

//------------------------------------------------------------
// convert input tile's channel values to float4 values
//------------------------------------------------------------

template <typename RASTER, typename PIXEL>
void Iwa_FloorBumpFx::setSourceRaster(const RASTER srcRas, float4 *srcMem,
                                      TDimensionI dim) {
  float4 *s_p = srcMem;
  for (int j = 0; j < dim.ly; j++) {
    PIXEL *s_pix = srcRas->pixels(j);
    for (int i = 0; i < dim.lx; i++, s_pix++, s_p++) {
      (*s_p).x = (float)s_pix->r / (float)PIXEL::maxChannelValue;
      (*s_p).y = (float)s_pix->g / (float)PIXEL::maxChannelValue;
      (*s_p).z = (float)s_pix->b / (float)PIXEL::maxChannelValue;
      (*s_p).w = (float)s_pix->m / (float)PIXEL::maxChannelValue;
    }
  }
}

void Iwa_FloorBumpFx::setRefRaster(const TRaster64P refRas, float *refMem,
                                   TDimensionI dim, bool isRef) {
  float zeroLevel =
      (isRef) ? float(128) / float(TPixel32::maxChannelValue) : 0.0;
  float *r_p = refMem;
  for (int j = 0; j < dim.ly; j++) {
    TPixel64 *r_pix = refRas->pixels(j);
    for (int i = 0; i < dim.lx; i++, r_pix++, r_p++) {
      float r = (float)r_pix->r / (float)TPixel64::maxChannelValue;
      float g = (float)r_pix->g / (float)TPixel64::maxChannelValue;
      float b = (float)r_pix->b / (float)TPixel64::maxChannelValue;
      float m = (float)r_pix->m / (float)TPixel64::maxChannelValue;
      // taking brightness
      float brightness = 0.298912f * r + 0.586610f * g + 0.114478f * b;
      (*r_p)           = brightness * m + zeroLevel * (1.0 - m);
    }
  }
}

//------------------------------------

void Iwa_FloorBumpFx::doCompute(TTile &tile, double frame,
                                const TRenderSettings &rend_sets) {
  FloorBumpVars vars;
  vars.renderMode = m_renderMode->getValue();
  if ((isTextureUsed(vars.renderMode) && !m_texture.isConnected()) ||
      !m_heightRef.isConnected()) {
    tile.getRaster()->clear();
    return;
  }

  initVars(vars, tile, rend_sets, frame);

  // メモリの確保
  float4 *source_host = nullptr;
  float *ref_host;
  float *disp_host = nullptr;

  TRasterGR8P source_host_ras;
  if (isTextureUsed(vars.renderMode)) {
    source_host_ras =
        TRasterGR8P(vars.sourceDim.lx * sizeof(float4), vars.sourceDim.ly);
    source_host_ras->lock();
    source_host = (float4 *)source_host_ras->getRawData();
  }

  TRasterGR8P ref_host_ras(vars.refDim.lx * sizeof(float), vars.refDim.ly);
  ref_host_ras->lock();
  ref_host = (float *)ref_host_ras->getRawData();

  TRasterGR8P disp_host_ras;
  if (vars.displacement != 0.0 &&
      m_dispRef.isConnected()) {  // renderMode すべてに対応させるかはTBD
    disp_host_ras = TRasterGR8P(vars.refDim.lx * sizeof(float), vars.refDim.ly);
    disp_host_ras->lock();
    disp_host = (float *)disp_host_ras->getRawData();
  }

  // 画像の取得、格納
  {
    if (isTextureUsed(vars.renderMode)) {
      TRenderSettings source_sets(rend_sets);
      TPointD sourceTilePos =
          (tile.m_pos - TPointD(vars.margin, vars.margin)) * vars.precision;
      source_sets.m_affine *= TScale(vars.precision, vars.precision);

      TTile sourceTile;
      m_texture->allocateAndCompute(sourceTile, sourceTilePos, vars.sourceDim,
                                    tile.getRaster(), frame, source_sets);

      TRaster32P ras32 = (TRaster32P)sourceTile.getRaster();
      TRaster64P ras64 = (TRaster64P)sourceTile.getRaster();
      if (ras32)
        setSourceRaster<TRaster32P, TPixel32>(ras32, source_host,
                                              vars.sourceDim);
      else if (ras64)
        setSourceRaster<TRaster64P, TPixel64>(ras64, source_host,
                                              vars.sourceDim);
    }
    // height image is always computed in 16bpc regardless of the current render
    // settings in order to get smooth gradient
    TRenderSettings ref_sets(rend_sets);
    TPointD refTilePos = tile.m_pos - TPointD(vars.margin, vars.margin);
    ref_sets.m_bpp     = 64;
    TTile refTile;
    m_heightRef->allocateAndCompute(refTile, refTilePos, vars.refDim,
                                    TRasterP(), frame, ref_sets);
    setRefRaster((TRaster64P)refTile.getRaster(), ref_host, vars.refDim, true);

    // disp imageも同様
    if (disp_host) {
      TTile dispTile;
      m_dispRef->allocateAndCompute(dispTile, refTilePos, vars.refDim,
                                    TRasterP(), frame, ref_sets);
      setRefRaster((TRaster64P)dispTile.getRaster(), disp_host, vars.refDim,
                   false);
    }
  }

  // prepare the memory for result image
  TRasterGR8P result_host_ras(vars.resultDim.lx * sizeof(float4),
                              vars.resultDim.ly);
  float4 *result_host;
  result_host_ras->lock();
  result_host = (float4 *)result_host_ras->getRawData();

  // compute
  if (disp_host) {
    doCompute_with_Displacement(tile, frame, rend_sets, vars, source_host,
                                ref_host, disp_host, result_host);
  } else {
    doCompute_CPU(tile, frame, rend_sets, vars, source_host, ref_host,
                  result_host);
  }

  if (isTextureUsed(vars.renderMode)) source_host_ras->unlock();
  ref_host_ras->unlock();
  if (disp_host) disp_host_ras->unlock();

  // cancel check
  if (rend_sets.m_isCanceled && *rend_sets.m_isCanceled) {
    tile.getRaster()->clear();
    return;
  }

  // output the result
  TRaster32P outRas32 = (TRaster32P)tile.getRaster();
  TRaster64P outRas64 = (TRaster64P)tile.getRaster();
  if (outRas32)
    setOutputRaster<TRaster32P, TPixel32>(result_host, outRas32, vars.outDim,
                                          vars.resultDim.ly);
  else if (outRas64)
    setOutputRaster<TRaster64P, TPixel64>(result_host, outRas64, vars.outDim,
                                          vars.resultDim.ly);

  result_host_ras->unlock();
}

//------------------------------------

void Iwa_FloorBumpFx::getParamUIs(TParamUIConcept *&concepts, int &length) {
  concepts = new TParamUIConcept[length = 3];

  concepts[0].m_type  = TParamUIConcept::VERTICAL_POS;
  concepts[0].m_label = "Eye Level";
  concepts[0].m_params.push_back(m_eyeLevel);

  concepts[1].m_type  = TParamUIConcept::VERTICAL_POS;
  concepts[1].m_label = "Draw Level";
  concepts[1].m_params.push_back(m_drawLevel);

  concepts[2].m_type  = TParamUIConcept::VERTICAL_POS;
  concepts[2].m_label = "Distance Level";
  concepts[2].m_params.push_back(m_distanceLevel);
  concepts[2].m_params.push_back(m_renderMode);
}

//------------------------------------

FX_PLUGIN_IDENTIFIER(Iwa_FloorBumpFx, "iwa_FloorBumpFx")
