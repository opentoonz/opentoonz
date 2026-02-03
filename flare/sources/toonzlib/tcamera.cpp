

#include "toonz/tcamera.h"
#include "toonz/stage.h"
#include "tstream.h"
#include "texception.h"

//=============================================================================
// TCamera

TCamera::TCamera()
    //: m_size(12, 9), m_res(768, 576), m_xPrevalence(true)
    //: m_size(36, 20.25),
    : m_size(16, 9), m_res(1920, 1080), m_xPrevalence(true) {}

//-------------------------------------------------------------------

void TCamera::setSize(const TDimensionD &size, bool preserveDpi,
                      bool preserveAR) {
  double currAR   = getAspectRatio();
  TPointD currDpi = getDpi();
  m_size.lx       = size.lx;
  if (preserveAR)
    m_size.ly = m_size.lx / currAR;  // WARNING! if also preserveDpi==true, the
                                     // AR could lose precision...
  else
    m_size.ly = size.ly;

  if (!preserveDpi) return;

  m_res.lx  = troundp(currDpi.x * m_size.lx);
  m_res.ly  = troundp(currDpi.y * m_size.ly);
  m_size.lx = m_res.lx / currDpi.x;
  if (preserveAR)  //&& currDpi.x==currDpi.y)
  {
    double aux = m_size.lx / currAR - m_res.ly / currDpi.y;
    m_size.ly  = m_size.lx / currAR;
  } else
    m_size.ly = m_res.ly / currDpi.y;
}

//-------------------------------------------------------------------

double TCamera::getAspectRatio() const { return m_size.lx / m_size.ly; }

//-------------------------------------------------------------------

void TCamera::setRes(const TDimension &res) {
  assert(res.lx > 0);
  assert(res.ly > 0);

  if (m_res != res) {
    m_res          = res;
    m_interestRect = TRect();
  }
}

//-------------------------------------------------------------------

TPointD TCamera::getDpi() const {
  TPointD dpi;
  if (m_size.lx > 0 && m_size.ly > 0) {
    dpi.x = m_res.lx / m_size.lx;
    dpi.y = m_res.ly / m_size.ly;
  }
  return dpi;
}

//-------------------------------------------------------------------

bool TCamera::isPixelSquared() const {
  return areAlmostEqual(m_res.lx * m_size.ly, m_res.ly * m_size.lx);
}

//-------------------------------------------------------------------

TAffine TCamera::getStageToCameraRef() const {
  return TAffine(m_res.lx / (Stage::inch * m_size.lx), 0, 0.5 * m_res.lx, 0,
                 m_res.ly / (Stage::inch * m_size.ly), 0.5 * m_res.ly);
}

//-------------------------------------------------------------------

TAffine TCamera::getCameraToStageRef() const {
  const double factor = Stage::inch;

  TDimensionD cameraSize = getSize();
  cameraSize.lx *= factor;
  cameraSize.ly *= factor;
  TPointD center(0.5 * cameraSize.lx, 0.5 * cameraSize.ly);

  return TAffine(factor * m_size.lx / (double)m_res.lx, 0, -center.x, 0,
                 factor * m_size.ly / (double)m_res.ly, -center.y);
}

//-------------------------------------------------------------------

TRectD TCamera::getStageRect() const {
  TDimensionD cameraSize = getSize();
  const double factor    = Stage::inch;
  cameraSize.lx *= factor;
  cameraSize.ly *= factor;
  TRectD rect(cameraSize);
  rect -= 0.5 * (rect.getP00() + rect.getP11());
  return rect;
}

//-------------------------------------------------------------------

void TCamera::setInterestRect(const TRect &rect) {
  // enable to preview outside of the original camera rect
  m_interestRect = rect;
  return;
  //// Not using the TRect's common intersection. Unfortunately, in case
  //// the rect's coordinates have lx or ly < 0, the intersection returns
  //// the default (empty) rect. We want to maintain the coordinates instead.
  //// m_interestRect = rect * TRect(m_res);
  //
  // m_interestRect.x0 = std::max(rect.x0, 0);
  // m_interestRect.y0 = std::max(rect.y0, 0);
  // m_interestRect.x1 = std::min(rect.x1, m_res.lx - 1);
  // m_interestRect.y1 = std::min(rect.y1, m_res.ly - 1);
}

//-------------------------------------------------------------------

TRectD TCamera::getInterestStageRect() const {
  return getCameraToStageRef() * TRectD(m_interestRect.x0, m_interestRect.y0,
                                        m_interestRect.x1 + 1,
                                        m_interestRect.y1 + 1);
}

//-------------------------------------------------------------------

void TCamera::setInterestStageRect(const TRectD &rect) {
  TRectD cameraInterestRectD(getStageToCameraRef() * rect);

  setInterestRect(TRect(
      tfloor(cameraInterestRectD.x0), tfloor(cameraInterestRectD.y0),
      tceil(cameraInterestRectD.x1) - 1, tceil(cameraInterestRectD.y1) - 1));
}

//-------------------------------------------------------------------

void TCamera::saveData(TOStream &os) const {
  os.child("cameraSize") << m_size.lx << m_size.ly;
  os.child("cameraRes") << m_res.lx << m_res.ly;
  os.child("cameraXPrevalence") << (int)m_xPrevalence;
  os.child("interestRect") << m_interestRect.x0 << m_interestRect.y0
                           << m_interestRect.x1 << m_interestRect.y1;
}

//-------------------------------------------------------------------

void TCamera::loadData(TIStream &is) {
  std::string tagName;
  while (is.matchTag(tagName)) {
    if (tagName == "cameraSize" || tagName == "size")
      is >> m_size.lx >> m_size.ly;
    else if (tagName == "cameraRes" || tagName == "res")
      is >> m_res.lx >> m_res.ly;
    else if (tagName == "cameraXPrevalence") {
      int xPrevalence;
      is >> xPrevalence;
      m_xPrevalence = (bool)xPrevalence;
    } else if (tagName == "interestRect") {
      is >> m_interestRect.x0 >> m_interestRect.y0 >> m_interestRect.x1 >>
          m_interestRect.y1;
    } else
      throw TException("TCamera::loadData. unexpected tag: " + tagName);
    is.matchEndTag();
  }
}
