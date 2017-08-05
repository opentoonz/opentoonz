#pragma once

#ifndef MYPAINTHELPERS_H
#define MYPAINTHELPERS_H

#include <cmath>
#include <cassert>

#include "mypaint.h"

namespace mypaint {
  namespace helpers {
    const float precision = 1e-4f;

    typedef void ReadPixelFunc(
        const void *pixel,
        float &colorR,
        float &colorG,
        float &colorB,
        float &colorA );
    typedef void WritePixelFunc(
        void *pixel,
        float colorR,
        float colorG,
        float colorB,
        float colorA );
    typedef bool AskAccessFunc(
        void *surfaceController,
        const void *surfacePointer,
        int x0,
        int y0,
        int x1,
        int y1 );

    inline void dummyReadPixel(const void*, float&, float&, float&, float&) { }
    inline void dummyWritePixel(void*, float, float, float, float) { }
    inline bool dummyAskAccess(void*, const void*, int, int, int, int) { return true; }

    template< ReadPixelFunc  read     = dummyReadPixel,
              WritePixelFunc write    = dummyWritePixel,
              AskAccessFunc  askRead  = dummyAskAccess,
              AskAccessFunc  askWrite = dummyAskAccess >
    class SurfaceCustom: public Surface {
    public:
      void *pointer;
      int width;
      int height;
      int pixelSize;
      int rowSize;
      void *controller;
      bool antialiasing;

      SurfaceCustom():
        pointer(), width(), height(), pixelSize(), rowSize(), controller(), antialiasing(true)
        { }

      SurfaceCustom(void *pointer, int width, int height, int pixelSize, int rowSize = 0, void *controller = 0, bool antialiasing = true):
        pointer(pointer),
        width(width),
        height(height),
        pixelSize(pixelSize),
        rowSize(rowSize ? rowSize : width*pixelSize),
        controller(controller),
        antialiasing(antialiasing)
        { }

