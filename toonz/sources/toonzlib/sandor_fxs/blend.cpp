

#include "blend.h"

// TPoint structure
#include "tgeometry.h"

// Palette - pixel functions
#include "tpalette.h"
#include "tpixelutils.h"

#include <vector>
#include <memory>

//=================================================================================

//===========================
//    Blur pattern class
//---------------------------

//! The BlurPattern class delineates the idea of a 'blur'
//! pattern from a number of random sample points taken
//! in a neighbourhood of the blurred pixel. The pattern
//! develops in a radial manner if specified, so that possible
//! 'obstacles' in the blur can be identified.

class BlurPattern {
public:
  typedef std::vector<TPoint> SamplePath;

  std::vector<TPoint> m_samples;
  std::vector<SamplePath> m_samplePaths;

  BlurPattern(double distance, unsigned int samplesCount, bool radial);
  ~BlurPattern() {}
};

//---------------------------------------------------------------------------------

// Builds the specified number of samples count, inside the specified distance
// from the origin. If the pattern is radial, paths to the samples points are
// calculated.
BlurPattern::BlurPattern(double distance, unsigned int samplesCount,
                         bool radial) {
  const double randFactor = 2.0 * distance / RAND_MAX;

  m_samples.resize(samplesCount);

  // Build the samples
  unsigned int i;
  for (i = 0; i < samplesCount; ++i) {
    // NOTE: The following method ensures a perfectly flat probability
    // distribution.

    TPoint candidatePoint(tround(rand() * randFactor - distance),
                          tround(rand() * randFactor - distance));
    double distanceSq = sq(distance);
    while (sq(candidatePoint.x) + sq(candidatePoint.y) > distanceSq)
      candidatePoint = TPoint(tround(rand() * randFactor - distance),
                              tround(rand() * randFactor - distance));

    m_samples[i] = candidatePoint;
  }

  m_samplePaths.resize(samplesCount);

  // If necessary, build the paths
  if (radial) {
    for (i = 0; i < samplesCount; ++i) {
      TPoint &sample = m_samples[i];

      int l = std::max(abs(sample.x), abs(sample.y));

      m_samplePaths[i].reserve(l);

      double dx = sample.x / (double)l;
      double dy = sample.y / (double)l;

      double x, y;
      int j;
      for (j = 0, x = dx, y = dy; j < l; x += dx, y += dy, ++j)
        m_samplePaths[i].push_back(TPoint(tround(x), tround(y)));
    }
  }
}

//=================================================================================

//=================================
//    Raster Selection classes
//---------------------------------

struct SelectionData {
  UCHAR m_selectedInk : 1;
  UCHAR m_selectedPaint : 1;
  UCHAR m_pureInk : 1;
  UCHAR m_purePaint : 1;
};

//=================================================================================

// Implements an array of selection infos using bitfields. It seems that
// bitfields are more optimized than
// using raw bits and bitwise operators, and use just the double of the space
// required with bit arrays.
class SelectionArrayPtr {
  std::unique_ptr<SelectionData[]> m_buffer;

public:
  inline void allocate(unsigned int count) {
    m_buffer.reset(new SelectionData[count]);
    memset(m_buffer.get(), 0, count * sizeof(SelectionData));
  }

  inline void destroy() { m_buffer.reset(); }

  inline SelectionData *data() const { return m_buffer.get(); }

  inline SelectionData *data() { return m_buffer.get(); }
};

//=================================================================================

// Bitmap used to store blend color selections and pure color informations.
class SelectionRaster {
  SelectionArrayPtr m_selection;

  int m_wrap;

public:
  SelectionRaster(TRasterCM32P cm);

  void updateSelection(TRasterCM32P cm, const BlendParam &param);

  SelectionData *data() const { return m_selection.data(); }

  SelectionData *data() { return m_selection.data(); }

  void destroy() { m_selection.destroy(); }

  bool isSelectedInk(int xy) const {
    return (m_selection.data() + xy)->m_selectedInk;
  }

  bool isSelectedInk(int x, int y) const {
    return isSelectedInk(x + y * m_wrap);
  }

  bool isSelectedPaint(int xy) const {
    return (m_selection.data() + xy)->m_selectedPaint;
  }

  bool isSelectedPaint(int x, int y) const {
    return isSelectedPaint(x + y * m_wrap);
  }

  bool isPureInk(int xy) const { return (m_selection.data() + xy)->m_pureInk; }

