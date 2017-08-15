

#include "toonz/levelproperties.h"

// TnzLib includes
#include "toonz/stage.h"

//**********************************************************************************
//    LevelProperties::Options  implementation
//**********************************************************************************

LevelOptions::LevelOptions()
    : m_dpi(Stage::standardDpi)
    , m_subsampling(1)
    , m_antialias(0)
    , m_dpiPolicy(DP_ImageDpi)
    , m_whiteTransp(false)
    , m_premultiply(false) {}

//-----------------------------------------------------------------------------

bool LevelOptions::operator==(const LevelOptions &other) const {
  return (m_premultiply == other.m_premultiply &&
          m_whiteTransp == other.m_whiteTransp &&
          m_dpiPolicy == other.m_dpiPolicy &&
          m_antialias == other.m_antialias &&
          (m_dpiPolicy == LevelOptions::DP_ImageDpi || m_dpi == other.m_dpi));
}

//**********************************************************************************
//    LevelProperties  implementation
//**********************************************************************************

LevelProperties::LevelProperties()
    : m_imageDpi()
    , m_creator("")
    , m_imageRes(0, 0)
    , m_bpp(32)
    , m_loadAtOnce(false)
    , m_dirtyFlag(true)
    , m_forbidden(false)
    , m_hasAlpha(false) {}

//-----------------------------------------------------------------------------

void LevelProperties::setDpiPolicy(LevelProperties::DpiPolicy dpiPolicy) {
  m_options.m_dpiPolicy = LevelOptions::DpiPolicy(dpiPolicy);
}

//-----------------------------------------------------------------------------

LevelProperties::DpiPolicy LevelProperties::getDpiPolicy() const {
  return LevelProperties::DpiPolicy(m_options.m_dpiPolicy);
}

//-----------------------------------------------------------------------------

void LevelProperties::setDpi(const TPointD &dpi) { m_options.m_dpi = dpi.x; }

//-----------------------------------------------------------------------------

void LevelProperties::setDpi(double dpi) { m_options.m_dpi = dpi; }

//-----------------------------------------------------------------------------

TPointD LevelProperties::getDpi() const {
  return TPointD(m_options.m_dpi, m_options.m_dpi);
}

//-----------------------------------------------------------------------------

void LevelProperties::setSubsampling(int s) { m_options.m_subsampling = s; }

//-----------------------------------------------------------------------------

int LevelProperties::getSubsampling() const { return m_options.m_subsampling; }

//-----------------------------------------------------------------------------

void LevelProperties::setDirtyFlag(bool on) { m_dirtyFlag = on; }

//-----------------------------------------------------------------------------

bool LevelProperties::getDirtyFlag() const { return m_dirtyFlag; }

//-----------------------------------------------------------------------------

TDimension LevelProperties::getImageRes() const { return m_imageRes; }

//-----------------------------------------------------------------------------

void LevelProperties::setImageDpi(const TPointD &dpi) {
  m_imageDpi = dpi;
  if (dpi.x <= 0 || dpi.y <= 0)
    m_options.m_dpiPolicy = LevelOptions::DP_CustomDpi;
}

//-----------------------------------------------------------------------------

TPointD LevelProperties::getImageDpi() const { return m_imageDpi; }

//-----------------------------------------------------------------------------

void LevelProperties::setImageRes(const TDimension &d) { m_imageRes = d; }

//-----------------------------------------------------------------------------

void LevelProperties::setBpp(int bpp) { m_bpp = bpp; }

//-----------------------------------------------------------------------------

int LevelProperties::getBpp() const { return m_bpp; }