    private:
      template< bool enableAspect,         // 2 variants
                bool enableAntialiasing,   // 1 variants (true)
                bool enableHardnessOne,    // 3 variants
                bool enableHardnessHalf,   //   --
                bool enablePremult,        // 1 variant  (true)
                bool enableBlendNormal,    // 2 variants
                bool enableBlendLockAlpha, // 2 variants
                bool enableBlendColorize,  // 2 variants
                bool enableSummary >       // 1 variants (false)  Total: 48 copies of function
      bool drawDabCustom(const Dab &dab, float *colorSummary) {
        const float antialiasing = 0.66f; // equals to drawDab::minRadiusX
        const float lr = 0.30f;
        const float lg = 0.59f;
        const float lb = 0.11f;

        if (!enableBlendNormal && !enableBlendLockAlpha && !enableBlendColorize && !enableSummary)
          return false;

        // prepare summary
        double colorSumR, colorSumG, colorSumB, colorSumA, colorSumW;
        if (enableSummary) {
          colorSummary[0] = 0.f;
          colorSummary[1] = 0.f;
          colorSummary[2] = 0.f;
          colorSummary[3] = 0.f;
          colorSumR = 0.0;
          colorSumG = 0.0;
          colorSumB = 0.0;
          colorSumA = 0.0;
          colorSumW = 0.0;
        }

        // bounding rect
        int x0 = std::max(0, (int)floor(dab.x - dab.radius - 1.f + precision));
        int x1 = std::min(width-1, (int)ceil(dab.x + dab.radius + 1.f - precision));
        int y0 = std::max(0, (int)floor(dab.y - dab.radius - 1.f + precision));
        int y1 = std::min(height-1, (int)ceil(dab.y + dab.radius + 1.f - precision));
        if (x0 > x1 || y0 > y1)
          return false;

        if (controller && !askRead(controller, pointer, x0, y0, x1, y1))
          return false;
        if (enableBlendNormal || enableBlendLockAlpha || enableBlendColorize)
          if (controller && !askWrite(controller, pointer, x0, y0, x1, y1))
            return false;

        assert(pointer);

        // prepare pixel iterator
        int w = x1 - x0 + 1;
        int h = y1 - y0 + 1;
        char *pixel = (char*)pointer + rowSize*y0 + pixelSize*x0;
        int pixelNextCol = pixelSize;
        int pixelNextRow = rowSize - w*pixelSize;

        // prepare geometry iterators
        float radiusInv = 1.f/dab.radius;
        float dx = (float)x0 - dab.x + 0.5f;
        float dy = (float)y0 - dab.y + 0.5f;
        float ddx, ddxNextCol, ddxNextRow;
        float ddy, ddyNextCol, ddyNextRow;
        if (enableAspect) {
          float angle = dab.angle*((float)M_PI/180.f);
          float s = sinf(angle);
          float c = cosf(angle);

          float radiusYInv = radiusInv*dab.aspectRatio;

          ddx        = (dx*c + dy*s)*radiusInv;
          ddxNextCol = c*radiusInv;
          ddxNextRow = (s - c*(float)w)*radiusInv;

          ddy        = (dy*c - dx*s)*radiusYInv;
          ddyNextCol = -s*radiusYInv;
          ddyNextRow = (c + s*(float)w)*radiusYInv;
        } else {
          ddx        = dx*radiusInv;
          ddxNextCol = radiusInv;
          ddxNextRow = -radiusInv*(float)w;

          ddy        = dy*radiusInv;
          ddyNextCol = 0.f;
          ddyNextRow = radiusInv;
        }

        // prepare antialiasing
        float hardness, ka0, ka1, kb1, kc1, kc2;
        float aa, aa2, aaSqr, ddySqrMin, aspectRatioSqr;
        if (enableAntialiasing) {
          if (enableHardnessOne) {
          } else
          if (enableHardnessHalf) {
            ka0 = 0.25f;
            kc2 = 0.75f;
          } else {
            hardness = std::min(dab.hardness, 1.f - precision);
            float hk = hardness/(hardness - 1.f);
            ka0 = 0.25f/hk;
            ka1 = 0.25f*hk;
            kb1 = -0.5f*hk;
            kc1 = ((ka0 - ka1)*hardness + 0.5f - kb1)*hardness;
            kc2 = ka1 + kb1 + kc1;
          }

          aa = antialiasing*radiusInv;
          if (enableAspect) {
            ddySqrMin = 0.5f*aa*dab.aspectRatio;
            ddySqrMin *= ddySqrMin;
            aspectRatioSqr = dab.aspectRatio*dab.aspectRatio;
          } else {
            aa2 = aa + aa;
            aaSqr = aa*aa;
          }
        } else {
          if (enableHardnessOne) {
          } else
          if (enableHardnessHalf) {
          } else {
            hardness = std::min(dab.hardness, 1.f - precision);
            float hk = hardness/(hardness - 1.f);
            ka0 = 1.f/hk;
            ka1 = hk;
            kb1 = -hk;
          }
        }

        // prepare blend
        float opaque = dab.opaque;
        float colorR, colorG, colorB;
        if (enableBlendNormal || enableBlendLockAlpha || enableBlendColorize) {
          colorR = dab.colorR;
          colorG = dab.colorG;
          colorB = dab.colorB;
        }
        float blendNormal, blendAlphaEraser;
        if (enableBlendNormal) {
          blendNormal = (1.f - dab.lockAlpha)*(1.f - dab.colorize);
          blendAlphaEraser = dab.alphaEraser;
        }
        float blendLockAlpha;
        if (enableBlendLockAlpha) {
          blendLockAlpha = dab.lockAlpha;
        }
        float blendColorize, blendColorizeSrcLum;
        if (enableBlendColorize) {
          blendColorize = dab.colorize;
          blendColorizeSrcLum = dab.colorR*lr + dab.colorG*lg + dab.colorB*lb;
        }

        // process
        for(int iy = h; iy; --iy, ddx += ddxNextRow, ddy += ddyNextRow, pixel += pixelNextRow)
        for(int ix = w; ix; --ix, ddx += ddxNextCol, ddy += ddyNextCol, pixel += pixelNextCol) {
          float o;
          if (enableAntialiasing) {
            float dd, dr;
            if (enableAspect) {
              float ddxSqr = ddx*ddx;
              float ddySqr = std::max(ddySqrMin, ddy*ddy);
              dd = ddxSqr + ddySqr;
              float k = aa*sqrtf(ddxSqr + ddySqr*aspectRatioSqr);
              dr = k*(2.f + k/dd);
            } else {
              dd = ddx*ddx + ddy*ddy;
              dr = aa2*sqrtf(dd) + aaSqr;
            }

            float dd0 = dd - dr;
            if (dd0 > 1.f)
              continue;
            float dd1 = dd + dr;

            float o0, o1;
            if (enableHardnessOne) {
              o0 = dd0 < -1.f      ?  -0.5f
                 :                     0.5f*dd0;
              o1 = dd1 <  1.f      ?   0.5f*dd1
                 :                     0.5f;
            } else
            if (enableHardnessHalf) {
              o0 = dd0 < -1.f      ?  -0.25f
                 : dd0 <  0.f      ? ( 0.25f*dd0 + 0.5f )*dd0
                 :                   (-0.25f*dd0 + 0.5f )*dd0;
              o1 = dd1 <  1.f      ? (-0.25f*dd1 + 0.5f )*dd1
                 :                     0.25f;
            } else {
              o0 = dd0 < -1.f      ?  -kc2
                 : dd0 < -hardness ? (-ka1*dd0   + kb1  )*dd0 - kc1
                 : dd0 <  0.f      ? (-ka0*dd0   + 0.5f )*dd0
                 : dd0 <  hardness ? ( ka0*dd0   + 0.5f )*dd0
                 :                   ( ka1*dd0   + kb1  )*dd0 + kc1;
              o1 = dd1 <  hardness ? ( ka0*dd1   + 0.5f )*dd1
                 : dd1 <  1.f      ? ( ka1*dd1   + kb1  )*dd1 + kc1
                 :                     kc2;
            }
            o = opaque*(o1 - o0)/dr;
          } else {
            float dd = ddx*ddx + ddy*ddy;
            if (dd > 1.f)
              continue;
            if (enableHardnessOne) {
              o = opaque;
            } else
            if (enableHardnessHalf) {
              o = opaque*(1.f - dd);
            } else {
              o = opaque*(dd <  hardness ? ka0*dd + 1.f : ka1*dd + kb1);
            }
          }

          if (o <= precision)
            continue;

          // read pixel
          float destR, destG, destB, destA;
          read(pixel, destR, destG, destB, destA);

          if (enablePremult) {
            destR *= destA;
            destG *= destA;
            destB *= destA;
          }

          if (enableSummary) {
            colorSumR += (double)(o*destR);
            colorSumG += (double)(o*destG);
            colorSumB += (double)(o*destB);
            colorSumA += (double)(o*destA);
            colorSumW += (double)o;
          }

          if (!enableBlendNormal && !enableBlendLockAlpha && !enableBlendColorize)
            continue;

          if (enableBlendNormal) {
            float oa = blendNormal*o;
            float ob = 1.f - oa;
            oa *= blendAlphaEraser;
            destR = oa*dab.colorR + ob*destR;
            destG = oa*dab.colorG + ob*destG;
            destB = oa*dab.colorB + ob*destB;
            destA = oa + ob*destA;
          }

          if (enableBlendLockAlpha) {
            float oa = blendLockAlpha*o;
            float ob = 1.f - oa;
            oa *= destA;
            destR = oa*colorR + ob*destR;
            destG = oa*colorG + ob*destG;
            destB = oa*colorB + ob*destB;
          }

          if (enableBlendColorize) {
            float dLum = destR*lr + destG*lg + destB*lb - blendColorizeSrcLum;
            float r = colorR + dLum;
            float g = colorG + dLum;
            float b = colorB + dLum;

            float lum = r*lr + g*lg + b*lb;
            float cmin = std::min(std::min(r, g), b);
            float cmax = std::max(std::max(r, g), b);
            if (cmin < 0.f) {
              float k = lum/(lum - cmin);
              r = lum + k*(r - lum);
              g = lum + k*(g - lum);
              b = lum + k*(b - lum);
            }
            if (cmax > 1.f) {
              float k = (1.f - lum)/(cmax - lum);
              r = lum + k*(r - lum);
              g = lum + k*(g - lum);
              b = lum + k*(b - lum);
            }

            float oa = blendColorize*o;
            float ob = 1.f - oa;
            destR = oa*r + ob*destR;
            destG = oa*g + ob*destG;
            destB = oa*b + ob*destB;
          }

          if (enablePremult) {
            if (destA > precision) {
              float oneDivA = 1.f/destA;
              destR *= oneDivA;
              destG *= oneDivA;
              destB *= oneDivA;
            }
          }

          // clamp
          destR = std::min(std::max(destR, 0.f), 1.f);
          destG = std::min(std::max(destG, 0.f), 1.f);
          destB = std::min(std::max(destB, 0.f), 1.f);
          destA = std::min(std::max(destA, 0.f), 1.f);

          write(pixel, destR, destG, destB, destA);
        }

        if (enableSummary) {
          double k = colorSumA > precision ? 1.0/colorSumA : 0.0;
          colorSummary[0] = (float)(k*colorSumR);
          colorSummary[1] = (float)(k*colorSumG);
          colorSummary[2] = (float)(k*colorSumB);
          colorSummary[3] = (float)(colorSumW > precision ? colorSumA/colorSumW : 0.0);
        }

        return true;
      }

