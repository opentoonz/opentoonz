

#include "traster.h"
#include "trop.h"
#include "tpixelgr.h"

#if defined(_WIN32) && defined(x64)
#define USE_SSE2
#endif

#ifdef USE_SSE2
#include <emmintrin.h>
#include <stdlib.h>
#endif

namespace {

#ifdef _WIN32
template <class T>
struct BlurPixel {
  T b;
  T g;
  T r;
  T m;
};

#else
template <class T>
struct BlurPixel {
  T r;
  T g;
  T b;
  T m;
};

#endif

//===================================================================

#define LOAD_COL_CODE                                                          \
                                                                               \
  buffer += x;                                                                 \
  pix = col + by1;                                                             \
                                                                               \
  for (i = by1; i < ly + by1; i++) {                                           \
    *pix++ = *buffer;                                                          \
    buffer += lx;                                                              \
  }                                                                            \
                                                                               \
  pix += by2;                                                                  \
  left_val  = col[0];                                                          \
  right_val = *(pix - 1);                                                      \
  col--;                                                                       \
                                                                               \
  for (i = 0; i < brad; i++) {                                                 \
    *col-- = left_val;                                                         \
    *pix++ = right_val;                                                        \
  }

//-------------------------------------------------------------------

#define BLUR_CODE(round_fac, channel_type)                                     \
  pix1 = row1;                                                                 \
  pix2 = row1 - 1;                                                             \
                                                                               \
  sigma1.r = pix1->r;                                                          \
  sigma1.g = pix1->g;                                                          \
  sigma1.b = pix1->b;                                                          \
  sigma1.m = pix1->m;                                                          \
  pix1++;                                                                      \
                                                                               \
  sigma2.r = sigma2.g = sigma2.b = sigma2.m = 0.0;                             \
  sigma3.r = sigma3.g = sigma3.b = sigma3.m = 0.0;                             \
                                                                               \
  for (i = 1; i < brad; i++) {                                                 \
    sigma1.r += pix1->r;                                                       \
    sigma1.g += pix1->g;                                                       \
    sigma1.b += pix1->b;                                                       \
    sigma1.m += pix1->m;                                                       \
                                                                               \
    sigma2.r += pix2->r;                                                       \
    sigma2.g += pix2->g;                                                       \
    sigma2.b += pix2->b;                                                       \
    sigma2.m += pix2->m;                                                       \
                                                                               \
    sigma3.r += i * (pix1->r + pix2->r);                                       \
    sigma3.g += i * (pix1->g + pix2->g);                                       \
    sigma3.b += i * (pix1->b + pix2->b);                                       \
    sigma3.m += i * (pix1->m + pix2->m);                                       \
                                                                               \
    pix1++;                                                                    \
    pix2--;                                                                    \
  }                                                                            \
                                                                               \
  rsum = (sigma1.r + sigma2.r) * coeff - sigma3.r * coeffq + (round_fac);      \
  gsum = (sigma1.g + sigma2.g) * coeff - sigma3.g * coeffq + (round_fac);      \
  bsum = (sigma1.b + sigma2.b) * coeff - sigma3.b * coeffq + (round_fac);      \
  msum = (sigma1.m + sigma2.m) * coeff - sigma3.m * coeffq + (round_fac);      \
                                                                               \
  row2->r = (channel_type)(rsum);                                              \
  row2->g = (channel_type)(gsum);                                              \
  row2->b = (channel_type)(bsum);                                              \
  row2->m = (channel_type)(msum);                                              \
  row2++;                                                                      \
                                                                               \
  sigma2.r += row1[-brad].r;                                                   \
  sigma2.g += row1[-brad].g;                                                   \
  sigma2.b += row1[-brad].b;                                                   \
  sigma2.m += row1[-brad].m;                                                   \
                                                                               \
  pix1 = row1 + brad;                                                          \
  pix2 = row1;                                                                 \
  pix3 = row1 - brad;                                                          \
  pix4 = row1 - brad + 1;                                                      \
                                                                               \
  desigma.r = sigma1.r - sigma2.r;                                             \
  desigma.g = sigma1.g - sigma2.g;                                             \
  desigma.b = sigma1.b - sigma2.b;                                             \
  desigma.m = sigma1.m - sigma2.m;                                             \
                                                                               \
  for (i = 1; i < length; i++) {                                               \
    desigma.r += pix1->r - 2 * pix2->r + pix3->r;                              \
    desigma.g += pix1->g - 2 * pix2->g + pix3->g;                              \
    desigma.b += pix1->b - 2 * pix2->b + pix3->b;                              \
    desigma.m += pix1->m - 2 * pix2->m + pix3->m;                              \
                                                                               \
    rsum += (desigma.r + diff * (pix1->r - pix4->r)) * coeffq;                 \
    gsum += (desigma.g + diff * (pix1->g - pix4->g)) * coeffq;                 \
    bsum += (desigma.b + diff * (pix1->b - pix4->b)) * coeffq;                 \
    msum += (desigma.m + diff * (pix1->m - pix4->m)) * coeffq;                 \
                                                                               \
    row2->r = (channel_type)(rsum);                                            \
    row2->g = (channel_type)(gsum);                                            \
    row2->b = (channel_type)(bsum);                                            \
    row2->m = (channel_type)(msum);                                            \
    row2++;                                                                    \
    pix1++, pix2++, pix3++, pix4++;                                            \
  }

//-------------------------------------------------------------------

template <typename PIXEL_SRC, typename PIXEL_DST, typename T>
inline void blur_code(PIXEL_SRC *row1, PIXEL_DST *row2, int length, float coeff,
                      float coeffq, int brad, float diff, float round_fac) {
  int i;
  T rsum, gsum, bsum, msum;

  BlurPixel<T> sigma1, sigma2, sigma3, desigma;
  PIXEL_SRC *pix1, *pix2, *pix3, *pix4;

  pix1 = row1;
  pix2 = row1 - 1;

  sigma1.r = pix1->r;
  sigma1.g = pix1->g;
  sigma1.b = pix1->b;
  sigma1.m = pix1->m;
  pix1++;

  sigma2.r = sigma2.g = sigma2.b = sigma2.m = 0.0;
  sigma3.r = sigma3.g = sigma3.b = sigma3.m = 0.0;

  for (i = 1; i < brad; i++) {
    sigma1.r += pix1->r;
    sigma1.g += pix1->g;
    sigma1.b += pix1->b;
    sigma1.m += pix1->m;

    sigma2.r += pix2->r;
    sigma2.g += pix2->g;
    sigma2.b += pix2->b;
    sigma2.m += pix2->m;

    sigma3.r += (T)i * (pix1->r + pix2->r);
    sigma3.g += (T)i * (pix1->g + pix2->g);
    sigma3.b += (T)i * (pix1->b + pix2->b);
    sigma3.m += (T)i * (pix1->m + pix2->m);

    pix1++;
    pix2--;
  }

  rsum = (sigma1.r + sigma2.r) * coeff - sigma3.r * coeffq + (round_fac);
  gsum = (sigma1.g + sigma2.g) * coeff - sigma3.g * coeffq + (round_fac);
  bsum = (sigma1.b + sigma2.b) * coeff - sigma3.b * coeffq + (round_fac);
  msum = (sigma1.m + sigma2.m) * coeff - sigma3.m * coeffq + (round_fac);

  row2->r = rsum;
  row2->g = gsum;
  row2->b = bsum;
  row2->m = msum;
  row2++;

  sigma2.r += row1[-brad].r;
  sigma2.g += row1[-brad].g;
  sigma2.b += row1[-brad].b;
  sigma2.m += row1[-brad].m;

  pix1 = row1 + brad;
  pix2 = row1;
  pix3 = row1 - brad;
  pix4 = row1 - brad + 1;

  desigma.r = sigma1.r - sigma2.r;
  desigma.g = sigma1.g - sigma2.g;
  desigma.b = sigma1.b - sigma2.b;
  desigma.m = sigma1.m - sigma2.m;

  for (i = 1; i < length; i++) {
    desigma.r += pix1->r - 2 * pix2->r + pix3->r;
    desigma.g += pix1->g - 2 * pix2->g + pix3->g;
    desigma.b += pix1->b - 2 * pix2->b + pix3->b;
    desigma.m += pix1->m - 2 * pix2->m + pix3->m;

    rsum += (desigma.r + diff * (pix1->r - pix4->r)) * coeffq;
    gsum += (desigma.g + diff * (pix1->g - pix4->g)) * coeffq;
    bsum += (desigma.b + diff * (pix1->b - pix4->b)) * coeffq;
    msum += (desigma.m + diff * (pix1->m - pix4->m)) * coeffq;

    row2->r = rsum;
    row2->g = gsum;
    row2->b = bsum;
    row2->m = msum;
    row2++;
    pix1++, pix2++, pix3++, pix4++;
  }
}

//-------------------------------------------------------------------

#ifdef USE_SSE2

//-------------------------------------------------------------------
template <class T, class P>
inline void blur_code_SSE2(T *row1, BlurPixel<P> *row2, int length, float coeff,
                           float coeffq, int brad, float diff,
                           float round_fac) {
  static float two     = 2;
  static __m128i zeros = _mm_setzero_si128();
  static __m128 twos   = _mm_load_ps1(&two);

  int i;

  __m128 sigma1, sigma2, sigma3, desigma;
  T *pix1, *pix2, *pix3, *pix4;

  pix1 = row1;
  pix2 = row1 - 1;

  //
  __m128i piPix1 = _mm_cvtsi32_si128(*(DWORD *)pix1);
  __m128i piPix2 = _mm_cvtsi32_si128(*(DWORD *)pix2);

  piPix1 = _mm_unpacklo_epi8(piPix1, zeros);
  piPix2 = _mm_unpacklo_epi8(piPix2, zeros);
  piPix1 = _mm_unpacklo_epi16(piPix1, zeros);
  piPix2 = _mm_unpacklo_epi16(piPix2, zeros);

  sigma1 = _mm_cvtepi32_ps(piPix1);
  //

  pix1++;

  float zero = 0;
  sigma2     = _mm_load1_ps(&zero);
  sigma3     = _mm_load1_ps(&zero);

  for (i = 1; i < brad; i++) {
    piPix1 = _mm_unpacklo_epi8(_mm_cvtsi32_si128(*(DWORD *)pix1), zeros);
    piPix2 = _mm_unpacklo_epi8(_mm_cvtsi32_si128(*(DWORD *)pix2), zeros);

    __m128 pPix1 = _mm_cvtepi32_ps(_mm_unpacklo_epi16(piPix1, zeros));
    __m128 pPix2 = _mm_cvtepi32_ps(_mm_unpacklo_epi16(piPix2, zeros));

    sigma1 = _mm_add_ps(sigma1, pPix1);
    sigma2 = _mm_add_ps(sigma2, pPix2);

    __m128i pii = _mm_unpacklo_epi8(_mm_cvtsi32_si128(i), zeros);
    __m128 pi   = _mm_cvtepi32_ps(_mm_unpacklo_epi16(pii, zeros));

    pPix1  = _mm_add_ps(pPix1, pPix2);
    pPix1  = _mm_mul_ps(pi, pPix1);      // i*(pix1 + pix2)
    sigma3 = _mm_add_ps(sigma3, pPix1);  // sigma3 += i*(pix1 + pix2)

    pix1++;
    pix2--;
  }

  __m128 pCoeff    = _mm_load1_ps(&coeff);
  __m128 pCoeffq   = _mm_load1_ps(&coeffq);
  __m128 pRoundFac = _mm_load1_ps(&round_fac);
  __m128 pDiff     = _mm_load1_ps(&diff);

  // sum = (sigma1 + sigma2)*coeff - sigma3*coeffq + round_fac
  __m128 sum  = _mm_add_ps(sigma1, sigma2);
  sum         = _mm_mul_ps(sum, pCoeff);
  __m128 sum2 = _mm_mul_ps(sigma3, pCoeffq);
  sum2        = _mm_add_ps(sum2, pRoundFac);
  sum         = _mm_sub_ps(sum, sum2);
  /*
  __m128i isum = _mm_cvtps_epi32(sum);
isum = _mm_packs_epi32(isum, zeros);
isum = _mm_packs_epi16(isum, zeros);

*(DWORD*)row2 = _mm_cvtsi128_si32(isum);
*/
  _mm_store_ps((float *)row2, sum);
  row2++;

  __m128i piPixMin =
      _mm_unpacklo_epi8(_mm_cvtsi32_si128(*(DWORD *)(row1 - brad)), zeros);
  __m128 pPixMin = _mm_cvtepi32_ps(_mm_unpacklo_epi16(piPixMin, zeros));
  sigma2         = _mm_add_ps(sigma2, pPixMin);
  /*
sigma2.r += row1[-brad].r;
sigma2.g += row1[-brad].g;
sigma2.b += row1[-brad].b;
sigma2.m += row1[-brad].m;
*/

  pix1 = row1 + brad;
  pix2 = row1;
  pix3 = row1 - brad;
  pix4 = row1 - brad + 1;

  desigma = _mm_sub_ps(sigma1, sigma2);

  for (i = 1; i < length; i++) {
    piPix1 = _mm_unpacklo_epi8(_mm_cvtsi32_si128(*(DWORD *)pix1), zeros);
    piPix2 = _mm_unpacklo_epi8(_mm_cvtsi32_si128(*(DWORD *)pix2), zeros);
    __m128i piPix3 =
        _mm_unpacklo_epi8(_mm_cvtsi32_si128(*(DWORD *)pix3), zeros);
    __m128i piPix4 =
        _mm_unpacklo_epi8(_mm_cvtsi32_si128(*(DWORD *)pix4), zeros);

    __m128 pPix1 = _mm_cvtepi32_ps(_mm_unpacklo_epi16(piPix1, zeros));
    __m128 pPix2 =
        _mm_cvtepi32_ps(_mm_slli_epi32(_mm_unpacklo_epi16(piPix2, zeros), 1));
    __m128 pPix3 = _mm_cvtepi32_ps(_mm_unpacklo_epi16(piPix3, zeros));
    __m128 pPix4 = _mm_cvtepi32_ps(_mm_unpacklo_epi16(piPix4, zeros));

    // desigma += pix1 - 2*pix2 + pix3
    __m128 tmp = _mm_sub_ps(pPix3, pPix2);
    tmp        = _mm_add_ps(tmp, pPix1);
    desigma    = _mm_add_ps(desigma, tmp);

    // sum += (desigma + diff*(pix1 - pix4))*coeffq
    tmp = _mm_sub_ps(pPix1, pPix4);
    tmp = _mm_mul_ps(tmp, pDiff);
    tmp = _mm_add_ps(desigma, tmp);
    tmp = _mm_mul_ps(tmp, pCoeffq);
    sum = _mm_add_ps(sum, tmp);
    /*
isum = _mm_cvtps_epi32(sum);
isum = _mm_packs_epi32(isum, zeros);
isum = _mm_packs_epi16(isum, zeros);

*(DWORD*)row2 = _mm_cvtsi128_si32(isum);
*/
    _mm_store_ps((float *)row2, sum);

    row2++;
    pix1++, pix2++, pix3++, pix4++;
  }
}

//-------------------------------------------------------------------
template <class T, class P>
inline void blur_code_SSE2(BlurPixel<P> *row1, T *row2, int length, float coeff,
                           float coeffq, int brad, float diff,
                           float round_fac) {
  int i;

  float two     = 2;
  __m128i zeros = _mm_setzero_si128();
  __m128 twos   = _mm_load_ps1(&two);

  __m128 sigma1, sigma2, sigma3, desigma;
  BlurPixel<P> *pix1, *pix2, *pix3, *pix4;

  pix1 = row1;
  pix2 = row1 - 1;

  __m128 pPix1 = _mm_load_ps((float *)pix1);
  __m128 pPix2 = _mm_load_ps((float *)pix2);

  // sigma1 = *pix1
  sigma1 = pPix1;

  pix1++;

  float zero = 0;
  sigma2     = _mm_load1_ps(&zero);
  sigma3     = _mm_load1_ps(&zero);

  for (i = 1; i < brad; i++) {
    pPix1 = _mm_load_ps((float *)pix1);
    pPix2 = _mm_load_ps((float *)pix2);

    sigma1 = _mm_add_ps(sigma1, pPix1);
    sigma2 = _mm_add_ps(sigma2, pPix2);

    __m128i pii = _mm_unpacklo_epi8(_mm_cvtsi32_si128(i), zeros);
    __m128 pi   = _mm_cvtepi32_ps(_mm_unpacklo_epi16(pii, zeros));

    pPix1  = _mm_add_ps(pPix1, pPix2);
    pPix1  = _mm_mul_ps(pi, pPix1);      // i*(pix1 + pix2)
    sigma3 = _mm_add_ps(sigma3, pPix1);  // sigma3 += i*(pix1 + pix2)

    pix1++;
    pix2--;
  }

  __m128 pCoeff  = _mm_load1_ps(&coeff);
  __m128 pCoeffq = _mm_load1_ps(&coeffq);
  //  __m128 pRoundFac = _mm_load1_ps(&round_fac);
  __m128 pDiff = _mm_load1_ps(&diff);

  // sum = (sigma1 + sigma2)*coeff - sigma3*coeffq + round_fac
  __m128 sum  = _mm_add_ps(sigma1, sigma2);
  sum         = _mm_mul_ps(sum, pCoeff);
  __m128 sum2 = _mm_mul_ps(sigma3, pCoeffq);
  // sum2 = _mm_add_ps(sum2, pRoundFac);
  sum = _mm_sub_ps(sum, sum2);

  // converte i canali da float a char
  __m128i isum = _mm_cvtps_epi32(sum);
  isum         = _mm_packs_epi32(isum, zeros);
  // isum = _mm_packs_epi16(isum, zeros);
  isum           = _mm_packus_epi16(isum, zeros);
  *(DWORD *)row2 = _mm_cvtsi128_si32(isum);

  row2++;

  // sigma2 += row1[-brad]
  __m128 pPixMin = _mm_load_ps((float *)(row1 - brad));
  sigma2         = _mm_add_ps(sigma2, pPixMin);

  pix1 = row1 + brad;
  pix2 = row1;
  pix3 = row1 - brad;
  pix4 = row1 - brad + 1;

  desigma = _mm_sub_ps(sigma1, sigma2);

  for (i = 1; i < length; i++) {
    __m128 pPix1 = _mm_load_ps((float *)pix1);
    __m128 pPix2 = _mm_load_ps((float *)pix2);
    __m128 pPix3 = _mm_load_ps((float *)pix3);
    __m128 pPix4 = _mm_load_ps((float *)pix4);

    pPix2 = _mm_mul_ps(pPix2, twos);

    // desigma += pix1 - 2*pix2 + pix3
    __m128 tmp = _mm_sub_ps(pPix3, pPix2);
    tmp        = _mm_add_ps(tmp, pPix1);
    desigma    = _mm_add_ps(desigma, tmp);

    // sum += (desigma + diff*(pix1 - pix4))*coeffq
    tmp = _mm_sub_ps(pPix1, pPix4);
    tmp = _mm_mul_ps(tmp, pDiff);
    tmp = _mm_add_ps(desigma, tmp);
    tmp = _mm_mul_ps(tmp, pCoeffq);
    sum = _mm_add_ps(sum, tmp);

    // converte i canali da float a char
    __m128i isum = _mm_cvtps_epi32(sum);
    isum         = _mm_packs_epi32(isum, zeros);
    // isum = _mm_packs_epi16(isum, zeros);  // QUESTA RIGA E' SBAGLIATA
    // assert(false);
    isum           = _mm_packus_epi16(isum, zeros);
    *(DWORD *)row2 = _mm_cvtsi128_si32(isum);

    row2++;
    pix1++, pix2++, pix3++, pix4++;
  }
}

#endif  // _WIN32

//-------------------------------------------------------------------

#define STORE_COL_CODE(crop_val)                                               \
  {                                                                            \
    int i, val;                                                                \
    double ampl;                                                               \
    buffer += x;                                                               \
                                                                               \
    ampl = 1.0 + blur / 15.0;                                                  \
                                                                               \
    if (backlit)                                                               \
      for (i = ((dy >= 0) ? 0 : -dy); i < std::min(ly, r_ly - dy); i++) {      \
        val       = troundp(col[i].r * ampl);                                  \
        buffer->r = (val > crop_val) ? crop_val : val;                         \
        val       = troundp(col[i].g * ampl);                                  \
        buffer->g = (val > crop_val) ? crop_val : val;                         \
        val       = troundp(col[i].b * ampl);                                  \
        buffer->b = (val > crop_val) ? crop_val : val;                         \
        val       = troundp(col[i].m * ampl);                                  \
        buffer->m = (val > crop_val) ? crop_val : val;                         \
        buffer += wrap;                                                        \
      }                                                                        \
    else                                                                       \
      for (i = ((dy >= 0) ? 0 : -dy); i < std::min(ly, r_ly - dy); i++) {      \
        *buffer = col[i];                                                      \
        buffer += wrap;                                                        \
      }                                                                        \
  }

//-------------------------------------------------------------------
template <class T>
void store_colRgb(T *buffer, int wrap, int r_ly, T *col, int ly, int x, int dy,
                  int backlit, double blur) {
  int val = T::maxChannelValue;

  if (val == 255)
    STORE_COL_CODE(204)
  else if (val == 65535)
    STORE_COL_CODE(204 * 257)
  else
    assert(false);
}

template <>
void store_colRgb<TPixelF>(TPixelF *buffer, int wrap, int r_ly, TPixelF *col,
                           int ly, int x, int dy, int backlit, double blur) {
  int i;
  buffer += x;
  if (backlit) {
    double ampl    = 1.0 + blur / 15.0;
    float crop_val = 204.f / 255.f;
    for (i = ((dy >= 0) ? 0 : -dy); i < std::min(ly, r_ly - dy); i++) {
      float val = col[i].r * ampl;
      buffer->r = (val > crop_val) ? crop_val : val;
      val       = col[i].g * ampl;
      buffer->g = (val > crop_val) ? crop_val : val;
      val       = col[i].b * ampl;
      buffer->b = (val > crop_val) ? crop_val : val;
      val       = col[i].m * ampl;
      buffer->m = (val > crop_val) ? crop_val : val;
      buffer += wrap;
    }
  } else
    for (i = ((dy >= 0) ? 0 : -dy); i < std::min(ly, r_ly - dy); i++) {
      *buffer = col[i];
      buffer += wrap;
    }
}
//-------------------------------------------------------------------
template <class T>
void store_colGray(T *buffer, int wrap, int r_ly, T *col, int ly, int x, int dy,
                   int backlit, double blur) {
  int i;
  double ampl;
  buffer += x;

  ampl = 1.0 + blur / 15.0;

  for (i = ((dy >= 0) ? 0 : -dy); i < std::min(ly, r_ly - dy); i++) {
    *buffer = col[i];
    buffer += wrap;
  }
}

//-------------------------------------------------------------------
template <class P>
void load_colRgb(BlurPixel<P> *buffer, BlurPixel<P> *col, int lx, int ly, int x,
                 int brad, int by1, int by2) {
  int i;
  BlurPixel<P> *pix, left_val, right_val;

  LOAD_COL_CODE
}

//-------------------------------------------------------------------

void load_channel_col32(float *buffer, float *col, int lx, int ly, int x,
                        int brad, int by1, int by2) {
  int i;
  float *pix, left_val, right_val;

  LOAD_COL_CODE
}

//-------------------------------------------------------------------
template <class T, class Q, class P>
void do_filtering_chan(BlurPixel<P> *row1, T *row2, int length, float coeff,
                       float coeffq, int brad, float diff, bool useSSE) {
#ifdef USE_SSE2
  if (useSSE && T::maxChannelValue == 255)
    blur_code_SSE2<T, P>(row1, row2, length, coeff, coeffq, brad, diff, 0.5);
  else
#endif
  {
    int i;
    P rsum, gsum, bsum, msum;
    BlurPixel<P> sigma1, sigma2, sigma3, desigma;
    BlurPixel<P> *pix1, *pix2, *pix3, *pix4;

    BLUR_CODE((P)0.5, Q)
  }
}
template <>
void do_filtering_chan<TPixelF, float, double>(BlurPixel<double> *row1,
                                               TPixelF *row2, int length,
                                               float coeff, float coeffq,
                                               int brad, float diff,
                                               bool useSSE) {
  int i;
  double rsum, gsum, bsum, msum;
  BlurPixel<double> sigma1, sigma2, sigma3, desigma;
  BlurPixel<double> *pix1, *pix2, *pix3, *pix4;

  BLUR_CODE(0.0, float)
}
//-------------------------------------------------------------------

template <class T>
void do_filtering_channel_float(T *row1, float *row2, int length, float coeff,
                                float coeffq, int brad, float diff) {
  int i;
  float sum;
  float sigma1, sigma2, sigma3, desigma;
  T *pix1, *pix2, *pix3, *pix4;

  pix1 = row1;
  pix2 = row1 - 1;

  sigma1 = pix1->value;
  pix1++;

  sigma2 = 0.0;
  sigma3 = 0.0;

  for (i = 1; i < brad; i++) {
    sigma1 += pix1->value;
    sigma2 += pix2->value;
    sigma3 += i * (pix1->value + pix2->value);
    pix1++;
    pix2--;
  }

  sum = (sigma1 + sigma2) * coeff - sigma3 * coeffq;

  *row2 = sum;
  row2++;

  sigma2 += row1[-brad].value;

  pix1 = row1 + brad;
  pix2 = row1;
  pix3 = row1 - brad;
  pix4 = row1 - brad + 1;

  desigma = sigma1 - sigma2;

  for (i = 1; i < length; i++) {
    desigma += pix1->value - 2 * pix2->value + pix3->value;

    sum += (desigma + diff * (pix1->value - pix4->value)) * coeffq;

    *row2 = sum;
    row2++;
    pix1++, pix2++, pix3++, pix4++;
  }
}

//-------------------------------------------------------------------
template <class T>
void do_filtering_channel_gray(float *row1, T *row2, int length, float coeff,
                               float coeffq, int brad, float diff) {
  int i;
  float sum;
  float sigma1, sigma2, sigma3, desigma;
  float *pix1, *pix2, *pix3, *pix4;

  pix1 = row1;
  pix2 = row1 - 1;

  sigma1 = *pix1;
  pix1++;

  sigma2 = 0.0;
  sigma3 = 0.0;

  for (i = 1; i < brad; i++) {
    sigma1 += *pix1;
    sigma2 += *pix2;
    sigma3 += i * (*pix1 + *pix2);
    pix1++;
    pix2--;
  }

  sum = (sigma1 + sigma2) * coeff - sigma3 * coeffq + 0.5F;

  row2->setValue((int)sum);
  row2++;

  sigma2 += row1[-brad];

  pix1 = row1 + brad;
  pix2 = row1;
  pix3 = row1 - brad;
  pix4 = row1 - brad + 1;

  desigma = sigma1 - sigma2;

  for (i = 1; i < length; i++) {
    desigma += *pix1 - 2 * (*pix2) + (*pix3);

    sum += (desigma + diff * (*pix1 - *pix4)) * coeffq;

    row2->setValue((int)sum);
    row2++;
    pix1++, pix2++, pix3++, pix4++;
  }
}

//-------------------------------------------------------------------
template <class T>
void load_rowRgb(TRasterPT<T> &rin, T *row, int lx, int y, int brad, int bx1,
                 int bx2) {
  int i;
  T *buf32, *pix;
  T left_val, right_val;

  pix = row + bx1;

  {
    rin->lock();
    buf32 = rin->pixels(y);

    for (i = 0; i < lx; i++) *pix++ = *buf32++;
    rin->unlock();
  }

  pix += bx2;
  left_val  = *row;
  right_val = *(pix - 1);
  row--;

  for (i = 0; i < brad;
       i++) /* pixels equal to the ones of border of image are added   */
  {         /* to avoid a black blur to get into the picture.          */
    *row-- = left_val;
    *pix++ = right_val;
  }
}

//-------------------------------------------------------------------
template <class T>
void load_rowGray(TRasterPT<T> &rin, T *row, int lx, int y, int brad, int bx1,
                  int bx2) {
  int i;
  T *buf8, *pix;
  T left_val, right_val;

  pix  = row + bx1;
  buf8 = (T *)(rin->pixels(y));

  for (i = 0; i < lx; i++) *pix++ = *buf8++;

  pix += bx2;
  left_val  = *row;
  right_val = *(pix - 1);
  row--;

  for (i = 0; i < brad;
       i++) /* pixels equal to the ones of border of image are added   */
  {         /* to avoid a black blur to get into the picture.          */
    *row-- = left_val;
    *pix++ = right_val;
  }
}

//-------------------------------------------------------------------
template <class T, class P>
void do_filtering_floatRgb(T *row1, BlurPixel<P> *row2, int length, float coeff,
                           float coeffq, int brad, float diff, bool useSSE) {
  /*
    int i;
    float rsum, gsum, bsum,  msum;
    CASM_FPIXEL sigma1, sigma2, sigma3, desigma;
    TPixel32 *pix1, *pix2, *pix3, *pix4;

    BLUR_CODE(0, unsigned char)
  */

#ifdef USE_SSE2
  if (useSSE)
    blur_code_SSE2<T, P>(row1, row2, length, coeff, coeffq, brad, diff, 0);
  else
#endif
    blur_code<T, BlurPixel<P>, P>(row1, row2, length, coeff, coeffq, brad, diff,
                                  0);
}

//-------------------------------------------------------------------
template <class T, class Q, class P>
void doBlurRgb(TRasterPT<T> &dstRas, TRasterPT<T> &srcRas, double blur, int dx,
               int dy, bool useSSE) {
  int i, lx, ly, llx, lly, brad;
  float coeff, coeffq, diff;
  int bx1 = 0, by1 = 0, bx2 = 0, by2 = 0;

  brad = (int)ceil(blur); /* number of pixels involved in the filtering */

  // int border = brad*2; // per sicurezza

  coeff  = (float)(blur /
                  (brad - brad * brad +
                   blur * (2 * brad -
                           1))); /*sum of the weights of triangolar filter. */
  coeffq = (float)(coeff / blur);
  diff   = (float)(blur - brad);
  lx     = srcRas->getLx();
  ly     = srcRas->getLy();

  if ((lx == 0) || (ly == 0)) return;

  llx = lx + bx1 + bx2;
  lly = ly + by1 + by2;

  T *row1, *col2, *buffer;
  BlurPixel<P> *row2, *col1, *fbuffer;
  TRasterGR8P r1;

#ifdef _WIN32
  if (useSSE) {
    fbuffer =
        (BlurPixel<P> *)_aligned_malloc(llx * ly * sizeof(BlurPixel<P>), 16);
    row1 = (T *)_aligned_malloc((llx + 2 * brad) * sizeof(T), 16);
    col1 = (BlurPixel<P> *)_aligned_malloc(
        (lly + 2 * brad) * sizeof(BlurPixel<P>), 16);
    col2 = (T *)_aligned_malloc(lly * sizeof(T), 16);
  } else
#endif
  {
    TRasterGR8P raux(llx * sizeof(BlurPixel<P>), ly);
    r1 = raux;
    r1->lock();
    fbuffer = (BlurPixel<P> *)r1->getRawData();  // new CASM_FPIXEL [llx *ly];
    row1    = new T[llx + 2 * brad];
    col1    = new BlurPixel<P>[lly + 2 * brad];
    col2    = new T[lly];
  }

  if ((!fbuffer) || (!row1) || (!col1) || (!col2)) {
    if (!useSSE) r1->unlock();
#ifdef _WIN32
    if (useSSE) {
      _aligned_free(col2);
      _aligned_free(col1);
      _aligned_free(row1);
      _aligned_free(fbuffer);
    } else
#endif
    {
      delete[] col2;
      delete[] col1;
      delete[] row1;
    }
    return;
  }

  row2 = fbuffer;

  try {
    for (i = 0; i < ly; i++) {
      load_rowRgb<T>(srcRas, row1 + brad, lx, i, brad, bx1, bx2);
      do_filtering_floatRgb<T>(row1 + brad, row2, llx, coeff, coeffq, brad,
                               diff, useSSE);
      row2 += llx;
    }
    dstRas->lock();
    buffer = (T *)dstRas->getRawData();

    if (dy >= 0) buffer += (dstRas->getWrap()) * dy;

    for (i = (dx >= 0) ? 0 : -dx; i < std::min(llx, dstRas->getLx() - dx);
         i++) {
      load_colRgb<P>(fbuffer, col1 + brad, llx, ly, i, brad, by1, by2);
      do_filtering_chan<T, Q, P>(col1 + brad, col2, lly, coeff, coeffq, brad,
                                 diff, useSSE);
      store_colRgb<T>(buffer, dstRas->getWrap(), dstRas->getLy(), col2, lly,
                      i + dx, dy, 0, blur);
    }
    dstRas->unlock();
  } catch (...) {
    dstRas->clear();
  }

#ifdef _WIN32
  if (useSSE) {
    _aligned_free(col2);
    _aligned_free(col1);
    _aligned_free(row1);
    _aligned_free(fbuffer);
  } else
#endif
  {
    delete[] col2;
    delete[] col1;
    delete[] row1;
    r1->unlock();
  }
}

//-------------------------------------------------------------------

template <class T>
void doBlurGray(TRasterPT<T> &dstRas, TRasterPT<T> &srcRas, double blur, int dx,
                int dy) {
  int i, lx, ly, llx, lly, brad;
  float coeff, coeffq, diff;
  int bx1 = 0, by1 = 0, bx2 = 0, by2 = 0;

  brad   = (int)ceil(blur); /* number of pixels involved in the filtering */
  coeff  = (float)(blur /
                  (brad - brad * brad +
                   blur * (2 * brad -
                           1))); /*sum of the weights of triangolar filter. */
  coeffq = (float)(coeff / blur);
  diff   = (float)(blur - brad);
  lx     = srcRas->getLx();
  ly     = srcRas->getLy();

  if ((lx == 0) || (ly == 0)) return;

  llx = lx + bx1 + bx2;
  lly = ly + by1 + by2;

  T *row1, *col2, *buffer;
  float *row2, *col1, *fbuffer;

  TRasterGR8P r1(llx * sizeof(float), ly);
  r1->lock();
  fbuffer = (float *)r1->getRawData();  // new float[llx *ly];

  row1 = new T[llx + 2 * brad];
  col1 = new float[lly + 2 * brad];
  col2 = new T[lly];

  if ((!fbuffer) || (!row1) || (!col1) || (!col2)) {
    delete[] row1;
    delete[] col1;
    delete[] col2;
    return;
  }

  row2 = fbuffer;

  for (i = 0; i < ly; i++) {
    load_rowGray<T>(srcRas, row1 + brad, lx, i, brad, bx1, bx2);
    do_filtering_channel_float<T>(row1 + brad, row2, llx, coeff, coeffq, brad,
                                  diff);
    row2 += llx;
  }
  dstRas->lock();
  buffer = (T *)dstRas->getRawData();

  if (dy >= 0) buffer += (dstRas->getWrap()) * dy;

  for (i = (dx >= 0) ? 0 : -dx; i < std::min(llx, dstRas->getLx() - dx); i++) {
    load_channel_col32(fbuffer, col1 + brad, llx, ly, i, brad, by1, by2);
    do_filtering_channel_gray<T>(col1 + brad, col2, lly, coeff, coeffq, brad,
                                 diff);

    int backlit = 0;
    store_colGray<T>(buffer, dstRas->getWrap(), dstRas->getLy(), col2, lly,
                     i + dx, dy, backlit, blur);
  }
  dstRas->unlock();
  delete[] col2;
  delete[] col1;
  delete[] row1;
  r1->unlock();  // delete[]fbuffer;
}

};  // namespace

