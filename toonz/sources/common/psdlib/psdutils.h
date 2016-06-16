#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BLOCKROWS 3

#define NEXT2(x) (((x) + 1) & -2)  // uguale o successivo (pad)
#define NEXT4(x) (((x) + 3) & -4)  // uguale o successivo multiplo di 4	(pad)
#define FIXDPI(x) ((x) / 65536.)   // fixedpt

typedef long psdByte;
typedef unsigned short psdUint16;
typedef long psdPixel;

enum { RAWDATA, RLECOMP, ZIPWITHOUTPREDICTION, ZIPWITHPREDICTION };

struct TPSDChannelInfo {
  int id;                         // channel id
  int comptype;                   // channel's compression type
  psdPixel rows, cols, rowbytes;  // computed by dochannel()
  // FIXME: need depth??
  psdByte length;   // channel byte count in file
  psdByte filepos;  // file offset of channel data (AFTER compression type)
  psdByte *rowpos;  // row data file positions (RLE ONLY)
  unsigned char *unzipdata;  // uncompressed data (ZIP ONLY)
};

int unpackrow(unsigned char *out, unsigned char *in, psdPixel outlen,
              psdPixel inlen);

void readrow(FILE *psd, TPSDChannelInfo *chan, psdPixel rowIndex,
             unsigned char *inbuffer, unsigned char *outbuffer);

void skipBlock(FILE *f);

void *mymalloc(long n);
unsigned read2UBytes(FILE *f);
int read2Bytes(FILE *f);
long read4Bytes(FILE *f);

int psdUnzipWithoutPrediction(unsigned char *src_buf, int src_len,
                              unsigned char *dst_buf, int dst_len);
int psdUnzipWithPrediction(unsigned char *src_buf, int src_len,
                           unsigned char *dst_buf, int dst_len, int row_size,
                           int color_depth);