      template< bool enableAspect,
                bool enableAntialiasing,
                bool enableHardnessOne,
                bool enableHardnessHalf,
                bool enableBlendNormal,
                bool enableBlendLockAlpha >
      inline bool drawDabCheckBlendColorize(const Dab &dab) {
        if (dab.colorize > precision) {
          return drawDabCustom<
              enableAspect,
              enableAntialiasing,
              enableHardnessOne,
              enableHardnessHalf,
              false, // enablePremult
              enableBlendNormal,
              enableBlendLockAlpha,
              true,  // enableBlendColorize,
              false  // enableSummary
              >(dab, 0);
        } else {
          return drawDabCustom<
              enableAspect,
              enableAntialiasing,
              enableHardnessOne,
              enableHardnessHalf,
              false, // enablePremult
              enableBlendNormal,
              enableBlendLockAlpha,
              false, // enableBlendColorize,
              false  // enableSummary
              >(dab, 0);
        }
      }

      template< bool enableAspect,
                bool enableAntialiasing,
                bool enableHardnessOne,
                bool enableHardnessHalf,
                bool enableBlendNormal >
      inline bool drawDabCheckBlendLockAlpha(const Dab &dab) {
        if (dab.lockAlpha > precision) {
            return drawDabCheckBlendColorize<
                enableAspect,
                enableAntialiasing,
                enableHardnessOne,
                enableHardnessHalf,
                enableBlendNormal,
                true   // enableBlendLockAlpha
                >(dab);
        } else {
          return drawDabCheckBlendColorize<
              enableAspect,
              enableAntialiasing,
              enableHardnessOne,
              enableHardnessHalf,
              enableBlendNormal,
              false  // enableBlendLockAlpha
              >(dab);
        }
      }

