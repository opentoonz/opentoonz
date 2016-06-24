

#include "stdfx.h"
#include "tfxparam.h"
#include "tparamset.h"

#include "toonz/tdistort.h"

//****************************************************************************
//    Local namespace stuff
//****************************************************************************

namespace {

class KaleidoDistorter final : public TDistorter {
  double m_angle;
  TAffine m_aff;
  TPointD m_shift;

public:
  KaleidoDistorter(double angle, const TAffine &aff, const TPointD shift)
      : m_angle(angle), m_aff(aff), m_shift(shift) {}

  TPointD map(const TPointD &p) const override { return TPointD(); }
  int maxInvCount() const override { return 1; }

  int invMap(const TPointD &p, TPointD *results) const override;
};

//-------------------------------------------------------------------

int KaleidoDistorter::invMap(const TPointD &p, TPointD *results) const {
  TPointD q(m_aff * p);

  // Build p's angular position
  double qAngle = atan2(q.y, q.x);
  if (qAngle < 0.0) qAngle += 2.0 * M_PI;

  assert(qAngle >= 0.0);

  int section  = tfloor(qAngle / m_angle);
  bool reflect = (bool)(section % 2);

  double normQ = norm(q);
  if (reflect) {
    double newAngle = qAngle - (section + 1) * m_angle;

    results[0].x = normQ * cos(newAngle) + m_shift.x;
    results[0].y = -normQ * sin(newAngle) + m_shift.y;
  } else {
    double newAngle = qAngle - section * m_angle;

    results[0].x = normQ * cos(newAngle) + m_shift.x;
    results[0].y = normQ * sin(newAngle) + m_shift.y;
  }

  return 1;
}

}  // namespace

//****************************************************************************
//    Kaleido Fx
//****************************************************************************

class KaleidoFx final : public TStandardRasterFx {
  FX_PLUGIN_DECLARATION(KaleidoFx)

  TRasterFxPort m_input;
  TPointParamP m_center;
  TDoubleParamP m_angle;
  TIntParamP m_count;

public:
  KaleidoFx() : m_center(TPointD()), m_angle(0.0), m_count(3) {
    m_center->getX()->setMeasureName("fxLength");
    m_center->getY()->setMeasureName("fxLength");
    m_angle->setMeasureName("angle");

    bindParam(this, "center", m_center);
    bindParam(this, "angle", m_angle);
    bindParam(this, "count", m_count);

    addInputPort("Source", m_input);

    m_count->setValueRange(1, 100);
  }

  ~KaleidoFx(){};

  bool doGetBBox(double frame, TRectD &bBox,
                 const TRenderSettings &info) override;

  void doDryCompute(TRectD &rect, double frame,
                    const TRenderSettings &ri) override;
  void doCompute(TTile &tile, double frame, const TRenderSettings &ri) override;
  int getMemoryRequirement(const TRectD &rect, double frame,
                           const TRenderSettings &info) override;

  bool canHandle(const TRenderSettings &info, double frame) override {
    return isAlmostIsotropic(info.m_affine);
  }

private:
  void buildSectionRect(TRectD &inRect, double angle);
  void rotate(TRectD &rect);

  TAffine buildInputReference(double frame, TRectD &inRect,
                              TRenderSettings &inInfo, const TRectD &outRect,
                              const TRenderSettings &outInfo);
};

//-------------------------------------------------------------------

void KaleidoFx::buildSectionRect(TRectD &inRect, double angle) {
  inRect.y0 = std::max(inRect.y0, 0.0);
  if (angle <= M_PI_2) {
    inRect.x0 = std::max(inRect.x0, 0.0);
    inRect.y1 = std::min(inRect.y1, inRect.x1 * tan(angle));
  }
}

//-------------------------------------------------------------------

void KaleidoFx::rotate(TRectD &rect) {
  TPointD pMax(std::max(-rect.x0, rect.x1), std::max(-rect.y0, rect.y1));
  double normPMax = norm(pMax);

  rect = TRectD(-normPMax, -normPMax, normPMax, normPMax);
}

//-------------------------------------------------------------------

TAffine KaleidoFx::buildInputReference(double frame, TRectD &inRect,
                                       TRenderSettings &inInfo,
                                       const TRectD &outRect,
                                       const TRenderSettings &outInfo) {
  double scale = fabs(sqrt(outInfo.m_affine.det()));
  double angle = M_PI / m_count->getValue();

  inInfo.m_affine = TRotation(-m_angle->getValue(frame) - angle) *
                    TScale(scale).place(m_center->getValue(frame), TPointD());

  TAffine outRefToInRef(inInfo.m_affine * outInfo.m_affine.inv());

  // Build the input bounding box
  TRectD inBBox;
  m_input->getBBox(frame, inBBox, inInfo);

  // Rotate the output rect in the input reference. This is required since the
  // rotational
  // deformation may rotate points outside the rect, inside it.
  TRectD outRect_inputRef(outRefToInRef * outRect);
  rotate(outRect_inputRef);

  // Intersect with the useful kaleido region
  inRect = inBBox * outRect_inputRef;
  buildSectionRect(inRect, angle);

  return outRefToInRef;
}

