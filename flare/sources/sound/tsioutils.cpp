

#include "tcommon.h"

#include "tsioutils.h"

//------------------------------------------------------------------------------
void swapAndCopy16Bits(const short *srcBuffer, short *dstBuffer,
                        TINT32 sampleCount) {
  const short *srcSample = srcBuffer;
  short *dstSample = dstBuffer;

  const short *endSrcSample = srcSample + sampleCount;
  while (srcSample < endSrcSample) *dstSample++ = swapShort(*srcSample++);
}

//------------------------------------------------------------------------------
void swapAndCopy32Bits(const TINT32 *srcBuffer, TINT32 *dstBuffer,
                        TINT32 sampleCount) {
  const TINT32 *srcSample = srcBuffer;
  TINT32 *dstSample = dstBuffer;

  const TINT32 *endSrcSample = srcSample + sampleCount;
  while (srcSample < endSrcSample) *dstSample++ = swapTINT32(*srcSample++);
}

//------------------------------------------------------------------------------
void swapAndCopy24Bits(const void *srcBuffer, void *dstBuffer,
                              TINT32 sampleCount) {
  const UCHAR *srcData = (const UCHAR *)srcBuffer;
  UCHAR *dstData       = (UCHAR *)dstBuffer;
  if (sampleCount <= 0) return;
  while (--sampleCount) {
    dstData[0] = srcData[2];
    dstData[1] = srcData[1];
    dstData[2] = srcData[0];
    srcData += 3;
    dstData += 3;
  }
}
