

#include "tcommon.h"

#include "tsioutils.h"

//------------------------------------------------------------------------------
void swapAndCopySamples(short *srcBuffer, short *dstBuffer,
                        TINT32 sampleCount) {
  short *srcSample = srcBuffer;
  short *dstSample = dstBuffer;

  short *endSrcSample                           = srcSample + sampleCount;
  while (srcSample < endSrcSample) *dstSample++ = swapShort(*srcSample++);
}
