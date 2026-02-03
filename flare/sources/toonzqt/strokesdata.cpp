

#include "toonzqt/strokesdata.h"
#include "tthreadmessage.h"
#include "tstroke.h"
#include "tpalette.h"
#include "toonzqt/rasterimagedata.h"
#include "toonz/toonzimageutils.h"
#include "toonz/trasterimageutils.h"
#include "toonz/stage.h"

using namespace std;

//=============================================================================
namespace {
//-----------------------------------------------------------------------------

int findStroke(const TVectorImageP &img, TStroke *stroke, const TAffine &aff) {
  TRectD strokeBBox = aff * stroke->getBBox();
  int count         = img->getStrokeCount();
  for (int i = 0; i < count; i++) {
    TStroke *s  = img->getStroke(i);
    TRectD bbox = s->getBBox();
    if (tdistance2(bbox.getP00(), strokeBBox.getP00()) +
            tdistance2(bbox.getP11(), strokeBBox.getP11()) >
        0.001)
      continue;
    return i;
  }
  return -1;
}

//-----------------------------------------------------------------------------

TAffine findOffset(const TVectorImageP &srcImg, const TVectorImageP &img) {
  TAffine offset;

  TVectorImageP tarImg = img;
  if (!tarImg) return offset;
  if (tarImg->getStrokeCount() == 0 || srcImg->getStrokeCount() == 0)
    return offset;
  bool done = false;
  int i;
  while (!done)
    for (i = 0; i < (int)srcImg->getStrokeCount(); i++) {
      TStroke *stroke = srcImg->getStroke(i);
      assert(stroke);
      if (findStroke(tarImg, stroke, offset) >= 0) {
        offset = offset * TTranslation(10, -10);
        break;
      }
      done = true;
    }
  return offset;
}

//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------

TStroke getStrokeByRect(TRectD r) {
  TStroke stroke;
  if (r.isEmpty()) return stroke;
  vector<TThickPoint> points;
  points.push_back(r.getP00());
  points.push_back((r.getP00() + r.getP01()) * 0.5);
  points.push_back(r.getP01());
  points.push_back((r.getP01() + r.getP11()) * 0.5);
  points.push_back(r.getP11());
  points.push_back((r.getP11() + r.getP10()) * 0.5);
  points.push_back(r.getP10());
  points.push_back((r.getP10() + r.getP00()) * 0.5);
  points.push_back(r.getP00());
  stroke.reshape(&(points[0]), points.size());
  stroke.setSelfLoop(true);
  return stroke;
}

//-----------------------------------------------------------------------------
}  // namespace
//-----------------------------------------------------------------------------

//=============================================================================
// StrokesData
//-----------------------------------------------------------------------------

void StrokesData::setImage(TVectorImageP image, const std::set<int> &indices) {
  if (!image) return;
  if (indices.empty()) return;

  // indices e' un set; splitImage si aspetta un vector
  vector<int> indicesV(indices.begin(), indices.end());
  QMutexLocker lock(image->getMutex());
  m_image = image->splitImage(indicesV, false);
  if (m_image->getPalette() == 0) {
    // nel caso lo stroke sia un path (e quindi senza palette)
    m_image->setPalette(new TPalette());
  }
}

//-----------------------------------------------------------------------------

void StrokesData::getImage(TVectorImageP image, std::set<int> &indices,
                           bool insert) const {
  if (!m_image) return;

  TVectorImageP srcImg = m_image;

  QMutexLocker lock(image->getMutex());
  if (insert) {
    TAffine offset    = findOffset(srcImg, image);
    UINT oldImageSize = image->getStrokeCount();

    int insertAt      = image->mergeImage(srcImg, offset, false);
    UINT newImageSize = image->getStrokeCount();
    indices.clear();

    if (insertAt == 0)
      for (UINT sI = oldImageSize; sI < newImageSize; sI++) indices.insert(sI);
    else
      for (UINT sI = oldImageSize; sI < newImageSize; sI++)
        indices.insert(sI - oldImageSize + insertAt);
  } else {
    std::vector<int> indicesToInsert(indices.begin(), indices.end());
    if (indicesToInsert.empty()) return;
    image->insertImage(srcImg, indicesToInsert);
  }
}

//-----------------------------------------------------------------------------

ToonzImageData *StrokesData::toToonzImageData(
    const TToonzImageP &imageToPaste) const {
  double dpix, dpiy;
  imageToPaste->getDpi(dpix, dpiy);
  assert(dpix != 0 && dpiy != 0);
  TScale sc(dpix / Stage::inch, dpiy / Stage::inch);

  TRectD bbox = sc * m_image->getBBox();
  bbox.x0     = tfloor(bbox.x0);
  bbox.y0     = tfloor(bbox.y0);
  bbox.x1     = tceil(bbox.x1);
  bbox.y1     = tceil(bbox.y1);
  TDimension size(bbox.getLx(), bbox.getLy());
  TToonzImageP app = ToonzImageUtils::vectorToToonzImage(
      m_image, sc, m_image->getPalette(), bbox.getP00(), size, 0, true);

  vector<TRectD> rects;
  vector<TStroke> strokes;
  TStroke stroke = getStrokeByRect(bbox);
  strokes.push_back(stroke);
  ToonzImageData *data = new ToonzImageData();
  data->setData(app->getRaster(), m_image->getPalette(), dpix, dpiy,
                TDimension(), rects, strokes, strokes, TAffine());
  return data;
}

//-----------------------------------------------------------------------------

FullColorImageData *StrokesData::toFullColorImageData(
    const TRasterImageP &imageToPaste) const {
  double dpix, dpiy;
  imageToPaste->getDpi(dpix, dpiy);
  assert(dpix != 0 && dpiy != 0);
  TScale sc(dpix / Stage::inch, dpiy / Stage::inch);

  TRectD bbox = sc * m_image->getBBox();
  bbox.x0     = tfloor(bbox.x0);
  bbox.y0     = tfloor(bbox.y0);
  bbox.x1     = tceil(bbox.x1);
  bbox.y1     = tceil(bbox.y1);
  TDimension size(bbox.getLx(), bbox.getLy());
  TRasterImageP app = TRasterImageUtils::vectorToFullColorImage(
      m_image, sc, m_image->getPalette(), bbox.getP00(), size, 0, true);

  vector<TRectD> rects;
  vector<TStroke> strokes;
  TStroke stroke = getStrokeByRect(bbox);
  strokes.push_back(stroke);
  FullColorImageData *data = new FullColorImageData();
  data->setData(app->getRaster(), m_image->getPalette(), dpix, dpiy,
                TDimension(), rects, strokes, strokes, TAffine());
  return data;
}
