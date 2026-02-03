#pragma once

#ifndef LOOP_MACROS_INCLUDED
#define LOOP_MACROS_INCLUDED

/*
          assert ((0 <= xI) && (xI <= up->getLx() - 1) && \
                  (0 <= yI) && (yI <= up->getLy() - 1)    ); \
*/

#define INTERNAL_LOOP_THE_FIRST                                                \
  xL += deltaXL;                                                               \
  yL += deltaYL;                                                               \
  xI          = xL >> PADN;                                                    \
  yI          = yL >> PADN;                                                    \
  upPix00     = upBasePix + (yI * upWrap + xI);                                \
  upPix10     = upPix00 + 1;                                                   \
  upPix01     = upPix00 + upWrap;                                              \
  upPix11     = upPix00 + upWrap + 1;                                          \
  xWeight1    = (xL & MASKN);                                                  \
  xWeight0    = (1 << PADN) - xWeight1;                                        \
  yWeight1    = (yL & MASKN);                                                  \
  yWeight0    = (1 << PADN) - yWeight1;                                        \
  c1          = MULT_2_X_16BIT(upPix00->r, upPix00->g, xWeight0);              \
  c3          = MULT_2_X_16BIT(upPix00->b, upPix01->r, xWeight0);              \
  c5          = MULT_2_X_16BIT(upPix01->g, upPix01->b, xWeight0);              \
  c2          = MULT_2_X_16BIT(upPix10->r, upPix10->g, xWeight1);              \
  c4          = MULT_2_X_16BIT(upPix10->b, upPix11->r, xWeight1);              \
  c6          = MULT_2_X_16BIT(upPix11->g, upPix11->b, xWeight1);              \
  s_gb        = (c5 + c6) >> 16;                                               \
  gColUpTmp   = s_gb >> 32;                                                    \
  bColUpTmp   = s_gb & 0x1FF;                                                  \
  s_br        = (c3 + c4) >> 16;                                               \
  bColDownTmp = s_br >> 32;                                                    \
  rColUpTmp   = s_br & 0x1FF;                                                  \
  s_rg        = (c1 + c2) >> 16;                                               \
  rColDownTmp = s_rg >> 32;                                                    \
  gColDownTmp = s_rg & 0x1FF;                                                  \
  rCol = (unsigned char)((yWeight0 * rColDownTmp + yWeight1 * rColUpTmp) >>    \
                         PADN);                                                \
  gCol = (unsigned char)((yWeight0 * gColDownTmp + yWeight1 * gColUpTmp) >>    \
                         PADN);                                                \
  bCol = (unsigned char)((yWeight0 * bColDownTmp + yWeight1 * bColUpTmp) >>    \
                         PADN);                                                \
  *dnPix = TPixel32(rCol, gCol, bCol, upPix00->m);

// -------------------------------------------------------------------------------------------------

#define INTERNAL_LOOP_THE_FIRST_X_2                                            \
  INTERNAL_LOOP_THE_FIRST                                                      \
  ++dnPix;                                                                     \
  INTERNAL_LOOP_THE_FIRST

// -------------------------------------------------------------------------------------------------

#define INTERNAL_LOOP_THE_FIRST_X_4                                            \
  INTERNAL_LOOP_THE_FIRST_X_2                                                  \
  ++dnPix;                                                                     \
  INTERNAL_LOOP_THE_FIRST_X_2

// -------------------------------------------------------------------------------------------------

#define INTERNAL_LOOP_THE_FIRST_X_8                                            \
  INTERNAL_LOOP_THE_FIRST_X_4                                                  \
  ++dnPix;                                                                     \
  INTERNAL_LOOP_THE_FIRST_X_4

// -------------------------------------------------------------------------------------------------

#define INTERNAL_LOOP_THE_FIRST_X_16                                           \
  INTERNAL_LOOP_THE_FIRST_X_8                                                  \
  ++dnPix;                                                                     \
  INTERNAL_LOOP_THE_FIRST_X_8

// -------------------------------------------------------------------------------------------------

#define INTERNAL_LOOP_THE_FIRST_X_32                                           \
  INTERNAL_LOOP_THE_FIRST_X_16                                                 \
  ++dnPix;                                                                     \
  INTERNAL_LOOP_THE_FIRST_X_16