      template< bool enableAspect,
                bool enableAntialiasing,
                bool enableHardnessOne,
                bool enableHardnessHalf >
      inline bool drawDabCheckBlendNormal(const Dab &dab) {
        if ((1.f - dab.lockAlpha)*(1.f - dab.colorize) > precision) {
            return drawDabCheckBlendLockAlpha<
                enableAspect,
                enableAntialiasing,
                enableHardnessOne,
                enableHardnessHalf,
                true   // enableBlendNormal
                >(dab);
        } else {
          return drawDabCheckBlendLockAlpha<
              enableAspect,
              enableAntialiasing,
              enableHardnessOne,
              enableHardnessHalf,
              false  // enableBlendNormal
              >(dab);
        }
      }

      template< bool enableAspect,
                bool enableAntialiasing >
      inline bool drawDabCheckHardness(const Dab &dab) {
        if (dab.hardness >= 1.f - precision) {
          return drawDabCheckBlendNormal<
              enableAspect,
              enableAntialiasing,
              true,  // enableHardnessOne
              false  // enableHardnessHalf
              >(dab);
        } else
        if (fabsf(dab.hardness - 0.5f) <= precision) {
          return drawDabCheckBlendNormal<
              enableAspect,
              enableAntialiasing,
              false, // enableHardnessOne
              true   // enableHardnessHalf
              >(dab);
        } else {
          return drawDabCheckBlendNormal<
              enableAspect,
              enableAntialiasing,
              false, // enableHardnessOne
              false  // enableHardnessHalf
              >(dab);
        }
      }