//-------------------------------------------------------------------

bool KaleidoFx::doGetBBox(double frame, TRectD &bBox,
                          const TRenderSettings &info) {
  // Remember: info.m_affine MUST NOT BE CONSIDERED in doGetBBox's
  // implementation

  // Retrieve the input bbox without applied affines.

  if (!m_input.getFx()) return false;

  double angle = M_PI / m_count->getValue();

  TRenderSettings inInfo(info);
  inInfo.m_affine = TRotation(-m_angle->getValue(frame) - angle) *
                    TTranslation(-m_center->getValue(frame));

  if (!m_input->getBBox(frame, bBox, inInfo)) return false;

  TRectD infiniteRect(TConsts::infiniteRectD);

  TRectD kaleidoRect((m_count->getValue() > 1) ? 0.0 : infiniteRect.x0, 0.0,
                     infiniteRect.x1, infiniteRect.y1);

  bBox *= kaleidoRect;
  if (bBox.x0 == infiniteRect.x0 || bBox.x1 == infiniteRect.x1 ||
      bBox.y1 == infiniteRect.y1) {
    bBox = infiniteRect;
    return true;
  }

  buildSectionRect(bBox, angle);

  // Now, we must rotate the bBox in order to obtain the kaleidoscoped box
  rotate(bBox);

  // Finally, bring it back to standard reference
  bBox = inInfo.m_affine.inv() * bBox;

  return true;
}

//-------------------------------------------------------------------

void KaleidoFx::doDryCompute(TRectD &rect, double frame,
                             const TRenderSettings &info) {
  if (!m_input.isConnected()) return;

  if (fabs(info.m_affine.det()) < TConsts::epsilon) return;

  // Build the input reference
  TRectD inRect;
  TRenderSettings inInfo(info);

  TAffine outRefToInRef(buildInputReference(frame, inRect, inInfo, rect, info));

  if (inRect.getLx() <= 0.0 || inRect.getLy() <= 0.0) return;

  inRect = inRect.enlarge(1.0);  // tdistort() seems to need it

  // Allocate a corresponding input tile and calculate it
  m_input->dryCompute(inRect, frame, inInfo);
}

//-------------------------------------------------------------------

void KaleidoFx::doCompute(TTile &tile, double frame,
                          const TRenderSettings &info) {
  if (!m_input.isConnected()) return;

  if (fabs(info.m_affine.det()) < TConsts::epsilon) return;

  // Build the output rect
  TDimension tileSize(tile.getRaster()->getSize());
  TRectD tileRect(tile.m_pos, TDimensionD(tileSize.lx, tileSize.ly));

  // Build the input reference
  TRectD inRect;
  TRenderSettings inInfo(info);

  TAffine outRefToInRef(
      buildInputReference(frame, inRect, inInfo, tileRect, info));

  if (inRect.getLx() <= 0.0 || inRect.getLy() <= 0.0) return;

  inRect = inRect.enlarge(1.0);  // tdistort() seems to need it

  // Allocate a corresponding input tile and calculate it
  TTile inTile;
  TDimension inDim(tceil(inRect.getLx()), tceil(inRect.getLy()));

  m_input->allocateAndCompute(inTile, inRect.getP00(), inDim, tile.getRaster(),
                              frame, inInfo);

  // Now, perform kaleido
  double angle = M_PI / m_count->getValue();
  KaleidoDistorter distorter(angle, outRefToInRef, -inRect.getP00());

  TRasterP inRas(inTile.getRaster());
  TRasterP tileRas(tile.getRaster());

  distort(tileRas, inRas, distorter, convert(tile.m_pos));
}

//------------------------------------------------------------------

int KaleidoFx::getMemoryRequirement(const TRectD &rect, double frame,
                                    const TRenderSettings &info) {
  if (!m_input.isConnected()) return 0;

  if (fabs(info.m_affine.det()) < TConsts::epsilon) return 0;

  // Build the input reference
  TRectD inRect;
  TRenderSettings inInfo(info);

  TAffine outRefToInRef(buildInputReference(frame, inRect, inInfo, rect, info));

  if (inRect.getLx() <= 0.0 || inRect.getLy() <= 0.0) return 0;

  inRect = inRect.enlarge(1.0);  // tdistort() seems to need it

  return TRasterFx::memorySize(inRect, info.m_bpp);
}

//------------------------------------------------------------------

FX_PLUGIN_IDENTIFIER(KaleidoFx, "kaleidoFx");
