

#include "toonzqt/rasterimagedata.h"
#include "toonzqt/strokesdata.h"
#include "tpaletteutil.h"
#include "trasterimage.h"
#include "toonz/stage.h"
#include "toonz/tcenterlinevectorizer.h"
#include "toonz/ttileset.h"
#include "toonz/toonzimageutils.h"
#include "toonz/toonzscene.h"
#include "toonz/sceneproperties.h"
#include "toonz/fullcolorpalette.h"

#include <memory>

namespace {
TVectorImageP vectorize(const TImageP &source, const TRectD &rect,
                        const VectorizerConfiguration &config,
                        TAffine transform) {
  VectorizerCore vc;
  TVectorImageP vi =
      vc.vectorize(source.getPointer(), config, source->getPalette());
  assert(vi);
  vi->setPalette(source->getPalette());

  double dpiX, dpiY;
  TToonzImageP ti(source);
  TRasterImageP ri(source);
  if (ti)
    ti->getDpi(dpiX, dpiY);
  else if (ri)
    ri->getDpi(dpiX, dpiY);
  else
    return vi;

  TScale sc(dpiX / Stage::inch, dpiY / Stage::inch);
  TTranslation tr(rect.getP00());

  for (int i = 0; i < (int)vi->getStrokeCount(); i++) {
    TStroke *stroke = vi->getStroke(i);
    stroke->transform((sc.inv() * tr) * transform, true);
  }
  return vi;
}

}  // namespace

//===================================================================
// RasterImageData
//-------------------------------------------------------------------

RasterImageData::RasterImageData()
    : m_dpiX(0), m_dpiY(0), m_transformation(), m_dim() {}

//-------------------------------------------------------------------

RasterImageData::~RasterImageData() = default;

//===================================================================
// ToonzImageData
//-------------------------------------------------------------------

ToonzImageData::ToonzImageData()
    : RasterImageData(), m_copiedRaster(0), m_palette(new TPalette()) {}

//-------------------------------------------------------------------

ToonzImageData::ToonzImageData(const ToonzImageData &src)
    : m_copiedRaster(src.m_copiedRaster)
    , m_palette(src.m_palette)
    , m_usedStyles(src.m_usedStyles) {
  m_dpiX            = src.m_dpiX;
  m_dpiY            = src.m_dpiY;
  m_rects           = src.m_rects;
  m_strokes         = src.m_strokes;
  m_transformation  = src.m_transformation;
  m_originalStrokes = src.m_originalStrokes;
  m_dim             = src.m_dim;
  assert(m_palette);
}

//-------------------------------------------------------------------

ToonzImageData::~ToonzImageData() = default;

//-------------------------------------------------------------------

void ToonzImageData::setData(const TRasterP &copiedRaster,
                             const TPaletteP &palette, double dpiX, double dpiY,
                             const TDimension &dim,
                             const std::vector<TRectD> &rects,
                             const std::vector<TStroke> &strokes,
                             const std::vector<TStroke> &originalStrokes,
                             const TAffine &transformation) {
  m_copiedRaster    = copiedRaster;
  m_palette         = palette;
  m_dpiX            = dpiX;
  m_dpiY            = dpiY;
  m_rects           = rects;
  m_strokes         = strokes;
  m_transformation  = transformation;
  m_originalStrokes = originalStrokes;
  m_dim             = dim;

  // Build a list of used styles
  TToonzImageP ti(m_copiedRaster, m_copiedRaster->getBounds());
  ToonzImageUtils::getUsedStyles(m_usedStyles, ti);
}

//-------------------------------------------------------------------