  bool isPureInk(int x, int y) const { return isPureInk(x + y * m_wrap); }

  bool isPurePaint(int xy) const {
    return (m_selection.data() + xy)->m_purePaint;
  }

  bool isPurePaint(int x, int y) const { return isPurePaint(x + y * m_wrap); }

  bool isToneColor(int xy) const { return !(isPureInk(xy) || isPurePaint(xy)); }

  bool isToneColor(int x, int y) const { return isToneColor(x + y * m_wrap); }
};

//---------------------------------------------------------------------------------

inline UCHAR linearSearch(const int *v, unsigned int vSize, int k) {
  const int *vEnd = v + vSize;
  for (; v < vEnd; ++v)
    if (*v == k) return 1;
  return 0;
}

//---------------------------------------------------------------------------------

// I've seen the std::binary_search go particularly slow... perhaps it was the
// debug mode,
// but I guess this is the fastest version possible.
inline UCHAR binarySearch(const int *v, unsigned int vSize, int k) {
  // NOTE: v.size() > 0 due to external restrictions. See SelectionRaster's
  // constructor.

  int a = -1, b, c = vSize;

  for (b = c >> 1; b != a; b = (a + c) >> 1) {
    if (v[b] == k)
      return 1;
    else if (k < v[b])
      c = b;
    else
      a = b;
  }

  return 0;
}

//---------------------------------------------------------------------------------

SelectionRaster::SelectionRaster(TRasterCM32P cm) {
  unsigned int lx = cm->getLx(), ly = cm->getLy(), wrap = cm->getWrap();
  unsigned int size = lx * ly;

  m_wrap = lx;

  m_selection.allocate(size);

  cm->lock();
  TPixelCM32 *pix, *pixBegin = (TPixelCM32 *)cm->getRawData();

  SelectionData *selData = data();

  unsigned int i, j;
  for (i = 0; i < ly; ++i) {
    pix = pixBegin + i * wrap;
    for (j = 0; j < lx; ++j, ++pix, ++selData) {
      selData->m_pureInk   = pix->getTone() == 0;
      selData->m_purePaint = pix->getTone() == 255;
    }
  }

  cm->unlock();
}

//---------------------------------------------------------------------------------

void SelectionRaster::updateSelection(TRasterCM32P cm,
                                      const BlendParam &param) {
  // Make a hard copy of color indexes. We do so since we absolutely prefer
  // having them SORTED!
  std::vector<int> cIndexes = param.colorsIndexes;
  std::sort(cIndexes.begin(), cIndexes.end());

  unsigned int lx = cm->getLx(), ly = cm->getLy(), wrap = cm->getWrap();

  // Scan each cm pixel, looking if its ink or paint is in param's colorIndexes.
  cm->lock();
  TPixelCM32 *pix, *pixBegin = (TPixelCM32 *)cm->getRawData();

  SelectionData *selData = data();

  const int *v =
      &cIndexes[0];  // NOTE: cIndexes.size() > 0 due to external check.
  unsigned int vSize = cIndexes.size();
  unsigned int i, j;

  // NOTE: It seems that linear searches are definitely best for small color
  // indexes.
  if (vSize > 50) {
    for (i = 0; i < ly; ++i) {
      pix = pixBegin + i * wrap;
      for (j = 0; j < lx; ++j, ++pix, ++selData) {
        selData->m_selectedInk   = binarySearch(v, vSize, pix->getInk());
        selData->m_selectedPaint = binarySearch(v, vSize, pix->getPaint());
      }
    }
  } else {
    for (i = 0; i < ly; ++i) {
      pix = pixBegin + i * wrap;
      for (j = 0; j < lx; ++j, ++pix, ++selData) {
        selData->m_selectedInk   = linearSearch(v, vSize, pix->getInk());
        selData->m_selectedPaint = linearSearch(v, vSize, pix->getPaint());
      }
    }
  }

  cm->unlock();
}

//=================================================================================

//========================
//    Blend functions
//------------------------

// Pixel whose channels are doubles. Used to store intermediate values for pixel
// blending.
struct DoubleRGBMPixel {
  double r;
  double g;
  double b;
  double m;

  DoubleRGBMPixel() : r(0.0), g(0.0), b(0.0), m(0.0) {}
};

//---------------------------------------------------------------------------------

const double maxTone = TPixelCM32::getMaxTone();

// Returns the ink & paint convex factors associated with passed tone.
inline void getFactors(int tone, double &inkFactor, double &paintFactor) {
  paintFactor = tone / maxTone;
  inkFactor   = (1.0 - paintFactor);
}

