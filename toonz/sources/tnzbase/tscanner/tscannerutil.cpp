

#include "tscannerutil.h"

#include <cstring>

#define BUFBYTE(X, Y, BUF, BYTEWRAP, BITOFFS)                                  \
  (((UCHAR *)(BUF))[(((X) + (BITOFFS)) >> 3) + (Y) * (BYTEWRAP)])

#define GET_BIT(X, Y, BUF, BYTEWRAP, BITOFFS)                                  \
  ((BUFBYTE(X, Y, BUF, BYTEWRAP, BITOFFS) >> (7 - (((X) + (BITOFFS)) & 7))) &  \
   (UCHAR)1)

namespace TScannerUtil {

struct EP {
  unsigned char r, g, b;
};

//-----------------------------------------------------------------------------

void copyRGBBufferToTRaster32(unsigned char *rgbBuffer, int rgbLx, int rgbLy,
                              const TRaster32P &rout, bool internal) {
  if (internal) {
    TPixelRGBM32 *dst = rout->pixels();
    EP *src           = (EP *)(rgbBuffer + (rgbLx * rgbLy - 1) * 3);
    const int w       = rout->getWrap();

    for (int x = 0; x < rout->getLx(); ++x) {
      dst = rout->pixels() + x;
      for (int y = 0; y < rout->getLy(); ++y) {
        dst->r = src->r;
        dst->g = src->g;
        dst->b = src->b;
        dst->m = 0xff;
        dst += w;
        --src;
      }
    }
  } else {
    unsigned char *index = rgbBuffer;
    unsigned char *end   = index + (rgbLx * rgbLy * 3);
    TPixel32 *outPix     = rout->pixels();
    while (index < end) {
      outPix->r = (int)*index;
      index++;
      outPix->g = (int)*index;
      index++;
      outPix->b = (int)*index;
      index++;
      outPix->m = 255;
      outPix++;
    }
    rout->yMirror();
  }
}

//-----------------------------------------------------------------------------

void copyRGBBufferToTRasterGR8(unsigned char *rgbBuffer, int rgbLx, int rgbLy,
                               int rgbWrap, const TRasterGR8P &rout) {
  TPixelGR8 *dst = rout->pixels();
  EP *src        = (EP *)(rgbBuffer + (rgbLx * rgbLy - 1) * 3);
  const int w    = rout->getWrap();

  for (int x = 0; x < rout->getLx(); ++x) {
    dst = rout->pixels() + x;
    for (int y = 0; y < rout->getLy(); ++y) {
      *dst = TPixelGR8::from(TPixelRGBM32(src->r, src->g, src->b));
      dst += w;
      --src;
    }
  }
}

//-----------------------------------------------------------------------------

void copyGR8BufferToTRasterGR8(unsigned char *gr8Buffer, int rgbLx, int rgbLy,
                               const TRasterGR8P &rout, bool internal) {
  if (internal) {
    unsigned char *dst = rout->getRawData();
    unsigned char *src = (gr8Buffer + rgbLx * rgbLy - 1);
    const int w        = rout->getWrap();

    for (int x = 0; x < rout->getLx(); ++x) {
      dst = rout->getRawData() + x;
      for (int y = 0; y < rout->getLy(); ++y) {
        *dst = (*src);
        dst += w;
        --src;
      }
    }
  } else {
    memcpy(rout->getRawData(), gr8Buffer, rgbLx * rgbLy);
    rout->yMirror();
  }
}

//-----------------------------------------------------------------------------

void copyGR8BufferToTRasterBW(unsigned char *gr8Buffer, int rgbLx, int rgbLy,
                              const TRasterGR8P &rout, bool internal,
                              float thres) {
  if (internal) {
    unsigned char *dst = rout->getRawData();
    unsigned char *src = (gr8Buffer + rgbLx * rgbLy - 1);
    const int w        = rout->getWrap();

    for (int x = 0; x < rout->getLx(); ++x) {
      dst = rout->getRawData() + x;
      for (int y = 0; y < rout->getLy(); ++y) {
        if (*src < thres)
          *dst = 0;
        else
          *dst = 255;
        dst += w;
        --src;
      }
    }
  } else {
    memcpy(rout->getRawData(), gr8Buffer, rgbLx * rgbLy);
    rout->yMirror();
  }
}

//-----------------------------------------------------------------------------

void copyBWBufferToTRasterGR8(const unsigned char *buffer, int rgbLx, int rgbLy,
                              const TRasterGR8P &rout, bool isBW,
                              bool internal) {
  if (0)
    assert(0);
  else {
    int i      = 0;
    UCHAR *pix = rout->getRawData();
    const unsigned char *byte;
    while (i < rgbLx * rgbLy) {
      int bytePos = i / 8;
      int bitPos  = i % 8;
      byte        = buffer + bytePos;
      bool bit    = (*byte) >> (7 - bitPos);
      if (isBW)
        *pix = (bit ? 255 : 0);
      else
        *pix = (bit ? 0 : 255);

      pix++;
      i++;
    }
    rout->yMirror();
  }
}

//-----------------------------------------------------------------------------

void copy90BWBufferToRasGR8(unsigned char *bwBuffer, int bwLx, int bwLy,
                            int bwWrap, bool isBW, TRasterGR8P &rout,
                            int mirror, int ninety) {
  int x1   = 0;
  int y1   = 0;
  int x2   = bwLx - 1;
  int y2   = bwLy - 1;
  int newx = 0;
  int newy = 0;

  UCHAR *bufin, *bufout, /* *bytein,*/ *byteout;
  int bytewrapin, bitoffsin, wrapout;
  int value_for_0, value_for_1;
  int u, v, lu, lv, su, sv, u00, v00, u0, v0, dudy, dvdy, dudx, dvdx;
  int x, y, lx, ly;
  int u1, v1, u2, v2;

  mirror &= 1;
  ninety &= 3;

  if (!ninety && !mirror) {
    assert(0);
    return;
  }

  if (isBW) {
    value_for_0 = 0;
    value_for_1 = 255;
  } else {
    value_for_0 = 255;
    value_for_1 = 0;
  }

  u1 = x1;
  v1 = y1;
  u2 = x2;
  v2 = y2;

  su = u2 - u1;
  sv = v2 - v1;
  lu = u2 - u1 + 1;
  lv = v2 - v1 + 1;

  if (ninety & 1) {
    lx = lv;
    ly = lu;
  } else {
    lx = lu;
    ly = lv;
  }

  bufin      = bwBuffer;
  bytewrapin = (bwWrap + 7) >> 3;
  bitoffsin  = 0;  // rin->bit_offs;
  bufout     = rout->getRawData();
  wrapout    = rout->getWrap();

  dudx = 0;
  dudy = 0;
  dvdx = 0;
  dvdy = 0;

  switch ((mirror << 2) + ninety) {
  case (0 << 2) + 0:
    u00  = u1;
    v00  = v1;
    dudx = 1;
    dvdy = 1;
    break;
  case (0 << 2) + 1:
    u00  = u1;
    v00  = v1 + sv;
    dudy = 1;
    dvdx = -1;
    break;
  case (0 << 2) + 2:
    u00  = u1 + su;
    v00  = v1 + sv;
    dudx = -1;
    dvdy = -1;
    break;
  case (0 << 2) + 3:
    u00  = u1 + su;
    v00  = v1;
    dudy = -1;
    dvdx = 1;
    break;
  case (1 << 2) + 0:
    u00  = u1 + su;
    v00  = v1;
    dudx = -1;
    dvdy = 1;
    break;
  case (1 << 2) + 1:
    u00  = u1 + su;
    v00  = v1 + sv;
    dudy = -1;
    dvdx = -1;
    break;
  case (1 << 2) + 2:
    u00  = u1;
    v00  = v1 + sv;
    dudx = 1;
    dvdy = -1;
    break;
  case (1 << 2) + 3:
    u00  = u1;
    v00  = v1;
    dudy = 1;
    dvdx = 1;
    break;
  default:
    abort();
    u00 = v00 = dudy = dvdx = 0;
  }

  if (dudx)
    for (u0 = u00, v0 = v00, y = newy; y < newy + ly; v0 += dvdy, y++) {
      u       = u0;
      v       = v0;
      byteout = bufout + newx + y * wrapout;
      for (x = newx; x < newx + lx; u += dudx, x++) {
        if (GET_BIT(u, v, bufin, bytewrapin, bitoffsin))
          *byteout++ = value_for_1;
        else
          *byteout++ = value_for_0;
      }
    }
  else
    for (u0 = u00, v0 = v00, y = newy; y < newy + ly; u0 += dudy, y++) {
      u       = u0;
      v       = v0;
      byteout = bufout + newx + y * wrapout;
      for (x = newx; x < newx + lx; v += dvdx, x++) {
        if (GET_BIT(u, v, bufin, bytewrapin, bitoffsin))
          *byteout++ = value_for_1;
        else
          *byteout++ = value_for_0;
      }
    }
}
};