      template< bool enableAspect >
      inline bool drawDabCheckAntialiasing(const Dab &dab, bool antialiasing) {
        if (antialiasing) {
          return drawDabCheckHardness<
              enableAspect,
              true   // enableAntialiasing
              >(dab);
        } else {
          return drawDabCheckHardness<
              enableAspect,
              false  // enableAntialiasing
              >(dab);
        }
      }

      inline bool drawDabCheckAspect(const Dab &dab, bool antialiasing) {
        if (dab.aspectRatio > 1.f + precision) {
          return drawDabCheckAntialiasing<
              true   // enableAspect
              >(dab, antialiasing);
        } else {
          return drawDabCheckAntialiasing<
              false  // enableAspect
              >(dab, antialiasing);
        }
      }

    public:
      bool getColor(float x, float y, float radius,
          float &colorR, float &colorG, float &colorB, float &colorA)
      {
        float color[4];
        bool done = drawDabCustom<
            false, // enableAspect
            false, // enableAntialiasing
            false, // enableHardnessOne
            true,  // enableHardnessHalf
            false, // enablePremult
            false, // enableBlendNormal
            false, // enableBlendLockAlpha
            false, // enableBlendColorize
            true   // enableSummary
            >(Dab(x, y, radius), color);
        colorR = color[0];
        colorG = color[1];
        colorB = color[2];
        colorA = color[3];
        return done;
      }

      bool drawDab(const Dab &dab) {
        const float minRadiusX = 0.66f; // equals to drawDabCustom::antialiasing
        const float minRadiusY = 3.f*minRadiusX;
        const float maxAspect = 10.f;
        const float minOpaque = 1.f/256.f;

        // check limits
        Dab d = dab.getClamped();
        if (d.radius <= precision)
          return true;
        if (d.hardness <= precision)
          return true;

        // fix aspect
        if (d.aspectRatio > maxAspect) {
          d.opaque *= maxAspect/d.aspectRatio;
          d.aspectRatio = maxAspect;
        }

        // fix radius
        float hardnessSize = 1.f;
        if (d.radius < minRadiusX) {
          d.opaque *= d.radius/minRadiusX;
          d.radius = minRadiusX;
        }
        if (d.hardness < 0.5f) {
          hardnessSize = sqrtf(d.hardness/(1.f - d.hardness));
          float radiusH = d.radius*hardnessSize;
          if (radiusH < minRadiusX) {
            d.opaque *= radiusH/minRadiusX;
            hardnessSize = minRadiusX/d.radius;
            float hardnessSizeSqr = hardnessSize*hardnessSize;
            d.hardness = hardnessSizeSqr/(1.f + hardnessSizeSqr);
            radiusH = minRadiusX;
          }
          if (d.hardness*d.opaque < minOpaque) {
            d.radius = radiusH;
            d.hardness = 0.5f;
            hardnessSize = 1.f;
          }
        }

        float radiusYh = d.radius*hardnessSize/d.aspectRatio;
        float actualMinRadiusY = std::min(d.radius, minRadiusY);
        if (radiusYh < actualMinRadiusY) {
          float k = radiusYh/actualMinRadiusY;
          d.opaque *= k;
          d.aspectRatio *= k;
        }

        // check opaque
        if (d.opaque < minOpaque)
          return false;

        return drawDabCheckAspect(d, antialiasing);
      }
    }; // SurfaceCustom
  } // helpers
} // mypaint

#endif  // MYPAINTHELPERS_H