//---------------------------------------------------------------------------------

// Copies the cmIn paint and ink colors to the output rasters.
static void buildLayers(const TRasterCM32P &cmIn,
                        const std::vector<TPixel32> &palColors,
                        TRaster32P &inkRaster, TRaster32P &paintRaster) {
  // Separate cmIn by copying the ink & paint colors directly to the layer
  // rasters.
  TPixelCM32 *cmPix, *cmBegin = (TPixelCM32 *)cmIn->getRawData();
  TPixel32 *inkPix   = (TPixel32 *)inkRaster->getRawData();
  TPixel32 *paintPix = (TPixel32 *)paintRaster->getRawData();

  unsigned int i, j, lx = cmIn->getLx(), ly = cmIn->getLy(),
                     wrap = cmIn->getWrap();
  for (i = 0; i < ly; ++i) {
    cmPix = cmBegin + i * wrap;

    for (j = 0; j < lx; ++j, ++cmPix, ++inkPix, ++paintPix) {
      *inkPix   = palColors[cmPix->getInk()];
      *paintPix = palColors[cmPix->getPaint()];

      // Should pure colors be checked...?
    }
  }
}

//---------------------------------------------------------------------------------

// Returns true or false whether the selectedColor is the only selectable color
// in the neighbourhood. If so, the blend copies it to the output layer pixel
// directly.
inline bool isFlatNeighbourhood(int selectedColor, const TRasterCM32P &cmIn,
                                const TPoint &pos,
                                const SelectionRaster &selRas,
                                const BlurPattern &blurPattern) {
  TPixelCM32 &pix = cmIn->pixels(pos.y)[pos.x];
  int lx = cmIn->getLx(), ly = cmIn->getLy();
  unsigned int xy;

  TPoint samplePix;

  const TPoint *samplePoint =
      blurPattern.m_samples.empty() ? 0 : &blurPattern.m_samples[0];

  // Read the samples to determine if they only have posSelectedColor
  unsigned int i, samplesCount = blurPattern.m_samples.size();
  for (i = 0; i < samplesCount; ++i, ++samplePoint) {
    // Make sure the sample is inside the image
    samplePix.x = pos.x + samplePoint->x;
    samplePix.y = pos.y + samplePoint->y;

    xy = samplePix.x + lx * samplePix.y;

    if (samplePix.x < 0 || samplePix.y < 0 || samplePix.x >= lx ||
        samplePix.y >= ly)
      continue;

    if (!selRas.isPurePaint(xy) && selRas.isSelectedInk(xy))
      if (cmIn->pixels(samplePix.y)[samplePix.x].getInk() != selectedColor)
        return false;

    if (!selRas.isPureInk(xy) && selRas.isSelectedPaint(xy))
      if (cmIn->pixels(samplePix.y)[samplePix.x].getPaint() != selectedColor)
        return false;
  }

  return true;
}

//---------------------------------------------------------------------------------