void ToonzImageData::getData(TRasterP &copiedRaster, double &dpiX, double &dpiY,
                             std::vector<TRectD> &rects,
                             std::vector<TStroke> &strokes,
                             std::vector<TStroke> &originalStrokes,
                             TAffine &transformation,
                             TPalette *targetPalette) const {
  if (!m_copiedRaster || (m_rects.empty() && m_strokes.empty())) return;

  copiedRaster = m_copiedRaster->clone();
  dpiX         = m_dpiX;
  dpiY         = m_dpiY;
  assert(m_palette);

  rects.assign(m_rects.begin(), m_rects.end());
  strokes.assign(m_strokes.begin(), m_strokes.end());
  originalStrokes.assign(m_originalStrokes.begin(), m_originalStrokes.end());

  transformation     = m_transformation;
  TRasterCM32P cmRas = copiedRaster;

  // Manage temporary palette to avoid leak on early return
  std::unique_ptr<TPalette> tempPalette;
  if (!targetPalette) {
    tempPalette.reset(new TPalette());
    targetPalette = tempPalette.get();
  }

  if (!cmRas) return;  // tempPalette cleans up automatically

  std::set<int> usedStyles(m_usedStyles);
  TToonzImageP ti(cmRas, cmRas->getBounds());
  if (usedStyles.empty()) ToonzImageUtils::getUsedStyles(usedStyles, ti);

  std::map<int, int> indexTable;
  mergePalette(targetPalette, indexTable, m_palette, usedStyles);
  ToonzImageUtils::scrambleStyles(ti, indexTable);
  ti->setPalette(m_palette.getPointer());

  if (tempPalette) tempPalette.release();  // ownership transferred to caller
}

//-------------------------------------------------------------------

StrokesData *ToonzImageData::toStrokesData(ToonzScene *scene) const {
  assert(scene);
  TRectD rect;
  if (!m_rects.empty())
    rect = m_rects[0];
  else if (!m_strokes.empty())
    rect = m_strokes[0].getBBox();

  for (const auto &r : m_rects) rect += r;
  for (const auto &s : m_strokes) rect += s.getBBox();

  TToonzImageP image(m_copiedRaster, m_copiedRaster->getBounds());
  image->setPalette(m_palette.getPointer());
  image->setDpi(m_dpiX, m_dpiY);

  const VectorizerParameters &vParams =
      *scene->getProperties()->getVectorizerParameters();

  CenterlineConfiguration cConf = vParams.getCenterlineConfiguration(0.0);
  NewOutlineConfiguration oConf = vParams.getOutlineConfiguration(0.0);

  const VectorizerConfiguration &config =
      vParams.m_isOutline ? static_cast<const VectorizerConfiguration &>(oConf)
                          : static_cast<const VectorizerConfiguration &>(cConf);

  TVectorImageP vi = vectorize(image, rect, config, m_transformation);

  StrokesData *sd = new StrokesData();

  std::set<int> indexes;
  for (int i = 0; i < (int)vi->getStrokeCount(); i++) indexes.insert(i);

  sd->setImage(vi, indexes);
  return sd;
}

//-------------------------------------------------------------------

int ToonzImageData::getMemorySize() const {
  int size = 0;
  for (const auto &s : m_strokes)
    size += s.getControlPointCount() * sizeof(TThickPoint) + 100;
  for (const auto &s : m_originalStrokes)
    size += s.getControlPointCount() * sizeof(TThickPoint) + 100;
  return size + sizeof(*(m_copiedRaster.getPointer())) +
         sizeof(*(m_palette.getPointer())) + sizeof(*this);
}

//===================================================================
// FullColorImageData
//-------------------------------------------------------------------

FullColorImageData::FullColorImageData()
    : RasterImageData(), m_copiedRaster(0), m_palette(new TPalette()) {}

//-------------------------------------------------------------------

FullColorImageData::FullColorImageData(const FullColorImageData &src)
    : m_copiedRaster(src.m_copiedRaster), m_palette(src.m_palette) {
  m_dpiX            = src.m_dpiX;
  m_dpiY            = src.m_dpiY;
  m_rects           = src.m_rects;
  m_strokes         = src.m_strokes;
  m_transformation  = src.m_transformation;
  m_originalStrokes = src.m_originalStrokes;
  m_dim             = src.m_dim;
}

//-------------------------------------------------------------------

FullColorImageData::~FullColorImageData() = default;

//-------------------------------------------------------------------