// -------------------------------------------------------------------------------------------------

#define EXTERNAL_LOOP_THE_FIRST                                                \
  a     = invAff * TPointD(xMin, y);                                           \
  xL0   = tround(a.x * (1 << PADN));                                           \
  yL0   = tround(a.y * (1 << PADN));                                           \
  kMinX = 0;                                                                   \
  kMaxX = xMax - xMin;                                                         \
  kMinY = 0;                                                                   \
  kMaxY = xMax - xMin;                                                         \
  if (deltaXL == 0) {                                                          \
    if ((xL0 < 0) || (lxPred < xL0)) continue;                                 \
  } else if (deltaXL > 0) {                                                    \
    if (lxPred < xL0) continue;                                                \
    kMaxX = (lxPred - xL0) / deltaXL;                                          \
    if (xL0 < 0) {                                                             \
      kMinX = ((-xL0) + deltaXL - 1) / deltaXL;                                \
    }                                                                          \
  } else {                                                                     \
    if (xL0 < 0) continue;                                                     \
    kMaxX = xL0 / (-deltaXL);                                                  \
    if (lxPred < xL0) {                                                        \
      kMinX = (xL0 - lxPred - deltaXL - 1) / (-deltaXL);                       \
    }                                                                          \
  }                                                                            \
  if (deltaYL == 0) {                                                          \
    if ((yL0 < 0) || (lyPred < yL0)) continue;                                 \
  } else if (deltaYL > 0) {                                                    \
    if (lyPred < yL0) continue;                                                \
    kMaxY = (lyPred - yL0) / deltaYL;                                          \
    if (yL0 < 0) {                                                             \
      kMinY = ((-yL0) + deltaYL - 1) / deltaYL;                                \
    }                                                                          \
  } else {                                                                     \
    if (yL0 < 0) continue;                                                     \
    kMaxY = yL0 / (-deltaYL);                                                  \
    if (lyPred < yL0) {                                                        \
      kMinY = (yL0 - lyPred - deltaYL - 1) / (-deltaYL);                       \
    }                                                                          \
  }                                                                            \
  kMin     = std::max({kMinX, kMinY, (int)0});                                 \
  kMax     = std::min(kMaxX, kMaxY, xMax - xMin);                              \
  dnPix    = dnRow + xMin + kMin;                                              \
  dnEndPix = dnRow + xMin + kMax + 1;                                          \
  xL       = xL0 + (kMin - 1) * deltaXL;                                       \
  yL       = yL0 + (kMin - 1) * deltaYL;                                       \
  for (; dnPix < dnEndPix - 32; ++dnPix) {                                     \
    INTERNAL_LOOP_THE_FIRST_X_32;                                              \
  }                                                                            \
  for (; dnPix < dnEndPix - 16; ++dnPix) {                                     \
    INTERNAL_LOOP_THE_FIRST_X_16;                                              \
  }                                                                            \
  for (; dnPix < dnEndPix - 8; ++dnPix) {                                      \
    INTERNAL_LOOP_THE_FIRST_X_8;                                               \
  }                                                                            \
  for (; dnPix < dnEndPix - 4; ++dnPix) {                                      \
    INTERNAL_LOOP_THE_FIRST_X_4;                                               \
  }                                                                            \
  for (; dnPix < dnEndPix - 2; ++dnPix) {                                      \
    INTERNAL_LOOP_THE_FIRST_X_2;                                               \
  }                                                                            \
  for (; dnPix < dnEndPix; ++dnPix) {                                          \
    INTERNAL_LOOP_THE_FIRST                                                    \
  }

// -------------------------------------------------------------------------------------------------

#define EXTERNAL_LOOP_THE_FIRST_X_2                                            \
  EXTERNAL_LOOP_THE_FIRST                                                      \
  ++y;                                                                         \
  dnRow += dnWrap;                                                             \
  EXTERNAL_LOOP_THE_FIRST

// -------------------------------------------------------------------------------------------------

#define EXTERNAL_LOOP_THE_FIRST_X_4                                            \
  EXTERNAL_LOOP_THE_FIRST_X_2                                                  \
  ++y;                                                                         \
  dnRow += dnWrap;                                                             \
  EXTERNAL_LOOP_THE_FIRST_X_2