// Calculates the estimate of blend selection in the neighbourhood specified by
// blurPattern.
inline void addSamples(const TRasterCM32P &cmIn, const TPoint &pos,
                       const TRaster32P &inkRas, const TRaster32P &paintRas,
                       const SelectionRaster &selRas,
                       const BlurPattern &blurPattern, DoubleRGBMPixel &pixSum,
                       double &factorsSum) {
  double inkFactor, paintFactor;
  unsigned int xy, j, l;
  int lx = cmIn->getLx(), ly = cmIn->getLy();
  TPixel32 *color;
  TPoint samplePos, pathPos;

  const TPoint *samplePoint =
      blurPattern.m_samples.empty() ? 0 : &blurPattern.m_samples[0];
  const TPoint *pathPoint;

  unsigned int i, blurSamplesCount = blurPattern.m_samples.size();
  for (i = 0; i < blurSamplesCount; ++i, ++samplePoint) {
    // Add each samples contribute to the sum
    samplePos.x = pos.x + samplePoint->x;
    samplePos.y = pos.y + samplePoint->y;
    if (samplePos.x < 0 || samplePos.y < 0 || samplePos.x >= lx ||
        samplePos.y >= ly)
      continue;

    // Ensure that each pixel on the sample's path (if any) is selected
    l         = blurPattern.m_samplePaths[i].size();
    pathPoint = blurPattern.m_samplePaths[i].empty()
                    ? 0
                    : &blurPattern.m_samplePaths[i][0];
    for (j = 0; j < l; ++j, ++pathPoint) {
      pathPos.x = pos.x + pathPoint->x;
      pathPos.y = pos.y + pathPoint->y;
      xy        = pathPos.x + lx * pathPos.y;

      if (!(selRas.isPurePaint(xy) || selRas.isSelectedInk(xy))) break;

      if (!(selRas.isPureInk(xy) || selRas.isSelectedPaint(xy))) break;
    }

    if (j < l) continue;

    xy = samplePos.x + lx * samplePos.y;

    if (selRas.isSelectedInk(xy) && !selRas.isPurePaint(xy)) {
      getFactors(cmIn->pixels(samplePos.y)[samplePos.x].getTone(), inkFactor,
                 paintFactor);

      color = &inkRas->pixels(samplePos.y)[samplePos.x];
      pixSum.r += inkFactor * color->r;
      pixSum.g += inkFactor * color->g;
      pixSum.b += inkFactor * color->b;
      pixSum.m += inkFactor * color->m;
      factorsSum += inkFactor;
    }

    if (selRas.isSelectedPaint(xy) && !selRas.isPureInk(xy)) {
      getFactors(cmIn->pixels(samplePos.y)[samplePos.x].getTone(), inkFactor,
                 paintFactor);

      color = &paintRas->pixels(samplePos.y)[samplePos.x];
      pixSum.r += paintFactor * color->r;
      pixSum.g += paintFactor * color->g;
      pixSum.b += paintFactor * color->b;
      pixSum.m += paintFactor * color->m;
      factorsSum += paintFactor;
    }
  }
}

//---------------------------------------------------------------------------------

typedef std::pair<TRaster32P, TRaster32P> RGBMRasterPair;

//---------------------------------------------------------------------------------

// Performs a single color blending. This function can be repeatedly invoked to
// perform multiple color blending.
inline void doBlend(const TRasterCM32P &cmIn, RGBMRasterPair &inkLayer,
                    RGBMRasterPair &paintLayer, const SelectionRaster &selRas,
                    const std::vector<BlurPattern> &blurPatterns) {
  // Declare some vars
  unsigned int blurPatternsCount = blurPatterns.size();
  int lx = cmIn->getLx(), ly = cmIn->getLy();
  double totalFactor;

  TPixelCM32 *cmPix, *cmBegin = (TPixelCM32 *)cmIn->getRawData();

  TPixel32 *inkIn    = (TPixel32 *)inkLayer.first->getRawData(),
           *inkOut   = (TPixel32 *)inkLayer.second->getRawData(),
           *paintIn  = (TPixel32 *)paintLayer.first->getRawData(),
           *paintOut = (TPixel32 *)paintLayer.second->getRawData();

  const BlurPattern *blurPattern, *blurPatternsBegin = &blurPatterns[0];
  bool builtSamples = false;

  DoubleRGBMPixel samplesSum;

  // For every cmIn pixel
  TPoint pos;
  SelectionData *selData = selRas.data();
  cmPix                  = cmBegin;
  for (pos.y = 0; pos.y < ly;
       ++pos.y, cmPix = cmBegin + pos.y * cmIn->getWrap())
    for (pos.x = 0; pos.x < lx; ++pos.x, ++inkIn, ++inkOut, ++paintIn,
        ++paintOut, ++selData, ++cmPix) {
      blurPattern = blurPatternsBegin + (rand() % blurPatternsCount);

      // Build the ink blend color
      if (!selData->m_purePaint && selData->m_selectedInk) {
        if (!builtSamples) {
          // Build samples contributes
          totalFactor  = 1.0;
          samplesSum.r = samplesSum.g = samplesSum.b = samplesSum.m = 0.0;

          if (!isFlatNeighbourhood(cmPix->getInk(), cmIn, pos, selRas,
                                   *blurPattern))
            addSamples(cmIn, pos, inkLayer.first, paintLayer.first, selRas,
                       *blurPattern, samplesSum, totalFactor);

          builtSamples = true;
        }

        // Output the blended pixel
        inkOut->r = (samplesSum.r + inkIn->r) / totalFactor;
        inkOut->g = (samplesSum.g + inkIn->g) / totalFactor;
        inkOut->b = (samplesSum.b + inkIn->b) / totalFactor;
        inkOut->m = (samplesSum.m + inkIn->m) / totalFactor;
      } else {
        // If the color is not blended, then just copy the old layer pixel
        *inkOut = *inkIn;
      }

      // Build the paint blend color
      if (!selData->m_pureInk && selData->m_selectedPaint) {
        if (!builtSamples) {
          // Build samples contributes
          totalFactor  = 1.0;
          samplesSum.r = samplesSum.g = samplesSum.b = samplesSum.m = 0.0;

          if (!isFlatNeighbourhood(cmPix->getPaint(), cmIn, pos, selRas,
                                   *blurPattern))
            addSamples(cmIn, pos, inkLayer.first, paintLayer.first, selRas,
                       *blurPattern, samplesSum, totalFactor);

          builtSamples = true;
        }

        // Output the blended pixel
        paintOut->r = (samplesSum.r + paintIn->r) / totalFactor;
        paintOut->g = (samplesSum.g + paintIn->g) / totalFactor;
        paintOut->b = (samplesSum.b + paintIn->b) / totalFactor;
        paintOut->m = (samplesSum.m + paintIn->m) / totalFactor;
      } else {
        // If the color is not blended, then just copy the old layer pixel
        *paintOut = *paintIn;
      }

      builtSamples = false;
    }
}

