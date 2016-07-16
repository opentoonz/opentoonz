#if defined(_MSC_VER) && (_MSC_VER >= 1400)
#define _CRT_SECURE_NO_DEPRECATE 1
#endif

#include "zlib.h"

#include "psdutils.h"

void readrow(FILE *psd, TPSDChannelInfo *chan,
             psdPixel row,             // row index
             unsigned char *inbuffer,  // dest buffer for the uncompressed row
             unsigned char *
                 tmpbuffer)  // temp rlebuffer for compressed data, 2xrb in size
{
  psdPixel n = 0, rlebytes;
  psdByte pos;

  int seekres = 0;

  switch (chan->comptype) {
  case RAWDATA: /* uncompressed */
    pos                  = chan->filepos + chan->rowbytes * row;
    seekres              = fseek(psd, pos, SEEK_SET);
    if (seekres != -1) n = fread(inbuffer, 1, chan->rowbytes, psd);
    break;
  case RLECOMP:
    pos     = chan->rowpos[row];
    seekres = fseek(psd, pos, SEEK_SET);
    if (seekres != -1) {
      rlebytes = fread(tmpbuffer, 1, chan->rowpos[row + 1] - pos, psd);
      n        = unpackrow(inbuffer, tmpbuffer, chan->rowbytes, rlebytes);
    }
    break;
  case ZIPWITHPREDICTION:
  case ZIPWITHOUTPREDICTION:
    memcpy(inbuffer, chan->unzipdata + chan->rowbytes * row, chan->rowbytes);
    return;
  }
  // if we don't recognise the compression type, skip the row
  // FIXME: or would it be better to use the last valid type seen?

  //	if(seekres == -1)
  //	alwayswarn("# can't seek to " LL_L("%lld\n","%ld\n"), pos);

  if (n < chan->rowbytes) {
    // warn("row data short (wanted %d, got %d bytes)", chan->rowbytes, n);
    // zero out unwritten part of row
    memset(inbuffer + n, 0, chan->rowbytes - n);
  }
}

int unpackrow(unsigned char *out, unsigned char *in, psdPixel outlen,
              psdPixel inlen) {
  psdPixel i, len;
  int val;

  for (i = 0; inlen > 1 && i < outlen;) {
    len = *in++;
    --inlen;

    if (len == 128) /* ignore RLE flag value */
      ;
    else {
      if (len > 128) {
        len = 1 + 256 - len;

        val = *in++;
        --inlen;

        if ((i + len) <= outlen)
          memset(out, val, len);
        else {
          memset(out, val, outlen - i);  // fill to complete row
          len = 0;
        }
      } else {
        ++len;
        if ((i + len) <= outlen) {
          if (len > inlen) break;
          memcpy(out, in, len);
          in += len;
          inlen -= len;
        } else {
          memcpy(out, in, outlen - i);  // copy to complete row
          len = 0;
        }
      }
      out += len;
      i += len;
    }
  }
  if (i < outlen) {
    // WARNING: not enough RLE data for row;
  }
  return i;
}

void skipBlock(FILE *f) {
  psdByte n = read4Bytes(f);
  if (n) {
    fseek(f, n, SEEK_CUR);
  } else {
  }
}

// Read 2byte unsigned binary value.
// BigEndian.
unsigned read2UBytes(FILE *f) {
  unsigned n = fgetc(f) << 8;
  return n |= fgetc(f);
}
// Read 2byte binary value
// BigEndian.
int read2Bytes(FILE *f) {
  unsigned n = fgetc(f) << 8;
  n |= fgetc(f);
  return n < 0x8000 ? n : n - 0x10000;
}

// Read 4byte signed binary value.
// BigEndian.
long read4Bytes(FILE *f) {
  long n = fgetc(f) << 24;
  n |= fgetc(f) << 16;
  n |= fgetc(f) << 8;
  return n | fgetc(f);
}

void *mymalloc(long n) {
  void *p = malloc(n);
  if (p)
    return p;
  else {
    // ALLOCATION ERROR
  }
  return NULL;
}

// ZIP COMPRESSION

// ZIP WITHOUT PREDICTION
// If no error returns 1 else returns 0
int psdUnzipWithoutPrediction(unsigned char *src_buf, int src_len,
                              unsigned char *dst_buf, int dst_len) {
  z_stream stream;
  int state;

  memset(&stream, 0, sizeof(z_stream));
  stream.data_type = Z_BINARY;

  stream.next_in   = (Bytef *)src_buf;
  stream.avail_in  = src_len;
  stream.next_out  = (Bytef *)dst_buf;
  stream.avail_out = dst_len;

  if (inflateInit(&stream) != Z_OK) return 0;

  do {
    state = inflate(&stream, Z_PARTIAL_FLUSH);
    if (state == Z_STREAM_END) break;
    if (state == Z_DATA_ERROR || state != Z_OK) break;
  } while (stream.avail_out > 0);

  if (state != Z_STREAM_END && state != Z_OK) return 0;

  return 1;
}

// ZIP WITH PREDICTION

int psdUnzipWithPrediction(unsigned char *src_buf, int src_len,
                           unsigned char *dst_buf, int dst_len, int row_size,
                           int color_depth) {
  int status;
  int len;
  unsigned char *buf;

  status = psdUnzipWithoutPrediction(src_buf, src_len, dst_buf, dst_len);
  if (!status) return status;

  buf = dst_buf;
  do {
    len = row_size;
    if (color_depth == 16) {
      while (--len) {
        buf[2] += buf[0] + ((buf[1] + buf[3]) >> 8);
        buf[3] += buf[1];
        buf += 2;
      }
      buf += 2;
      dst_len -= row_size * 2;
    } else {
      while (--len) {
        *(buf + 1) += *buf;
        buf++;
      }
      buf++;
      dst_len -= row_size;
    }
  } while (dst_len > 0);

  return 1;
}