// -------------------------------------------------------------------------------------------------

#define EXTERNAL_LOOP_THE_FIRST_X_8                                            \
  EXTERNAL_LOOP_THE_FIRST_X_4                                                  \
  ++y;                                                                         \
  dnRow += dnWrap;                                                             \
  EXTERNAL_LOOP_THE_FIRST_X_4

// -------------------------------------------------------------------------------------------------

#define EXTERNAL_LOOP_THE_FIRST_X_16                                           \
  EXTERNAL_LOOP_THE_FIRST_X_8                                                  \
  ++y;                                                                         \
  dnRow += dnWrap;                                                             \
  EXTERNAL_LOOP_THE_FIRST_X_8

// -------------------------------------------------------------------------------------------------

#define EXTERNAL_LOOP_THE_FIRST_X_32                                           \
  EXTERNAL_LOOP_THE_FIRST_X_16                                                 \
  ++y;                                                                         \
  dnRow += dnWrap;                                                             \
  EXTERNAL_LOOP_THE_FIRST_X_16

/*
          assert ((0 <= xI) && (xI <= up->getLx() - 1) &&  \
                  (0 <= yI) && (yI <= up->getLy() - 1)    ); \
*/

// -------------------------------------------------------------------------------------------------

#define INTERNAL_LOOP_THE_SECOND                                               \
  xL += deltaXL;                                                               \
  xI          = xL >> PADN;                                                    \
  upPix00     = upBasePix + (yI * upWrap + xI);                                \
  upPix10     = upPix00 + 1;                                                   \
  upPix01     = upPix00 + upWrap;                                              \
  upPix11     = upPix00 + upWrap + 1;                                          \
  xWeight1    = (xL & MASKN);                                                  \
  xWeight0    = (1 << PADN) - xWeight1;                                        \
  c1          = MULT_2_X_16BIT(upPix00->r, upPix00->g, xWeight0);              \
  c3          = MULT_2_X_16BIT(upPix00->b, upPix01->r, xWeight0);              \
  c5          = MULT_2_X_16BIT(upPix01->g, upPix01->b, xWeight0);              \
  c2          = MULT_2_X_16BIT(upPix10->r, upPix10->g, xWeight1);              \
  c4          = MULT_2_X_16BIT(upPix10->b, upPix11->r, xWeight1);              \
  c6          = MULT_2_X_16BIT(upPix11->g, upPix11->b, xWeight1);              \
  s_gb        = (c5 + c6) >> 16;                                               \
  gColUpTmp   = s_gb >> 32;                                                    \
  bColUpTmp   = s_gb & 0x1FF;                                                  \
  s_br        = (c3 + c4) >> 16;                                               \
  bColDownTmp = s_br >> 32;                                                    \
  rColUpTmp   = s_br & 0x1FF;                                                  \
  s_rg        = (c1 + c2) >> 16;                                               \
  rColDownTmp = s_rg >> 32;                                                    \
  gColDownTmp = s_rg & 0x1FF;                                                  \
  rCol = (unsigned char)((yWeight0 * rColDownTmp + yWeight1 * rColUpTmp) >>    \
                         PADN);                                                \
  gCol = (unsigned char)((yWeight0 * gColDownTmp + yWeight1 * gColUpTmp) >>    \
                         PADN);                                                \
  bCol = (unsigned char)((yWeight0 * bColDownTmp + yWeight1 * bColUpTmp) >>    \
                         PADN);                                                \
  *dnPix = TPixel32(rCol, gCol, bCol, upPix00->m);

// -------------------------------------------------------------------------------------------------

#define INTERNAL_LOOP_THE_SECOND_X_2                                           \
  INTERNAL_LOOP_THE_SECOND                                                     \
  ++dnPix;                                                                     \
  INTERNAL_LOOP_THE_SECOND

// -------------------------------------------------------------------------------------------------

#define INTERNAL_LOOP_THE_SECOND_X_4                                           \
  INTERNAL_LOOP_THE_SECOND_X_2                                                 \
  ++dnPix;                                                                     \
  INTERNAL_LOOP_THE_SECOND_X_2

