

#include "trasterimage.h"
#include "trop.h"

//---------------------------------------------------------

TRasterImage::TRasterImage()
    : m_mainRaster()
    , m_patchRaster()
    , m_iconRaster()
    , m_dpix(0)
    , m_dpiy(0)
    , m_name("")
    , m_savebox()
    //, m_hPos(0.0)
    , m_isOpaque(false)
    , m_isScanBW(false)
    , m_offset(0, 0)
    , m_subsampling(1) {}

//---------------------------------------------------------

TRasterImage::TRasterImage(const TRasterP &ras)
    : m_mainRaster(ras)
    , m_patchRaster()
    , m_iconRaster()
    , m_dpix(0)
    , m_dpiy(0)
    , m_name("")
    , m_savebox(0, 0, ras->getLx() - 1, ras->getLy() - 1)
    //, m_hPos(0.0)
    , m_isOpaque(false)
    , m_isScanBW(false)
    , m_offset(0, 0)
    , m_subsampling(1) {}

void TRasterImage::setRaster(const TRasterP &raster) {
  m_mainRaster = raster;
  m_savebox    = TRect(0, 0, raster->getLx() - 1, raster->getLy() - 1);
}

//---------------------------------------------------------

TRasterImage::TRasterImage(const TRasterImage &src)
    : m_mainRaster(src.m_mainRaster)
    , m_patchRaster(src.m_patchRaster)
    , m_iconRaster(src.m_iconRaster)
    , m_dpix(src.m_dpix)
    , m_dpiy(src.m_dpiy)
    , m_name(src.m_name)
    , m_savebox(src.m_savebox)
    //, m_hPos(src.m_hPos)
    , m_isOpaque(src.m_isOpaque)
    , m_isScanBW(src.m_isScanBW)
    , m_offset(src.m_offset)
    , m_subsampling(src.m_subsampling) {
  if (m_mainRaster) m_mainRaster   = m_mainRaster->clone();
  if (m_patchRaster) m_patchRaster = m_patchRaster->clone();
  if (m_iconRaster) m_iconRaster   = m_iconRaster->clone();
}
//---------------------------------------------------------

TRasterImage::~TRasterImage() {}

//---------------------------------------------------------

TImage *TRasterImage::cloneImage() const { return new TRasterImage(*this); }

//---------------------------------------------------------

void TRasterImage::setSubsampling(int s) { m_subsampling = s; }

//---------------------------------------------------------

void TRasterImage::makeIcon(const TRaster32P &dstRas) {
#ifndef TNZCORE_LIGHT
  assert(dstRas && dstRas->getLx() > 0 && dstRas->getLy() > 0);

  TRasterP &srcRas = m_mainRaster;
  if (!srcRas || srcRas->getLx() <= 0 || srcRas->getLy() <= 0) {
    dstRas->clear();
    return;
  }

  double dpix = m_dpix, dpiy = m_dpiy;
  if (dpix == 0) dpix = 1;
  if (dpiy == 0) dpiy = 1;
  double sx   = (double)dstRas->getLx() * dpix / (double)srcRas->getLx();
  double sy   = (double)dstRas->getLy() * dpiy / (double)srcRas->getLy();
  double sc   = std::max(sx, sy);
  TAffine aff = TScale(sc / dpix, sc / dpiy)
                    .place(srcRas->getCenterD(), dstRas->getCenterD());

  TRop::resample(dstRas, srcRas, aff);
#endif
}

//---------------------------------------------------------