//====================================================================

int TRop::getBlurBorder(double blur) {
  int brad = (int)ceil(blur); /* number of pixels involved in the filtering */

  int border = brad * 2;  // per sicurezza
  return border;
}

//--------------------------------------------------------------------

void TRop::blur(const TRasterP &dstRas, const TRasterP &srcRas, double blur,
                int dx, int dy, bool useSSE) {
  TRaster32P dstRas32 = dstRas;
  TRaster32P srcRas32 = srcRas;
  if (dstRas32 && srcRas32) {
    doBlurRgb<TPixel32, UCHAR, float>(dstRas32, srcRas32, blur, dx, dy, useSSE);
    return;
  }

  TRaster64P dstRas64 = dstRas;
  TRaster64P srcRas64 = srcRas;
  if (dstRas64 && srcRas64) {
    doBlurRgb<TPixel64, USHORT, double>(dstRas64, srcRas64, blur, dx, dy,
                                        useSSE);
    return;
  }

  TRasterFP dstRasF = dstRas;
  TRasterFP srcRasF = srcRas;
  if (dstRasF && srcRasF) {
    doBlurRgb<TPixelF, float, double>(dstRasF, srcRasF, blur, dx, dy, useSSE);
    return;
  }

  TRasterGR8P dstRasGR8 = dstRas;
  TRasterGR8P srcRasGR8 = srcRas;
  if (dstRasGR8 && srcRasGR8) {
    doBlurGray<TPixelGR8>(dstRasGR8, srcRasGR8, blur, dx, dy);
    return;
  }

  TRasterGR16P dstRasGR16 = dstRas;
  TRasterGR16P srcRasGR16 = srcRas;
  if (dstRasGR16 && srcRasGR16) {
    doBlurGray<TPixelGR16>(dstRasGR16, srcRasGR16, blur, dx, dy);
    return;
  }

  throw TException("TRop::blur unsupported pixel type");
}