// -------------------------------------------------------------------------------------------------

#define INTERNAL_LOOP_THE_SECOND_X_8                                           \
  INTERNAL_LOOP_THE_SECOND_X_4                                                 \
  ++dnPix;                                                                     \
  INTERNAL_LOOP_THE_SECOND_X_4

// -------------------------------------------------------------------------------------------------

#define INTERNAL_LOOP_THE_SECOND_X_16                                          \
  INTERNAL_LOOP_THE_SECOND_X_8                                                 \
  ++dnPix;                                                                     \
  INTERNAL_LOOP_THE_SECOND_X_8

// -------------------------------------------------------------------------------------------------

#define INTERNAL_LOOP_THE_SECOND_X_32                                          \
  INTERNAL_LOOP_THE_SECOND_X_16                                                \
  ++dnPix;                                                                     \
  INTERNAL_LOOP_THE_SECOND_X_16

// -------------------------------------------------------------------------------------------------

#define EXTERNAL_LOOP_THE_SECOND                                               \
  xL = xL0 + (kMinX - 1) * deltaXL;                                            \
  yL += deltaYL;                                                               \
  yI       = yL >> PADN;                                                       \
  yWeight1 = (yL & MASKN);                                                     \
  yWeight0 = (1 << PADN) - yWeight1;                                           \
  dnPix    = dnRow + xMin + kMinX;                                             \
  dnEndPix = dnRow + xMin + kMaxX + 1;                                         \
  for (; dnPix < dnEndPix - 32; ++dnPix) {                                     \
    INTERNAL_LOOP_THE_SECOND_X_32;                                             \
  }                                                                            \
  for (; dnPix < dnEndPix - 16; ++dnPix) {                                     \
    INTERNAL_LOOP_THE_SECOND_X_16;                                             \
  }                                                                            \
  for (; dnPix < dnEndPix - 8; ++dnPix) {                                      \
    INTERNAL_LOOP_THE_SECOND_X_8;                                              \
  }                                                                            \
  for (; dnPix < dnEndPix - 4; ++dnPix) {                                      \
    INTERNAL_LOOP_THE_SECOND_X_4;                                              \
  }                                                                            \
  for (; dnPix < dnEndPix - 2; ++dnPix) {                                      \
    INTERNAL_LOOP_THE_SECOND_X_2;                                              \
  }                                                                            \
  for (; dnPix < dnEndPix; ++dnPix) {                                          \
    INTERNAL_LOOP_THE_SECOND                                                   \
  }

// -------------------------------------------------------------------------------------------------

#define EXTERNAL_LOOP_THE_SECOND_X_2                                           \
  EXTERNAL_LOOP_THE_SECOND                                                     \
  ++kY;                                                                        \
  dnRow += dnWrap;                                                             \
  EXTERNAL_LOOP_THE_SECOND

// -------------------------------------------------------------------------------------------------

#define EXTERNAL_LOOP_THE_SECOND_X_4                                           \
  EXTERNAL_LOOP_THE_SECOND_X_2                                                 \
  ++kY;                                                                        \
  dnRow += dnWrap;                                                             \
  EXTERNAL_LOOP_THE_SECOND_X_2

// -------------------------------------------------------------------------------------------------

#define EXTERNAL_LOOP_THE_SECOND_X_8                                           \
  EXTERNAL_LOOP_THE_SECOND_X_4                                                 \
  ++kY;                                                                        \
  dnRow += dnWrap;                                                             \
  EXTERNAL_LOOP_THE_SECOND_X_4

// -------------------------------------------------------------------------------------------------

#define EXTERNAL_LOOP_THE_SECOND_X_16                                          \
  EXTERNAL_LOOP_THE_SECOND_X_8                                                 \
  ++kY;                                                                        \
  dnRow += dnWrap;                                                             \
  EXTERNAL_LOOP_THE_SECOND_X_8

// -------------------------------------------------------------------------------------------------

#define EXTERNAL_LOOP_THE_SECOND_X_32                                          \
  EXTERNAL_LOOP_THE_SECOND_X_16                                                \
  ++kY;                                                                        \
  dnRow += dnWrap;                                                             \
  EXTERNAL_LOOP_THE_SECOND_X_16

#endif
