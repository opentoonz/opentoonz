#pragma once

#ifndef RUNSMAP_H
#define RUNSMAP_H

#include "traster.h"

//*********************************************************************************************************
//    Run Maps
//*********************************************************************************************************

/*!
  The RunsMapP is an auxiliary raster greymap type used to store run-length
  informations
  about an image.

  Not every image pixel has valid informations. Only pixels corresponding to
  run headers do, and those include the position of the next run header.
  This means that users must always iterate from the beginning of a line to get
  valid
  data.

  The following coding is adopted to extract the run length from the run
  headers:

  \li We'll use the last 2 bits only in the headers. With these, we can cover
  directly
      those runs up to 4 pixels length.
  \li When the length >=4, we require that one byte is taken in the run to store
  the length
      up to 256 pixels.
  \li When the length >= 256, we take 4 additional bytes to store the length
  (which this time
      could go up to billions).

  Observe that the runsmap supports a symmetrical representation, useful
  to traverse runs both forward and backwards. This means that 2 headers are
  provided for each run, at the opposite ends (unless the run is 1 pixel-width).
*/
class RunsMap final : public TRasterT<TPixelGR8> {
public:
  RunsMap(int lx, int ly) : TRasterT<TPixelGR8>(lx, ly) { clear(); }

  const UCHAR &runHeader(int x, int y) const { return pixels(y)[x].value; }
  UCHAR &runHeader(int x, int y) { return pixels(y)[x].value; }

  TUINT32 runLength(const TPixelGR8 *run, bool reversed = false) const;
  TUINT32 runLength(int x, int y, bool reversed = false) const {
    return runLength(pixels(y) + x, reversed);
  }

public:
  void setRunLength(TPixelGR8 *run, TUINT32 length);
  void setRunLength(int x, int y, TUINT32 length) {
    setRunLength(pixels(y) + x, length);
  }
};

//---------------------------------------------------------------------------------------------

#ifdef _WIN32
template class DV_EXPORT_API TSmartPointerT<RunsMap>;
#endif

class RunsMapP final : public TSmartPointerT<RunsMap> {
public:
  RunsMapP() {}
  RunsMapP(int lx, int ly) : TSmartPointerT<RunsMap>(new RunsMap(lx, ly)) {}
  RunsMapP(const TDimension &d)
      : TSmartPointerT<RunsMap>(new RunsMap(d.lx, d.ly)) {}
};

//---------------------------------------------------------------------------------------------

template <typename Pixel, typename PixelSelector>
void buildRunsMap(RunsMapP &runsMap, const TRasterPT<Pixel> &ras,
                  const PixelSelector &selector) {
  // Traverse the raster, extracting run lengths
  int y, ly = ras->getLy();
  for (y = 0; y < ly; ++y) {
    Pixel *lineStart = (Pixel *)ras->pixels(y),
          *lineEnd   = lineStart + ras->getLx();

    Pixel *pix, *runStart;
    typename PixelSelector::value_type colorIndex;
    for (pix = runStart = lineStart, colorIndex = selector.value(*pix);
         pix < lineEnd; ++pix)
      if (selector.value(*pix) != colorIndex) {
        runsMap->setRunLength(runStart - lineStart, y, pix - runStart);
        runStart   = pix;
        colorIndex = selector.value(*pix);
      }
    runsMap->setRunLength(runStart - lineStart, y, pix - runStart);
  }
}

#endif  // RUNSMAP_H
