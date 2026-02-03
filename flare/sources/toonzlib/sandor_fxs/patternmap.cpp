

#ifdef _WIN32
#include "windows.h"
#endif
#include <vector>

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>
#include <string.h>
#include <math.h>

#include "Params.h"
#include "PatternMapParam.h"
#include "STColSelPic.h"
#include "BlurMatrix.h"
#include "SDirection.h"
#include "SDef.h"
#include "CallCircle.h"
#include "CallParam.h"
#include "EraseContour.h"
#include "PatternPosition.h"
#include "Pattern.h"
#include "SError.h"
//#include "tmsg.h"
#include "patternmap.h"

using namespace std;

#ifdef __cplusplus
extern "C" {
#endif

//#define P(d) tmsg_warning(" - %d -\n",d)
#define COPY_RASTER(iras, oras, border)                                        \
  tP.copy_raster(iras, oras, border, border, iras->lx - border - 1,            \
                 iras->ly - border - 1, 0, 0)

#define DIRACCURACY 12

// ----- PatternMapping for UCHAR pixels (range 0-255)
// --------------------------
static void patternmapUC(
    const RASTER *iras, RASTER *oras, CPatternMapParam &pmP, const int border,
    RASTER
        *imgContour)  // throw (SMemAllocError,SWriteRasterError,SFileReadError)
{
  try {
    SRECT rect = {border, border, iras->lx - border - 1, iras->ly - border - 1};
    SPOINT p   = {0, 0};

    CSTColSelPic<UC_PIXEL> ipUC, ipOri;
    ipUC.read(iras);
    ipUC.initSel();
    ipOri = ipUC;
    // Selection of pixels using CM
    CCIL emptyCIL;
    int nbContourPixel = 0;

    if (pmP.m_isKeepContour) {
      CEraseContour ec(ipUC);
      nbContourPixel = ec.makeSelection(pmP.m_ink);
      //			nbContourPixel=ipUC.makeSelectionCMAP(pmP.m_ink,emptyCIL);
    } else {
      CEraseContour ec(ipUC);
      nbContourPixel = ec.doIt(pmP.m_ink);
    }
    if (nbContourPixel > 0) {
      if (!pmP.m_isRandomDir) {
        CSDirection dir(ipUC.m_lX, ipUC.m_lY, ipUC.m_sel.get(), DIRACCURACY);
        dir.doDir();
        dir.getResult(ipUC.m_sel.get());
      }
      CPatternPosition pPos(ipUC, nbContourPixel, pmP.m_density, pmP.m_minDist,
                            pmP.m_maxDist);
      // Reads the pattern
      CPattern pat(imgContour);

      for (vector<SPOINT>::iterator pp = pPos.m_pos.begin();
           pp != pPos.m_pos.end(); pp++) {
        // Calculates the rotation angle
        double angle = 0.0;
        if (pmP.m_isRandomDir)
          angle = pmP.m_minDirAngle +
                  (double)(rand() % 1001) * 0.001 *
                      (pmP.m_maxDirAngle - pmP.m_minDirAngle);
        else {
          if (pp->x >= 0 && pp->y >= 0 && pp->x < ipUC.m_lX &&
              pp->y < ipUC.m_lY) {
            UCHAR *sel = ipUC.m_sel.get() + pp->y * ipUC.m_lX + pp->x;
            if (*sel > (UCHAR)0) angle = (double)(*sel) - 50.0;
          }
          angle += pmP.m_minDirAngle +
                   (double)(rand() % 1001) * 0.001 *
                       (pmP.m_maxDirAngle - pmP.m_minDirAngle);
        }

        // Calculates the scale
        double scale = 1.0;
        scale =
            pmP.m_minScale +
            (double)(rand() % 1001) * 0.001 * (pmP.m_maxScale - pmP.m_minScale);

        // Mapping of Pattern
        pat.mapIt(ipUC, ipOri, pp->x, pp->y, scale, angle, pmP.m_isUseInkColor,
                  pmP.m_isIncludeAlpha);
      }
    }

    //		ipUC.showSelection();

    ipUC.write(oras, rect, p);
  } catch (SMemAllocError) {
    // SMemAllocError();
  } catch (SWriteRasterError) {
    //	SWriteRasterError();
  } catch (SFileReadError) {
    //	SFileReadError();
  }
}

// ----- PatternMapping for USHORT pixels (range 0-255)
// --------------------------
static void patternmapUS(
    const RASTER *iras, RASTER *oras, CPatternMapParam &pmP, const int border,
    RASTER
        *imgContour)  // throw (SMemAllocError,SWriteRasterError,SFileReadError)
{
  try {
    SRECT rect = {border, border, iras->lx - border - 1, iras->ly - border - 1};
    SPOINT p   = {0, 0};

    CSTColSelPic<US_PIXEL> ipUS, ipOri;
    ipUS.read(iras);
    ipUS.initSel();
    ipOri = ipUS;
    // Selection of pixels using CM
    CCIL emptyCIL;
    int nbContourPixel = 0;

    if (pmP.m_isKeepContour) {
      CEraseContour ec(ipUS);
      nbContourPixel = ec.makeSelection(pmP.m_ink);
      // a 255-nek utananezni !!!
      //			nbContourPixel=ipUS.makeSelectionCMAP(pmP.m_ink,emptyCIL);
    } else {
      CEraseContour ec(ipUS);
      nbContourPixel = ec.doIt(pmP.m_ink);
    }
    if (nbContourPixel > 0) {
      if (!pmP.m_isRandomDir) {
        CSDirection dir(ipUS.m_lX, ipUS.m_lY, ipUS.m_sel.get(), DIRACCURACY);
        dir.doDir();
        dir.getResult(ipUS.m_sel.get());
      }
      CPatternPosition pPos(ipUS, nbContourPixel, pmP.m_density, pmP.m_minDist,
                            pmP.m_maxDist);
      // Reads the pattern
      CPattern pat(imgContour);

      for (vector<SPOINT>::iterator pp = pPos.m_pos.begin();
           pp != pPos.m_pos.end(); pp++) {
        // Calculates the rotation angle
        double angle = 0.0;
        if (pmP.m_isRandomDir)
          angle = pmP.m_minDirAngle +
                  (double)(rand() % 1001) * 0.001 *
                      (pmP.m_maxDirAngle - pmP.m_minDirAngle);
        else {
          if (pp->x >= 0 && pp->y >= 0 && pp->x < ipUS.m_lX &&
              pp->y < ipUS.m_lY) {
            UCHAR *sel = ipUS.m_sel.get() + pp->y * ipUS.m_lX + pp->x;
            if (*sel > (UCHAR)0) angle = (double)(*sel) - 50.0;
          }
          angle += pmP.m_minDirAngle +
                   (double)(rand() % 1001) * 0.001 *
                       (pmP.m_maxDirAngle - pmP.m_minDirAngle);
        }

        // Calculates the scale
        double scale = 1.0;
        scale =
            pmP.m_minScale +
            (double)(rand() % 1001) * 0.001 * (pmP.m_maxScale - pmP.m_minScale);

        // Mapping of Pattern
        pat.mapIt(ipUS, ipOri, pp->x, pp->y, scale, angle, pmP.m_isUseInkColor,
                  pmP.m_isIncludeAlpha);
      }
    }
    //		ipUC.showSelection();
    ipUS.write(oras, rect, p);
  } catch (SMemAllocError) {
    // SMemAllocError();
  } catch (SWriteRasterError) {
    //	SWriteRasterError();
  } catch (SFileReadError) {
    //	SFileReadError();
  }
}

int patternmap(const RASTER *iras, RASTER *oras, const int border, int argc,
               const char *argv[], const int shrink, RASTER *imgContour) {
  // The input raster must be RAS_CM16 or RAS_CM24!!!!
  CSTPic<UC_PIXEL> tP;
  if (iras->type != RAS_CM32) {
    COPY_RASTER(iras, oras, border);
    return 0;
  }
  try {
    CPatternMapParam pmP(argc, argv, shrink);
    if (!pmP.isOK()) {
      COPY_RASTER(iras, oras, border);
      return 0;
    }
    srand(10);
    if (oras->type == RAS_RGBM) {
      patternmapUC(iras, oras, pmP, border, imgContour);
    } else if (oras->type == RAS_RGBM64) {
      patternmapUS(iras, oras, pmP, border, imgContour);
    } else {
      COPY_RASTER(iras, oras, border);
      return 0;
    }
  } catch (SMemAllocError e) {
    e.debug_print();
    COPY_RASTER(iras, oras, border);
    return 0;
  } catch (SWriteRasterError e) {
    e.debug_print();
    COPY_RASTER(iras, oras, border);
    return 0;
  } catch (SFileReadError e) {
    e.debug_print();
    COPY_RASTER(iras, oras, border);
    return 0;
  }

  return 1;
}

#ifdef __cplusplus
}
#endif