//---------------------------------------------------------------------------------

typedef std::vector<BlurPattern> BlurPatternContainer;

//---------------------------------------------------------------------------------

/*! This function performs a group of <a> spatial color blending <\a> operations
   on Toonz Images.
    The BlendParam structure stores the blend options recognized by this
   function; it includes
    a list of the palette indexes involved in the blend operation, plus:
    \li \b Intensity represents the \a radius of the blur operation between
   blend colors.
    \li \b Smoothness is the number of samples per pixel used to approximate the
   blur.
    <li> <b> Stop at Contour <\b> specifies if lines from pixels to neighbouring
   samples
         should not trespass color indexes not included in the blend operation
   <\li>
    The succession of input blend parameters are applied in the order.
*/

template <typename PIXEL>
void blend(TToonzImageP ti, TRasterPT<PIXEL> rasOut,
           const std::vector<BlendParam> &params) {
  assert(ti->getRaster()->getSize() == rasOut->getSize());

  // Extract the interesting raster. It should be the savebox of passed cmap,
  // plus - if
  // some param has the 0 index as blending color - the intensity of that blend
  // param.
  unsigned int i, j;
  TRect saveBox(ti->getSavebox());

  int enlargement = 0;
  for (i = 0; i < params.size(); ++i)
    for (j = 0; j < params[i].colorsIndexes.size(); ++j)
      if (params[i].colorsIndexes[j] == 0)
        enlargement = std::max(enlargement, tceil(params[i].intensity));
  saveBox           = saveBox.enlarge(enlargement);

  TRasterCM32P cmIn(ti->getRaster()->extract(saveBox));
  TRasterPT<PIXEL> rasOutExtract = rasOut->extract(saveBox);

  // Ensure that cmIn and rasOut have the same size
  unsigned int lx = cmIn->getLx(), ly = cmIn->getLy();

  // Build the pure colors infos
  SelectionRaster selectionRaster(cmIn);

  // Now, build a little group of BlurPatterns - and for each, one for passed
  // param.
  // A small number of patterns per param is needed to make the pattern look not
  // ever the same.
  const int blurPatternsPerParam = 10;
  std::vector<BlurPatternContainer> blurGroup(params.size());

  for (i = 0; i < params.size(); ++i) {
    BlurPatternContainer &blurContainer = blurGroup[i];
    blurContainer.reserve(blurPatternsPerParam);

    for (j = 0; j < blurPatternsPerParam; ++j)
      blurContainer.push_back(BlurPattern(
          params[i].intensity, params[i].smoothness, params[i].stopAtCountour));
  }

  // Build the palette
  TPalette *palette = ti->getPalette();
  std::vector<TPixel32> paletteColors;
  paletteColors.resize(palette->getStyleCount());
  for (i             = 0; i < paletteColors.size(); ++i)
    paletteColors[i] = premultiply(palette->getStyle(i)->getAverageColor());

  // Build the 4 auxiliary rasters for the blending procedure: they are ink /
  // paint versus input / output in the blend.
  // The output raster is reused to spare some memory - it should be, say, the
  // inkLayer's second at the end of the overall
  // blending procedure. It could be the first, without the necessity of
  // clearing it before blending the layers, but things
  // get more complicated when PIXEL is TPixel64...
  RGBMRasterPair inkLayer, paintLayer;

  TRaster32P rasOut32P_1(lx, ly, lx, (TPixel32 *)rasOut->getRawData(), false);
  inkLayer.first  = (params.size() % 2) ? rasOut32P_1 : TRaster32P(lx, ly);
  inkLayer.second = (params.size() % 2) ? TRaster32P(lx, ly) : rasOut32P_1;

  if (PIXEL::maxChannelValue >= TPixel64::maxChannelValue) {
    TRaster32P rasOut32P_2(lx, ly, lx,
                           ((TPixel32 *)rasOut->getRawData()) + lx * ly, false);
    paintLayer.first  = (params.size() % 2) ? rasOut32P_2 : TRaster32P(lx, ly);
    paintLayer.second = (params.size() % 2) ? TRaster32P(lx, ly) : rasOut32P_2;
  } else {
    paintLayer.first  = TRaster32P(lx, ly);
    paintLayer.second = TRaster32P(lx, ly);
  }

  inkLayer.first->clear();
  inkLayer.second->clear();
  paintLayer.first->clear();
  paintLayer.second->clear();

  // Now, we have to perform the blur of each of the cm's pixels.
  cmIn->lock();
  rasOut->lock();

  inkLayer.first->lock();
  inkLayer.second->lock();
  paintLayer.first->lock();
  paintLayer.second->lock();

  // Convert the initial cmIn to fullcolor ink - paint layers
  buildLayers(cmIn, paletteColors, inkLayer.first, paintLayer.first);

  // Perform the blend on separated ink - paint layers
  for (i = 0; i < params.size(); ++i) {
    if (params[i].colorsIndexes.size() == 0) continue;

    selectionRaster.updateSelection(cmIn, params[i]);
    doBlend(cmIn, inkLayer, paintLayer, selectionRaster, blurGroup[i]);

    std::swap(inkLayer.first, inkLayer.second);
    std::swap(paintLayer.first, paintLayer.second);
  }

  // Release the unnecessary rasters
  inkLayer.second->unlock();
  paintLayer.second->unlock();
  inkLayer.second   = TRaster32P();
  paintLayer.second = TRaster32P();

  // Clear rasOut - since it was reused to spare space...
  rasOut->clear();

  // Add the ink & paint layers on the output raster
  double PIXELmaxChannelValue = PIXEL::maxChannelValue;
  double toPIXELFactor =
      PIXELmaxChannelValue / (double)TPixel32::maxChannelValue;
  double inkFactor, paintFactor;
  TPoint pos;

  PIXEL *outPix, *outBegin    = (PIXEL *)rasOutExtract->getRawData();
  TPixelCM32 *cmPix, *cmBegin = (TPixelCM32 *)cmIn->getRawData();
  int wrap = rasOutExtract->getWrap();

  TPixel32 *inkPix   = (TPixel32 *)inkLayer.first->getRawData();
  TPixel32 *paintPix = (TPixel32 *)paintLayer.first->getRawData();

  for (i = 0; i < ly; ++i) {
    outPix = outBegin + wrap * i;
    cmPix  = cmBegin + wrap * i;
    for (j = 0; j < lx; ++j, ++outPix, ++cmPix, ++inkPix, ++paintPix) {
      getFactors(cmPix->getTone(), inkFactor, paintFactor);

      outPix->r = tcrop(
          toPIXELFactor * (inkFactor * inkPix->r + paintFactor * paintPix->r),
          0.0, PIXELmaxChannelValue);
      outPix->g = tcrop(
          toPIXELFactor * (inkFactor * inkPix->g + paintFactor * paintPix->g),
          0.0, PIXELmaxChannelValue);
      outPix->b = tcrop(
          toPIXELFactor * (inkFactor * inkPix->b + paintFactor * paintPix->b),
          0.0, PIXELmaxChannelValue);
      outPix->m = tcrop(
          toPIXELFactor * (inkFactor * inkPix->m + paintFactor * paintPix->m),
          0.0, PIXELmaxChannelValue);
    }
  }

  inkLayer.first->unlock();
  paintLayer.first->unlock();

  cmIn->unlock();
  rasOut->unlock();

  // Destroy the auxiliary bitmaps
  selectionRaster.destroy();
}

template void blend<TPixel32>(TToonzImageP cmIn, TRasterPT<TPixel32> rasOut,
                              const std::vector<BlendParam> &params);
template void blend<TPixel64>(TToonzImageP cmIn, TRasterPT<TPixel64> rasOut,
                              const std::vector<BlendParam> &params);
