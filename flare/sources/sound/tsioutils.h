#pragma once

#ifndef TSIOUTILS_INCLUDED
#define TSIOUTILS_INCLUDED

void swapAndCopy16Bits(const short *srcBuffer, short *dstBuffer,
                        TINT32 sampleCount);

void swapAndCopy32Bits(const TINT32 *srcBuffer, TINT32 *dstBuffer,
                          TINT32 sampleCount);

void swapAndCopy24Bits(const void *srcBuffer, void *dstBuffer,
                       TINT32 sampleCount);

#endif
