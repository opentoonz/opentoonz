
#ifdef _MSC_VER
#pragma warning(disable : 4996)
#endif

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
#include "YOMBParam.h"
#include "YOMBInputParam.h"
#include "InputParam.h"
#include "STColSelPic.h"
#include "BlurMatrix.h"
#include "SDirection.h"
#include "SDef.h"
#include "CallCircle.h"
#include "CallParam.h"
#include "calligraph.h"
//#include "tmsg.h"

using namespace std;

#ifdef __cplusplus

extern "C" {
#endif

#define P(d) tmsg_info(" - %d -\n", d)
#define COPY_RASTER(inr, outr, border)                                         \
  tP.copy_raster(inr, outr, border, border, inr->lx - border - 1,              \
                 inr->ly - border - 1, 0, 0)

// ----- CALLIGRAPH for UCHAR pixels (range 0-255) --------------------------
static void calligraphUC(
    const RASTER *inr, RASTER *outr, CCallParam &par, const int border,
    bool isOutBorder)  // throw(SMemAllocError,SWriteRasterError)
{
  try {
    SRECT rect = {border, border, inr->lx - border - 1, inr->ly - border - 1};
    SPOINT p   = {0, 0};

    CSTColSelPic<UC_PIXEL> ipUC;
    ipUC.read(inr);
    ipUC.initSel();
    // Selection of pixels using CM
    int nbSel = ipUC.makeSelectionCMAP(par.m_ink, par.m_paint);
    if (nbSel > 0) {
      // Calculation of 'DIRECTION MAP'
      double fSize = par.m_accuracy / 5.0;
      fSize        = D_CUT(fSize, 1.0, 20.0);
      //			CSDirection
      // dir(ipUC.m_lX,ipUC.m_lY,ipUC.m_sel,I_ROUND(fSize));
      // Only on border (thickness of border is 3 )
      CSDirection dir(ipUC.m_lX, ipUC.m_lY, ipUC.m_sel.get(), I_ROUND(fSize),
                      3);
      dir.doDir();

      // Calculation of 'RADIUS MAP' based on the 'DIRECTION MAP'
      // doRadius(rH,rLR,rV,rRL,radiusBlur)
      fSize = par.m_accuracy / 10.0;
      fSize = D_CUT(fSize, 1.0, 10.0);
      ;
      dir.doRadius(par.m_rH, par.m_rLR, par.m_rV, par.m_rRL, I_ROUND(fSize));
      dir.getResult(ipUC.m_sel.get());

      // Draws the circles
      CCallCircle cc(par.m_thickness);
      cc.draw(ipUC, (par.m_ink.m_nb == 1), par.m_randomness);

      //			ipUC.showSelection();
    }
    if (isOutBorder)
      ipUC.writeOutBorder(inr, border, outr, rect, p);
    else
      ipUC.write(outr, rect, p);

  } catch (SMemAllocError) {
    //	SMemAllocError();
  } catch (SWriteRasterError) {
    // SWriteRasterError();
  }
}

// ----- CALLIGRAPH for USHORT pixels (range 0-65535) ------------------------
static void calligraphUS(
    const RASTER *inr, RASTER *outr, CCallParam &par, const int border,
    bool isOutBorder)  // throw(SMemAllocError,SWriteRasterError)
{
  try {
    SRECT rect = {border, border, inr->lx - border - 1, inr->ly - border - 1};
    SPOINT p   = {0, 0};

    CSTColSelPic<US_PIXEL> ipUS;
    ipUS.read(inr);
    ipUS.initSel();
    // Selection of pixels using CM
    int nbSel = ipUS.makeSelectionCMAP(par.m_ink, par.m_paint);
    if (nbSel > 0) {
      // Calculation of 'DIRECTION MAP'
      double fSize = par.m_accuracy / 5.0;
      fSize        = D_CUT(fSize, 1.0, 20.0);
      //			CSDirection
      // dir(ipUS.m_lX,ipUS.m_lY,ipUS.m_sel,I_ROUND(fSize));
      // Only on border (thickness of border is 3 )
      CSDirection dir(ipUS.m_lX, ipUS.m_lY, ipUS.m_sel.get(), I_ROUND(fSize),
                      3);
      dir.doDir();

      // Calculation of 'RADIUS MAP' based on the 'DIRECTION MAP'
      // doRadius(rH,rLR,rV,rRL,radiusBlur)
      fSize = par.m_accuracy / 10.0;
      fSize = D_CUT(fSize, 1.0, 10.0);
      ;
      dir.doRadius(par.m_rH, par.m_rLR, par.m_rV, par.m_rRL, I_ROUND(fSize));
      dir.getResult(ipUS.m_sel.get());

      // Draws the circles
      CCallCircle cc(par.m_thickness);
      cc.draw(ipUS, (par.m_ink.m_nb == 1), par.m_randomness);

      //			ipUC.showSelection();
    }
    if (isOutBorder)
      ipUS.writeOutBorder(inr, border, outr, rect, p);
    else
      ipUS.write(outr, rect, p);

  } catch (SMemAllocError) {
    // SMemAllocError();
  } catch (SWriteRasterError) {
    // SWriteRasterError();
  }
}

int calligraph(const RASTER *inr, RASTER *outr, const int border, int argc,
               const char *argv[], const int shrink, bool isOutBorder) {
  // The input raster must be RAS_CM16 or RAS_CM32!!!!
  CSTPic<UC_PIXEL> tP;
  if (inr->type != RAS_CM32) {
    COPY_RASTER(inr, outr, border);
    return 0;
  }
  try {
    CCallParam callP(argc, argv, shrink);
    if (!callP.isOK()) {
      COPY_RASTER(inr, outr, border);
      return 0;
    }
    srand(10);
    if (outr->type == RAS_RGBM) {
      calligraphUC(inr, outr, callP, border, isOutBorder);
    } else if (outr->type == RAS_RGBM64) {
      calligraphUS(inr, outr, callP, border, isOutBorder);
    } else {
      COPY_RASTER(inr, outr, border);
      return 0;
    }
  } catch (SMemAllocError e) {
    e.debug_print();
    COPY_RASTER(inr, outr, border);
    return 0;
  } catch (SWriteRasterError e) {
    e.debug_print();
    COPY_RASTER(inr, outr, border);
    return 0;
  }
  return 1;
}

#ifdef __cplusplus
}
#endif
