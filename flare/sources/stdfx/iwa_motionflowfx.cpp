#include "iwa_motionflowfx.h"

#include "tfxattributes.h"

#include "toonz/tstageobject.h"

#include "trop.h"

namespace {
double getSizePixelAmount(const double val, const TAffine affine) {
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
  return std::sqrt(vect.x * vect.x + vect.y * vect.y);
}

}  // namespace

//------------------------------------------------------------

Iwa_MotionFlowFx::Iwa_MotionFlowFx()
    : m_normalizeType(new TIntEnumParam(NORMALIZE_AUTO, "Auto"))
    , m_normalizeRange(1.0) {
  bindParam(this, "shutterLength", m_shutterLength);
  bindParam(this, "motionObjectType", m_motionObjectType);
  bindParam(this, "motionObjectIndex", m_motionObjectIndex);
  bindParam(this, "normalizeType", m_normalizeType);
  bindParam(this, "normalizeRange", m_normalizeRange);

  m_normalizeType->addItem(NORMALIZE_MANUAL, "Manual");
  m_normalizeRange->setMeasureName("fxLength");
  m_normalizeRange->setValueRange(0.01, 1000.0);

  getAttributes()->setIsSpeedAware(true);
}

//------------------------------------------------------------

void Iwa_MotionFlowFx::doCompute(TTile& tile, double frame,
                                 const TRenderSettings& settings) {
  /* Get parameters */
  TAffine aff_Before, aff_After;
  getAttributes()->getMotionAffines(aff_Before, aff_After);

  TDimensionI dimOut(tile.getRaster()->getLx(), tile.getRaster()->getLy());
  /* output */
  TRasterGR8P out_ras(sizeof(double3) * dimOut.lx * dimOut.ly, 1);
  out_ras->lock();
  double3* out_ras_p = (double3*)out_ras->getRawData();

  double3* out_p = out_ras_p;

  double norm_range = 0.0;

  for (int y = 0; y < dimOut.ly; y++) {
    for (int x = 0; x < dimOut.lx; x++, out_p++) {
      TPointD pos = TPointD((double)x, (double)y) + tile.m_pos;
      TPointD vec = pos - (aff_Before * aff_After.inv() * pos);
      out_p->z    = std::sqrt(vec.x * vec.x + vec.y * vec.y);
      if (out_p->z > 0.0) {
        out_p->x   = vec.x / out_p->z;
        out_p->y   = vec.y / out_p->z;
        norm_range = std::max(norm_range, out_p->z);
      } else {
        out_p->x = 0.0;
        out_p->y = 0.0;
      }
    }
  }

  if (m_normalizeType->getValue() == NORMALIZE_MANUAL)
    norm_range = getSizePixelAmount(m_normalizeRange->getValue(frame),
                                    settings.m_affine);

  tile.getRaster()->clear();
  TRaster32P outRas32 = (TRaster32P)tile.getRaster();
  TRaster64P outRas64 = (TRaster64P)tile.getRaster();
  if (outRas32)
    setOutRas<TRaster32P, TPixel32>(outRas32, out_ras_p, norm_range);
  else if (outRas64)
    setOutRas<TRaster64P, TPixel64>(outRas64, out_ras_p, norm_range);

  out_ras->unlock();
}

//------------------------------------------------------------

template <typename RASTER, typename PIXEL>
void Iwa_MotionFlowFx::setOutRas(RASTER ras, double3* outBuf,
                                 double norm_range) {
  double3* buf_p = outBuf;
  for (int j = 0; j < ras->getLy(); j++) {
    PIXEL* pix = ras->pixels(j);
    for (int i = 0; i < ras->getLx(); i++, pix++, buf_p++) {
      double val = 0.5 + buf_p->x * 0.5;
      pix->r = (typename PIXEL::Channel)(val * double(PIXEL::maxChannelValue));
      val    = 0.5 + buf_p->y * 0.5;
      pix->g = (typename PIXEL::Channel)(val * double(PIXEL::maxChannelValue));
      val    = std::min(1.0, buf_p->z / norm_range);
      pix->b = (typename PIXEL::Channel)(val * double(PIXEL::maxChannelValue));
      pix->m = PIXEL::maxChannelValue;
    }
  }
}

//------------------------------------------------------------

bool Iwa_MotionFlowFx::doGetBBox(double frame, TRectD& bBox,
                                 const TRenderSettings& info) {
  bBox = TConsts::infiniteRectD;
  return true;
}

//------------------------------------------------------------

bool Iwa_MotionFlowFx::canHandle(const TRenderSettings& info, double frame) {
  return true;
}

/*------------------------------------------------------------
 Since there is a possibility that the reference object is moving,
 Change the alias every frame
------------------------------------------------------------*/

std::string Iwa_MotionFlowFx::getAlias(double frame,
                                       const TRenderSettings& info) const {
  std::string alias = getFxType();
  alias += "[";

  // alias of the effects related to the input ports separated by commas
  // a port that is not connected to an alias blank (empty string)

  std::string paramalias("");
  for (int i = 0; i < getParams()->getParamCount(); i++) {
    TParam* param = getParams()->getParam(i);
    paramalias += param->getName() + "=" + param->getValueAlias(frame, 3);
  }
  unsigned long id = getIdentifier();
  return alias + std::to_string(frame) + "," + std::to_string(id) + paramalias +
         "]";
}
FX_PLUGIN_IDENTIFIER(Iwa_MotionFlowFx, "iwa_MotionFlowFx")