void FullColorImageData::setData(const TRasterP &copiedRaster,
                                 const TPaletteP &palette, double dpiX,
                                 double dpiY, const TDimension &dim,
                                 const std::vector<TRectD> &rects,
                                 const std::vector<TStroke> &strokes,
                                 const std::vector<TStroke> &originalStrokes,
                                 const TAffine &transformation) {
  m_copiedRaster    = copiedRaster;
  m_palette         = palette;
  m_dpiX            = dpiX;
  m_dpiY            = dpiY;
  m_rects           = rects;
  m_strokes         = strokes;
  m_originalStrokes = originalStrokes;
  m_transformation  = transformation;
  m_dim             = dim;
}

//-------------------------------------------------------------------

void FullColorImageData::getData(TRasterP &copiedRaster, double &dpiX,
                                 double &dpiY, std::vector<TRectD> &rects,
                                 std::vector<TStroke> &strokes,
                                 std::vector<TStroke> &originalStrokes,
                                 TAffine &transformation,
                                 TPalette *targetPalette) const {
  if (!m_copiedRaster || (m_rects.empty() && m_strokes.empty())) return;

  copiedRaster = m_copiedRaster->clone();
  dpiX         = m_dpiX;
  dpiY         = m_dpiY;

  rects.assign(m_rects.begin(), m_rects.end());
  strokes.assign(m_strokes.begin(), m_strokes.end());
  originalStrokes.assign(m_originalStrokes.begin(), m_originalStrokes.end());

  transformation = m_transformation;

  TRasterP ras = copiedRaster;
  if (!ras) return;
  if (!m_palette) return;

  std::unique_ptr<TPalette> tempPalette;
  if (!targetPalette) {
    tempPalette.reset(new TPalette());
    targetPalette = tempPalette.get();
  }

  std::set<int> usedStyles;
  TRasterImageP ri(ras);

  for (int i = 0; i < m_palette->getPageCount(); i++) {
    TPalette::Page *page = m_palette->getPage(i);
    for (int j = 0; j < page->getStyleCount(); j++)
      usedStyles.insert(page->getStyleId(j));
  }
  std::map<int, int> indexTable;
  mergePalette(targetPalette, indexTable, m_palette, usedStyles);
  ri->setPalette(m_palette.getPointer());

  if (tempPalette) tempPalette.release();  // ownership transferred to caller
}

//-------------------------------------------------------------------

StrokesData *FullColorImageData::toStrokesData(ToonzScene *scene) const {
  assert(scene);
  TRectD rect;
  if (!m_rects.empty())
    rect = m_rects[0];
  else if (!m_strokes.empty())
    rect = m_strokes[0].getBBox();

  for (const auto &r : m_rects) rect += r;
  for (const auto &s : m_strokes) rect += s.getBBox();

  TRasterImageP image(m_copiedRaster);
  image->setPalette(FullColorPalette::instance()->getPalette(scene));
  image->setDpi(m_dpiX, m_dpiY);

  const VectorizerParameters *vParams =
      scene->getProperties()->getVectorizerParameters();
  assert(vParams);

  std::unique_ptr<VectorizerConfiguration> config(
      vParams->getCurrentConfiguration(0.0));
  TVectorImageP vi = vectorize(image, rect, *config, m_transformation);

  StrokesData *sd = new StrokesData();

  std::set<int> indexes;
  for (int i = 0; i < (int)vi->getStrokeCount(); i++) indexes.insert(i);

  sd->setImage(vi, indexes);
  return sd;
}

//-------------------------------------------------------------------

int FullColorImageData::getMemorySize() const {
  int size = 0;
  for (const auto &s : m_strokes)
    size += s.getControlPointCount() * sizeof(TThickPoint) + 100;
  for (const auto &s : m_originalStrokes)
    size += s.getControlPointCount() * sizeof(TThickPoint) + 100;
  return size + sizeof(*(m_copiedRaster.getPointer())) +
         sizeof(*(m_palette.getPointer())) + sizeof(*this);
}
