

#if defined(_MSC_VER) && (_MSC_VER >= 1400)
#define _CRT_SECURE_NO_DEPRECATE 1
#pragma warning(disable : 4996)
#endif

#include "texception.h"
#include "tpixel.h"
#include "tiio_sgi.h"
#include "tsystem.h"
#include "tpixelgr.h"
#include "../compatibility/tfile_io.h"
#if defined(LINUX) || defined(FREEBSD)
// #define _XOPEN_SOURCE_EXTENDED
#include <string.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#if defined(MACOSX)
#include <sys/malloc.h>
#endif

#include <assert.h>
#include <memory>

#ifdef _WIN32
#include <io.h>
#include <windows.h>
#else
#ifndef LINUX
#include <strings.h>
#endif
#include <unistd.h>
#endif
#include <stdarg.h>

// IMAGERGB: SGI image header structure
struct IMAGERGB {
  unsigned short imagic;  // Magic number
  unsigned short type;    // Image type
  unsigned short dim;     // Image dimensions
  unsigned short xsize;   // Width
  unsigned short ysize;   // Height
  unsigned short zsize;   // Depth (channels)
  TUINT32 min;            // Minimum pixel value
  TUINT32 max;            // Maximum pixel value
  TUINT32 wastebytes;     // Unused bytes
  char name[80];          // Image name
  TUINT32 colormap;       // Colormap type

  TINT32 file;             // File descriptor (in core only)
  unsigned short flags;    // File flags
  short dorev;             // Byte swapping needed
  short x;                 // Current x position
  short y;                 // Current y position
  short z;                 // Current z position
  short cnt;               // Count
  unsigned short *ptr;     // Current pointer
  unsigned short *base;    // Base pointer
  unsigned short *tmpbuf;  // Temporary buffer
  TUINT32 offset;          // File offset
  TUINT32 rleend;          // For RLE images
  TUINT32 *rowstart;       // For RLE images
  TINT32 *rowsize;         // For RLE images
};

const int IMAGERGB_HEADER_SIZE = sizeof(IMAGERGB);

// Macros defined in the old ImageP/filergb.h
#define IMAGIC 0732
#define TYPEMASK 0xff00
#define ITYPE_RLE 0x0100
#define ISRLE(type) (((type) & 0xff00) == ITYPE_RLE)
#define ITYPE_VERBATIM 0x0000
#define ISVERBATIM(type) (((type) & 0xff00) == ITYPE_VERBATIM)
#define BPPMASK 0x00ff
#define BPP(type) ((type) & BPPMASK)
#define RLE(bpp) (ITYPE_RLE | (bpp))
#define VERBATIM(bpp) (ITYPE_VERBATIM | (bpp))
#define IBUFSIZE(pixels) ((pixels + (pixels >> 6)) << 2)
#define RLE_NOP 0x00

#ifndef _IORW
#define _IOREAD 0x1
#define _IOWRT 0x2
#define _IORW 0x80
#define _IOERR 0x20
#endif

// Forward declarations
static USHORT *ibufalloc(IMAGERGB *image, int bpp);
static void cvtshorts(USHORT buffer[], TINT32 n);
static void cvtTINT32s(TUINT32 *buffer, TINT32 n);
static void cvtimage(IMAGERGB *image);
static void img_rle_expand(USHORT *rlebuf, int ibpp, USHORT *expbuf, int obpp);
static int img_getrowsize(IMAGERGB *image);
static TUINT32 img_optseek(IMAGERGB *image, TUINT32 offset);
static TINT32 rgb_img_read(IMAGERGB *image, char *buffer, TINT32 count);
static int img_badrow(IMAGERGB *image, int y, int z);
static TUINT32 img_seek(IMAGERGB *image, UINT y, UINT z, UINT offs);
static TINT32 RGB_img_write(IMAGERGB *image, char *buffer, TINT32 count);
static void img_setrowsize(IMAGERGB *image, UINT cnt, UINT y, UINT z);
static TINT32 img_rle_compact(USHORT *expbuf, int ibpp, USHORT *rlebuf,
                              int obpp, int cnt);
static int iflush(IMAGERGB *image);

/*-------------------------------------------------------------------------*/

static int do_rgb_write_header(IMAGERGB *img, int fd) {
  // Do we need to swap bytes?
  int count = 0;
  count += write(fd, &img->imagic, (int)sizeof(unsigned short));
  count += write(fd, &img->type, (int)sizeof(unsigned short));
  count += write(fd, &img->dim, (int)sizeof(unsigned short));
  count += write(fd, &img->xsize, (int)sizeof(unsigned short));
  count += write(fd, &img->ysize, (int)sizeof(unsigned short));
  count += write(fd, &img->zsize, (int)sizeof(unsigned short));
  count += write(fd, &img->min, (int)sizeof(TUINT32));
  count += write(fd, &img->max, (int)sizeof(TUINT32));
  count += write(fd, &img->wastebytes, (int)sizeof(TUINT32));
  count += write(fd, img->name, (int)sizeof(img->name));
  count += write(fd, &img->colormap, (int)sizeof(TUINT32));

  count += write(fd, &img->file, (int)sizeof(TINT32));
  count += write(fd, &img->flags, (int)sizeof(unsigned short));
  count += write(fd, &img->dorev, (int)sizeof(short));
  count += write(fd, &img->x, (int)sizeof(short));
  count += write(fd, &img->y, (int)sizeof(short));
  count += write(fd, &img->z, (int)sizeof(short));
  count += write(fd, &img->cnt, (int)sizeof(short));
  count += write(fd, &img->ptr, (int)sizeof(unsigned short *));
  count += write(fd, &img->base, (int)sizeof(unsigned short *));
  count += write(fd, &img->tmpbuf, (int)sizeof(unsigned short *));
  count += write(fd, &img->offset, (int)sizeof(TUINT32));
  count += write(fd, &img->rleend, (int)sizeof(TUINT32));
  count += write(fd, &img->rowstart, (int)sizeof(TUINT32 *));
  count += write(fd, &img->rowsize, (int)sizeof(TINT32 *));
  if (sizeof(void *) == 8)  // 64-bit: header has padding bytes
    count = (count + 0x7) & (~0x7);
  return count;
}

/*-------------------------------------------------------------------------*/

static int do_rgb_read_header(IMAGERGB *img, int fd) {
  // Do we need to swap bytes?
  int count = 0;
  count += read(fd, &img->imagic, sizeof(unsigned short));
  count += read(fd, &img->type, sizeof(unsigned short));
  count += read(fd, &img->dim, sizeof(unsigned short));
  count += read(fd, &img->xsize, sizeof(unsigned short));
  count += read(fd, &img->ysize, sizeof(unsigned short));
  count += read(fd, &img->zsize, sizeof(unsigned short));
  count += read(fd, &img->min, sizeof(TUINT32));
  count += read(fd, &img->max, sizeof(TUINT32));
  count += read(fd, &img->wastebytes, sizeof(TUINT32));
  count += read(fd, img->name, sizeof(img->name));
  count += read(fd, &img->colormap, sizeof(TUINT32));

  count += read(fd, &img->file, sizeof(TINT32));
  count += read(fd, &img->flags, sizeof(unsigned short));
  count += read(fd, &img->dorev, sizeof(short));
  count += read(fd, &img->x, sizeof(short));
  count += read(fd, &img->y, sizeof(short));
  count += read(fd, &img->z, sizeof(short));
  count += read(fd, &img->cnt, sizeof(short));
  count += read(fd, &img->ptr, sizeof(unsigned short *));
  count += read(fd, &img->base, sizeof(unsigned short *));
  count += read(fd, &img->tmpbuf, sizeof(unsigned short *));
  count += read(fd, &img->offset, sizeof(TUINT32));
  count += read(fd, &img->rleend, sizeof(TUINT32));
  count += read(fd, &img->rowstart, sizeof(TUINT32 *));
  count += read(fd, &img->rowsize, sizeof(TINT32 *));
  if (sizeof(void *) == 8)  // 64-bit: header has padding bytes
    count = (count + 0x7) & (~0x7);
  return count;
}

/*-------------------------------------------------------------------------*/
enum OpenMode { OpenRead, OpenWrite };

/*-------------------------------------------------------------------------*/

static IMAGERGB *iopen(int fd, OpenMode openMode, unsigned int type,
                       unsigned int dim, unsigned int xsize, unsigned int ysize,
                       unsigned int zsize, short dorev) {
  IMAGERGB *image = nullptr;
  int tablesize;
  int f = fd;

  image = (IMAGERGB *)malloc((int)sizeof(IMAGERGB));
  if (!image) {
    return nullptr;
  }

  memset(image, 0, sizeof(IMAGERGB));

  if (openMode == OpenWrite) {
    // WRITE mode
    image->imagic = IMAGIC;
    image->type   = type;
    image->xsize  = xsize;
    image->ysize  = 1;
    image->zsize  = 1;

    if (dim > 1) image->ysize = ysize;
    if (dim > 2) image->zsize = zsize;

    if (image->zsize == 1) {
      image->dim = 2;
      if (image->ysize == 1) image->dim = 1;
    } else {
      image->dim = 3;
    }

    image->min = 10000000;
    image->max = 0;
    strncpy(image->name, "no name", 80);
    image->name[79]   = '\0';  // Ensure null termination
    image->wastebytes = 0;
    image->dorev      = dorev;
    if (do_rgb_write_header(image, f) != IMAGERGB_HEADER_SIZE) {
      std::cout << "iopen: error on write of image header\n" << std::endl;
      free(image);
      return nullptr;
    }
    image->flags = _IOWRT;
  } else {
    // READ mode
    if (do_rgb_read_header(image, f) != IMAGERGB_HEADER_SIZE) {
      std::cout << "iopen: error on read of image header" << std::endl;
      free(image);
      return nullptr;
    }

    if (((image->imagic >> 8) | ((image->imagic & 0xff) << 8)) == IMAGIC) {
      image->dorev = 1;
      cvtimage(image);
    } else {
      image->dorev = 0;
    }

    if (image->imagic != IMAGIC) {
      std::cout << "iopen: bad magic in image file " << image->imagic
                << std::endl;
      free(image);
      return nullptr;
    }
    image->flags = _IOREAD;
  }

  if (ISRLE(image->type)) {
    tablesize       = image->ysize * image->zsize * (int)sizeof(TINT32);
    image->rowstart = (TUINT32 *)malloc(tablesize);
    image->rowsize  = (TINT32 *)malloc(tablesize);

    if (image->rowstart == nullptr || image->rowsize == nullptr) {
      std::cout << "iopen: error on table alloc" << std::endl;
      free(image->rowstart);
      free(image->rowsize);
      free(image);
      return nullptr;
    }

    image->rleend = 512L + 2 * tablesize;

    if (openMode == OpenWrite) {
      // WRITE mode - initialize tables
      int max = image->ysize * image->zsize;
      for (int i = 0; i < max; i++) {
        image->rowstart[i] = 0;
        image->rowsize[i]  = -1;
      }
    } else {
      // READ mode - load tables
      tablesize = image->ysize * image->zsize * (int)sizeof(TINT32);
      lseek(f, 512L, 0);
      if (read(f, image->rowstart, tablesize) != tablesize) {
        TSystem::outputDebug("iopen: error on read of rowstart\n");
        free(image->rowstart);
        free(image->rowsize);
        free(image);
        return nullptr;
      }

      if (image->dorev) cvtTINT32s(image->rowstart, tablesize);
      if (read(f, image->rowsize, tablesize) != tablesize) {
        TSystem::outputDebug("iopen: error on read of rowsize\n");
        free(image->rowstart);
        free(image->rowsize);
        free(image);
        return nullptr;
      }

      if (image->dorev) cvtTINT32s((TUINT32 *)image->rowsize, tablesize);
    }
  }

  image->cnt  = 0;
  image->ptr  = nullptr;
  image->base = nullptr;

  if ((image->tmpbuf = ibufalloc(image, BPP(image->type))) == nullptr) {
    char xs[1024];
    snprintf(xs, sizeof(xs), "%d", image->xsize);
    TSystem::outputDebug(std::string("iopen: error on tmpbuf alloc %d\n") + xs);
    if (ISRLE(image->type)) {
      free(image->rowstart);
      free(image->rowsize);
    }
    free(image);
    return nullptr;
  }

  image->x = image->y = image->z = 0;
  image->file                    = fd;
  image->offset                  = 512L;  // Set up for img_optseek
  lseek((int)image->file, 512L, 0);

  return image;
}

/*-------------------------------------------------------------------------*/

static USHORT *ibufalloc(IMAGERGB *image, int bpp) {
  size_t size = IBUFSIZE(image->xsize) * bpp;
  if (size == 0) return nullptr;
  return (USHORT *)malloc(size);
}

/*-----------------------------------------------------------------------------
 * Swap bytes in short buffer
 *----------------------------------------------------------------------------*/
static void cvtshorts(unsigned short buffer[], TINT32 n) {
  TINT32 nshorts = n >> 1;
  for (int i = 0; i < nshorts; i++) {
    unsigned short swrd = *buffer;
    *buffer++           = (swrd >> 8) | (swrd << 8);
  }
}

/*-----------------------------------------------------------------------------
 * Swap bytes in long buffer
 *----------------------------------------------------------------------------*/
static void cvtTINT32s(TUINT32 buffer[], TINT32 n) {
  TINT32 nTINT32s = n >> 2;
  for (int i = 0; i < nTINT32s; i++) {
    TUINT32 lwrd = buffer[i];
    buffer[i]    = ((lwrd >> 24) | ((lwrd >> 8) & 0xff00) |
                 ((lwrd << 8) & 0xff0000) | (lwrd << 24));
  }
}

/*-----------------------------------------------------------------------------
 * Swap bytes in image header
 *----------------------------------------------------------------------------*/
static void cvtimage(IMAGERGB *image) {
  TUINT32 *buffer = (TUINT32 *)image;
  cvtshorts((unsigned short *)buffer, 12);
  cvtTINT32s(buffer + 3, 12);
  cvtTINT32s(buffer + 26, 4);
}

/*-----------------------------------------------------------------------------*/

#define EXPAND_CODE(TYPE)                                                      \
  while (1) {                                                                  \
    pixel = *iptr++;                                                           \
    if (!(count = (pixel & 0x7f))) return;                                     \
    if (pixel & 0x80) {                                                        \
      while (count--) *optr++ = (TYPE) * iptr++;                               \
    } else {                                                                   \
      pixel = *iptr++;                                                         \
      while (count--) *optr++ = (TYPE)pixel;                                   \
    }                                                                          \
  }

/*-----------------------------------------------------------------------------
 * Expand RLE-compressed RGB image
 *----------------------------------------------------------------------------*/
static void img_rle_expand(unsigned short *rlebuf, int ibpp,
                           unsigned short *expbuf, int obpp) {
  if (ibpp == 1 && obpp == 1) {
    unsigned char *iptr = (unsigned char *)rlebuf;
    unsigned char *optr = (unsigned char *)expbuf;
    unsigned short pixel, count;

    EXPAND_CODE(unsigned char);
  } else if (ibpp == 1 && obpp == 2) {
    unsigned char *iptr  = (unsigned char *)rlebuf;
    unsigned short *optr = expbuf;
    unsigned short pixel, count;

    EXPAND_CODE(unsigned short);
  } else if (ibpp == 2 && obpp == 1) {
    unsigned short *iptr = rlebuf;
    unsigned char *optr  = (unsigned char *)expbuf;
    unsigned short pixel, count;

    EXPAND_CODE(unsigned char);
  } else if (ibpp == 2 && obpp == 2) {
    unsigned short *iptr = rlebuf;
    unsigned short *optr = expbuf;
    unsigned short pixel, count;

    EXPAND_CODE(unsigned short);
  } else {
    std::cout << "rle_expand: bad bpp: " << ibpp << " " << obpp << std::endl;
  }
}

/*-----------------------------------------------------------------------------
 * Get row size of RGB image
 *----------------------------------------------------------------------------*/
static int img_getrowsize(IMAGERGB *image) {
  switch (image->dim) {
  case 1:
    return (int)image->rowsize[0];
  case 2:
    return (int)image->rowsize[image->y];
  case 3:
    return (int)image->rowsize[image->y + image->z * image->ysize];
  }
  return 0;
}

/*-----------------------------------------------------------------------------
 * Seek to position in RGB file
 *----------------------------------------------------------------------------*/
static TUINT32 img_optseek(IMAGERGB *image, TUINT32 offset) {
  if (image->offset != offset) {
    image->offset = offset;
    return (TUINT32)lseek(image->file, (TINT32)offset, 0);
  }
  return offset;
}

/*-----------------------------------------------------------------------------
 * Read from RGB file into buffer
 *----------------------------------------------------------------------------*/
static TINT32 rgb_img_read(IMAGERGB *image, char *buffer, TINT32 count) {
  TINT32 retval;

  retval = read(image->file, buffer, count);
  if (retval == count) {
    image->offset += count;
  } else {
    // Error: set offset to invalid value
    image->offset = (TUINT32)-1;
  }
  return retval;
}

/*-----------------------------------------------------------------------------
 * Check if current row of RGB image is valid
 *----------------------------------------------------------------------------*/
static int img_badrow(IMAGERGB *image, int y, int z) {
  return (y >= image->ysize || z >= image->zsize) ? 1 : 0;
}

/*-----------------------------------------------------------------------------
 * Seek to image data area in RGB file
 *----------------------------------------------------------------------------*/
static TUINT32 img_seek(IMAGERGB *image, unsigned int y, unsigned int z,
                        unsigned int offs) {
  if (img_badrow(image, y, z)) {
    std::cout << "imglib: row number out of range" << std::endl;
    return (TUINT32)EOF;
  }
  image->x = 0;
  image->y = y;
  image->z = z;
  if (ISVERBATIM(image->type)) {
    switch (image->dim) {
    case 1:
      return img_optseek(image, 512L + offs);
    case 2:
      return img_optseek(image,
                         512L + offs + (y * image->xsize) * BPP(image->type));
    case 3:
      return img_optseek(
          image, 512L + offs +
                     (y * image->xsize + z * image->xsize * image->ysize) *
                         BPP(image->type));
    default:
      std::cout << "img_seek: weird dim" << std::endl;
      break;
    }
  } else if (ISRLE(image->type)) {
    switch (image->dim) {
    case 1:
      return img_optseek(image, offs + image->rowstart[0]);
    case 2:
      return img_optseek(image, offs + image->rowstart[y]);
    case 3:
      return img_optseek(image, offs + image->rowstart[y + z * image->ysize]);
    default:
      std::cout << "img_seek: weird dim" << std::endl;
      break;
    }
  } else {
    std::cout << "img_seek: weird image type" << std::endl;
  }
  return 0;
}

/*-----------------------------------------------------------------------------
 * Read a row (compressed/uncompressed) from RGB file.
 *----------------------------------------------------------------------------*/
static int new_getrow(IMAGERGB *image, void *buffer, UINT y, UINT z) {
  short cnt;

  if (!(image->flags & (_IORW | _IOREAD))) return -1;

  if (image->dim < 3) z = 0;
  if (image->dim < 2) y = 0;

  img_seek(image, y, z, 0);

  if (ISVERBATIM(image->type)) {
    switch (BPP(image->type)) {
    case 1:
      if (rgb_img_read(image, (char *)buffer, image->xsize) != image->xsize)
        return -1;
      return image->xsize;
    case 2:
      cnt = image->xsize << 1;
      if (rgb_img_read(image, (char *)buffer, cnt) != cnt)
        return -1;
      else {
        if (image->dorev) cvtshorts((unsigned short *)buffer, cnt);
        return image->xsize;
      }
    default:
      std::cout << "getrow: weird bpp" << std::endl;
      break;
    }
  } else if (ISRLE(image->type)) {
    switch (BPP(image->type)) {
    case 1:
      if ((cnt = img_getrowsize(image)) == -1) return -1;
      if (rgb_img_read(image, (char *)image->tmpbuf, cnt) != cnt)
        return -1;
      else {
        img_rle_expand(image->tmpbuf, 1, (unsigned short *)buffer, 1);
        return image->xsize;
      }
    case 2:
      if ((cnt = img_getrowsize(image)) == -1) return -1;
      if (cnt != rgb_img_read(image, (char *)image->tmpbuf, cnt))
        return -1;
      else {
        if (image->dorev) cvtshorts(image->tmpbuf, cnt);
        img_rle_expand(image->tmpbuf, 2, (unsigned short *)buffer, 2);
        return image->xsize;
      }
    default:
      std::cout << "getrow: weird bpp" << std::endl;
      break;
    }
  } else {
    std::cout << "getrow: weird image type" << std::endl;
  }
  return -1;
}

/*-----------------------------------------------------------------------------
 * Code taken from ASMWRITERGB.C
 *----------------------------------------------------------------------------*/
static TINT32 RGB_img_write(IMAGERGB *image, char *buffer, TINT32 count) {
  TINT32 retval;

  retval = write(image->file, buffer, count);
  if (retval == count) {
    image->offset += count;
  } else {
    image->offset = (TUINT32)-1;
  }
  return retval;
}

/*-----------------------------------------------------------------------------*/

static void img_setrowsize(IMAGERGB *image, UINT cnt, UINT y, UINT z) {
  TINT32 *sizeptr = nullptr;

  if (img_badrow(image, y, z)) return;

  switch (image->dim) {
  case 1:
    sizeptr            = &image->rowsize[0];
    image->rowstart[0] = image->rleend;
    break;
  case 2:
    sizeptr            = &image->rowsize[y];
    image->rowstart[y] = image->rleend;
    break;
  case 3:
    sizeptr = &image->rowsize[y + z * image->ysize];
    image->rowstart[y + z * image->ysize] = image->rleend;
    break;
  }

  if (*sizeptr != -1) image->wastebytes += *sizeptr;
  *sizeptr = (TINT32)cnt;
  image->rleend += cnt;
}

/*-----------------------------------------------------------------------------*/

#define COMPACT_CODE(TYPE)                                                     \
  while (iptr < ibufend) {                                                     \
    sptr = iptr;                                                               \
    iptr += 2;                                                                 \
    while ((iptr < ibufend) &&                                                 \
           ((iptr[-2] != iptr[-1]) || (iptr[-1] != iptr[0])))                  \
      iptr++;                                                                  \
    iptr -= 2;                                                                 \
    count = iptr - sptr;                                                       \
    while (count) {                                                            \
      todo = (TYPE)(count > 126 ? 126 : count);                                \
      count -= todo;                                                           \
      *optr++ = (TYPE)(0x80 | todo);                                           \
      while (todo--) *optr++ = (TYPE) * sptr++;                                \
    }                                                                          \
    sptr = iptr;                                                               \
    cc   = *iptr++;                                                            \
    while ((iptr < ibufend) && (*iptr == cc)) iptr++;                          \
    count = iptr - sptr;                                                       \
    while (count) {                                                            \
      todo = (TYPE)(count > 126 ? 126 : count);                                \
      count -= todo;                                                           \
      *optr++ = (TYPE)todo;                                                    \
      *optr++ = (TYPE)cc;                                                      \
    }                                                                          \
  }                                                                            \
  *optr++ = 0;

/*-----------------------------------------------------------------------------*/

static TINT32 img_rle_compact(unsigned short *expbuf, int ibpp,
                              unsigned short *rlebuf, int obpp, int cnt) {
  if (ibpp == 1 && obpp == 1) {
    unsigned char *iptr    = (unsigned char *)expbuf;
    unsigned char *ibufend = iptr + cnt;
    unsigned char *sptr;
    unsigned char *optr = (unsigned char *)rlebuf;
    TUINT32 todo, cc;
    TINT32 count;

    COMPACT_CODE(unsigned char);
    return (TINT32)(optr - (unsigned char *)rlebuf);
  } else if (ibpp == 1 && obpp == 2) {
    unsigned char *iptr    = (unsigned char *)expbuf;
    unsigned char *ibufend = iptr + cnt;
    unsigned char *sptr;
    unsigned short *optr = rlebuf;
    TUINT32 todo, cc;
    TINT32 count;

    COMPACT_CODE(unsigned short);
    return (TINT32)(optr - rlebuf);
  } else if (ibpp == 2 && obpp == 1) {
    unsigned short *iptr    = expbuf;
    unsigned short *ibufend = iptr + cnt;
    unsigned short *sptr;
    unsigned char *optr = (unsigned char *)rlebuf;
    TUINT32 todo, cc;
    TINT32 count;

    COMPACT_CODE(unsigned char);
    return (TINT32)(optr - (unsigned char *)rlebuf);
  } else if (ibpp == 2 && obpp == 2) {
    unsigned short *iptr    = expbuf;
    unsigned short *ibufend = iptr + cnt;
    unsigned short *sptr;
    unsigned short *optr = rlebuf;
    unsigned short todo, cc;
    TINT32 count;

    COMPACT_CODE(unsigned short);
    return (TINT32)(optr - rlebuf);
  } else {
    std::cout << "rle_compact: bad bpp: " << ibpp << " " << obpp;
    return 0;
  }
}

/*-----------------------------------------------------------------------------*/

static void iclose(IMAGERGB *image) {
  TINT32 tablesize;

  if (!image) return;

  iflush(image);
  img_optseek(image, 0);

  /* CHECK!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/

  if (image->flags & _IOWRT) {
    if (image->dorev) cvtimage(image);
    if (do_rgb_write_header(image, image->file) != IMAGERGB_HEADER_SIZE) {
      fprintf(stderr, "iflush: error on write of image header\n");
      // Don't return early - continue cleanup
    }

    if (image->dorev) cvtimage(image);
    if (ISRLE(image->type)) {
      img_optseek(image, 512L);
      tablesize = image->ysize * image->zsize * (int)sizeof(TINT32);
      if (image->dorev) cvtTINT32s(image->rowstart, tablesize);
      if (RGB_img_write(image, (char *)(image->rowstart), tablesize) !=
          tablesize) {
        fprintf(stderr, "iflush: error on write of rowstart\n");
      }
      if (image->dorev) cvtTINT32s((TUINT32 *)image->rowsize, tablesize);
      if (RGB_img_write(image, (char *)(image->rowsize), tablesize) !=
          tablesize) {
        fprintf(stderr, "iflush: error on write of rowsize\n");
      }
    }
  }

  // Cleanup in reverse order of allocation
  if (image->base) {
    free(image->base);
    image->base = nullptr;
  }
  if (image->tmpbuf) {
    free(image->tmpbuf);
    image->tmpbuf = nullptr;
  }
  if (ISRLE(image->type)) {
    free(image->rowstart);
    image->rowstart = nullptr;
    free(image->rowsize);
    image->rowsize = nullptr;
  }
  free(image);
}

/*-------------------------------------------------------------------------*/

static int new_putrow(IMAGERGB *image, void *buffer, UINT y, UINT z) {
  TINT32 cnt;
  int dorev, bpp;

  if (!(image->flags & (_IORW | _IOWRT))) return -1;

  if (image->dim < 3) z = 0;
  if (image->dim < 2) y = 0;

  image->min = 0;
  bpp        = BPP(image->type);
  dorev      = image->dorev && bpp == 2;

  // Comment out all max/min calculations to speed up
  if (bpp == 1)
    image->max = 255;
  else
    image->max = 65535;

  if (ISVERBATIM(image->type)) {
    img_seek(image, y, z, 0);
    cnt = image->xsize << (bpp - 1);
    if (dorev) cvtshorts((unsigned short *)buffer, cnt);
    if (RGB_img_write(image, (char *)buffer, cnt) != cnt) {
      if (dorev) cvtshorts((unsigned short *)buffer, cnt);
      return -1;
    } else {
      if (dorev) cvtshorts((unsigned short *)buffer, cnt);
      return image->xsize;
    }
  } else if (ISRLE(image->type)) {
    cnt = img_rle_compact((unsigned short *)buffer, bpp, image->tmpbuf, bpp,
                          image->xsize);
    cnt <<= (bpp - 1);
    img_setrowsize(image, cnt, y, z);
    img_seek(image, y, z, 0);
    if (dorev) cvtshorts(image->tmpbuf, cnt);
    if (RGB_img_write(image, (char *)(image->tmpbuf), cnt) != cnt) {
      if (dorev) cvtshorts(image->tmpbuf, cnt);
      return -1;
    } else {
      if (dorev) cvtshorts(image->tmpbuf, cnt);
      return image->xsize;
    }
  } else {
    fprintf(stderr, "putrow: weird image type\n");
  }

  return -1;
}

/*----------------------------------------------------------------------------*/

static int iflush(IMAGERGB *image) {
  unsigned short *base;

  if ((image->flags & _IOWRT) && (base = image->base) != nullptr &&
      (image->ptr - base) > 0) {
    if (new_putrow(image, base, image->y, image->z) != image->xsize) {
      image->flags |= _IOERR;
      return (EOF);
    }
  }
  return 0;
}

/*----------------------------------------------------------------------------*/

class SgiReader final : public Tiio::Reader {
  IMAGERGB *m_header;
  int m_currentY;

public:
  SgiReader() : m_header(nullptr), m_currentY(0) {}

  ~SgiReader() override;

  void open(FILE *file) override;

  TPropertyGroup *getProperties() const override { return nullptr; }

  void readLine(char *buffer, int x0, int x1, int shrink) override;
  void readLine(short *buffer, int x0, int x1, int shrink) override;

  int skipLines(int lineCount) override;
};

//-------------------------------------------------------------------

void SgiReader::open(FILE *file) {
  m_header = iopen(fileno(file), OpenRead, 0, 0, 0, 0, 0, 0);

  if (!m_header) {
    throw std::runtime_error("Can't open file");
  }

  m_info.m_lx             = m_header->xsize;
  m_info.m_ly             = m_header->ysize;
  m_info.m_bitsPerSample  = BPP(m_header->type) * 8;
  m_info.m_samplePerPixel = m_header->zsize;
  auto *prop              = new Tiio::SgiWriterProperties();
  m_info.m_properties     = prop;
  prop->m_endianness.setValue(m_header->dorev == 1 ? L"Big Endian"
                                                   : L"Little Endian");
  prop->m_compressed.setValue(ISRLE(m_header->type) ? true : false);
  std::wstring pixelSize;
  int ps = m_info.m_bitsPerSample * m_info.m_samplePerPixel;
  if (ps == 8)
    pixelSize = L"8 bits (Greyscale)";
  else if (ps == 24)
    pixelSize = L"24 bits";
  else if (ps == 32)
    pixelSize = L"32 bits";
  else if (ps == 48)
    pixelSize = L"48 bits";
  else if (ps == 64)
    pixelSize = L"64 bits";
  prop->m_pixelSize.setValue(pixelSize);
}

//-------------------------------------------------------------------

SgiReader::~SgiReader() {
  if (!m_header) return;

  // Use iclose for proper cleanup
  iclose(m_header);
  m_header = nullptr;
}

//-------------------------------------------------------------------

void SgiReader::readLine(short *buffer, int x0, int x1, int shrink) {
  assert(shrink == 1);
  assert(x0 == 0);
  assert(x1 == m_info.m_lx - 1);
  assert(BPP(m_header->type) == 2);

  // 64 -> 32
  TPixel64 *pix = (TPixel64 *)buffer;
  std::vector<USHORT> rbuf(m_info.m_lx), gbuf(m_info.m_lx), bbuf(m_info.m_lx),
      mbuf(m_info.m_lx);
  if (m_header->zsize == 4) {
    new_getrow(m_header, &rbuf[0], m_currentY, 0);
    new_getrow(m_header, &gbuf[0], m_currentY, 1);
    new_getrow(m_header, &bbuf[0], m_currentY, 2);
    new_getrow(m_header, &mbuf[0], m_currentY, 3);
    for (int i = 0; i < (x1 - x0 + 1); ++i) {
      pix->r = rbuf[i];
      pix->g = gbuf[i];
      pix->b = bbuf[i];
      pix->m = mbuf[i];
      ++pix;
    }
  } else {
    new_getrow(m_header, &rbuf[0], m_currentY, 0);
    new_getrow(m_header, &gbuf[0], m_currentY, 1);
    new_getrow(m_header, &bbuf[0], m_currentY, 2);
    for (int i = 0; i < (x1 - x0 + 1); ++i) {
      pix->r = rbuf[i];
      pix->g = gbuf[i];
      pix->b = bbuf[i];
      pix->m = 0xffff;
      ++pix;
    }
  }
  m_currentY++;
}

//-------------------------------------------------------------------

void SgiReader::readLine(char *buffer, int x0, int x1, int shrink) {
  // Partial read not yet implemented
  x0     = 0;
  x1     = m_info.m_lx - 1;
  shrink = 1;

  if (BPP(m_header->type) == 1) {  // 32 -> 32
    TPixel32 *pix = (TPixel32 *)buffer;
    std::vector<UCHAR> rbuf(m_info.m_lx), gbuf(m_info.m_lx), bbuf(m_info.m_lx),
        mbuf(m_info.m_lx);
    if (m_header->zsize == 4) {
      new_getrow(m_header, &rbuf[0], m_currentY, 0);
      new_getrow(m_header, &gbuf[0], m_currentY, 1);
      new_getrow(m_header, &bbuf[0], m_currentY, 2);
      new_getrow(m_header, &mbuf[0], m_currentY, 3);
      for (int i = 0; i < (x1 - x0 + 1); ++i) {
        pix->r = rbuf[i];
        pix->g = gbuf[i];
        pix->b = bbuf[i];
        pix->m = mbuf[i];
        ++pix;
      }
    } else {
      new_getrow(m_header, &rbuf[0], m_currentY, 0);
      if (m_header->zsize == 1) {
        for (int i = 0; i < (x1 - x0 + 1); ++i) {
          pix->r = rbuf[i];
          pix->g = rbuf[i];
          pix->b = rbuf[i];
          pix->m = 0xff;
          ++pix;
        }
      } else {
        new_getrow(m_header, &gbuf[0], m_currentY, 1);
        new_getrow(m_header, &bbuf[0], m_currentY, 2);
        for (int i = 0; i < (x1 - x0 + 1); ++i) {
          pix->r = rbuf[i];
          pix->g = gbuf[i];
          pix->b = bbuf[i];
          pix->m = 0xff;
          ++pix;
        }
      }
    }
  } else {  // 64 -> 32
    TPixel32 *pix = (TPixel32 *)buffer;
    std::vector<USHORT> rbuf(m_info.m_lx), gbuf(m_info.m_lx), bbuf(m_info.m_lx),
        mbuf(m_info.m_lx);
    if (m_header->zsize == 4) {
      new_getrow(m_header, &rbuf[0], m_currentY, 0);
      new_getrow(m_header, &gbuf[0], m_currentY, 1);
      new_getrow(m_header, &bbuf[0], m_currentY, 2);
      new_getrow(m_header, &mbuf[0], m_currentY, 3);
      for (int i = 0; i < (x1 - x0 + 1); ++i) {
        pix->r = rbuf[i] >> 8;
        pix->g = gbuf[i] >> 8;
        pix->b = bbuf[i] >> 8;
        pix->m = mbuf[i] >> 8;
        ++pix;
      }
    } else {
      new_getrow(m_header, &rbuf[0], m_currentY, 0);
      new_getrow(m_header, &gbuf[0], m_currentY, 1);
      new_getrow(m_header, &bbuf[0], m_currentY, 2);
      for (int i = 0; i < (x1 - x0 + 1); ++i) {
        pix->r = rbuf[i] >> 8;
        pix->g = gbuf[i] >> 8;
        pix->b = bbuf[i] >> 8;
        pix->m = 0xff;
        ++pix;
      }
    }
  }
  m_currentY++;
}

//-------------------------------------------------------------------

int SgiReader::skipLines(int lineCount) {
  m_currentY += lineCount;
  return lineCount;
}

/*-----------------------------------------------------------------------------
 *                             WRITER
 *----------------------------------------------------------------------------*/

class SgiWriter final : public Tiio::Writer {
  int m_currentY;
  IMAGERGB *m_header;

public:
  SgiWriter() : m_currentY(0), m_header(nullptr) {}
  ~SgiWriter() override {
    if (m_header) {
      iclose(m_header);
      m_header = nullptr;
    }
  }

  void open(FILE *file, const TImageInfo &info) override;

  TPropertyGroup *getProperties() { return m_properties; }

  void writeLine(char *buffer) override;
  void writeLine(short *buffer) override;

  void flush() override {}

  bool write64bitSupported() const override { return true; }

  void setProperties(TPropertyGroup *properties) {
    m_properties = dynamic_cast<Tiio::SgiWriterProperties *>(properties);
  }

  bool writeAlphaSupported() const override {
    return m_header && (m_header->zsize == 4);
  }

private:
  // Delete copy constructor and assignment operator
  SgiWriter(const SgiWriter &)            = delete;
  SgiWriter &operator=(const SgiWriter &) = delete;
};

Tiio::Reader *Tiio::makeSgiReader() { return new SgiReader(); }

//=========================================================

Tiio::Writer *Tiio::makeSgiWriter() { return new SgiWriter(); }

//---------------------------------------------------------

void SgiWriter::open(FILE *file, const TImageInfo &info) {
  if (!m_properties) {
    m_properties = new Tiio::SgiWriterProperties();
  }

  auto *p = dynamic_cast<TEnumProperty *>(
      m_properties->getProperty("Bits Per Pixel"));
  if (!p) {
    throw std::runtime_error("Bits Per Pixel property not found");
  }
  std::string str     = ::to_string(p->getValue());
  int bitPerPixel     = atoi(str.c_str());
  int channelBytesNum = 1;
  int dim             = 3;
  int zsize           = 1;

  m_info = info;

  switch (bitPerPixel) {
  case 8:
    dim   = 2;
    zsize = 1;
    break;
  case 24:
    dim   = 3;
    zsize = 3;
    break;
  case 32:
    dim   = 3;
    zsize = 4;
    break;
  case 48:
    dim             = 3;
    zsize           = 3;
    channelBytesNum = 2;
    break;
  case 64:
    zsize           = 4;
    dim             = 3;
    channelBytesNum = 2;
    break;
  }

  auto *bp = dynamic_cast<TBoolProperty *>(
      m_properties->getProperty("RLE-Compressed"));
  assert(bp);
  bool compressed = bp->getValue();
  p = dynamic_cast<TEnumProperty *>(m_properties->getProperty("Endianness"));
  assert(p);
  str            = ::to_string(p->getValue());
  bool bigEndian = (str == "Big Endian");

  m_header =
      iopen(fileno(file), OpenWrite,
            compressed ? RLE(channelBytesNum) : VERBATIM(channelBytesNum), dim,
            m_info.m_lx, m_info.m_ly, zsize, bigEndian ? 1 : 0);

  if (!m_header) {
    throw std::runtime_error("Failed to open SGI file for writing");
  }
}
//---------------------------------------------------------

void SgiWriter::writeLine(char *buffer) {
  if (BPP(m_header->type) == 1) {
    if (m_header->dim < 3) {  // 32->8
      new_putrow(m_header, buffer, m_currentY, 0);
    } else {
      std::vector<UCHAR> rbuf(m_info.m_lx), gbuf(m_info.m_lx),
          bbuf(m_info.m_lx), mbuf(m_info.m_lx);
      TPixelRGBM32 *pix = (TPixelRGBM32 *)buffer;
      for (int i = 0; i < m_info.m_lx; ++i) {
        rbuf[i] = pix->r;
        gbuf[i] = pix->g;
        bbuf[i] = pix->b;
        mbuf[i] = pix->m;
        ++pix;
      }
      new_putrow(m_header, &rbuf[0], m_currentY, 0);
      new_putrow(m_header, &gbuf[0], m_currentY, 1);
      new_putrow(m_header, &bbuf[0], m_currentY, 2);
      if (m_header->zsize == 4) new_putrow(m_header, &mbuf[0], m_currentY, 3);
    }
  }
  ++m_currentY;
}

//---------------------------------------------------------

void SgiWriter::writeLine(short *buffer) {
  assert(BPP(m_header->type) == 2);

  if (m_header->dim < 3) {
    std::vector<USHORT> tmp(m_info.m_lx);
    TPixelRGBM64 *pix = (TPixelRGBM64 *)buffer;
    for (int i = 0; i < m_info.m_lx; ++i) {
      tmp[i] = TPixelGR16::from(*pix).value;
      ++pix;
    }
    new_putrow(m_header, &tmp[0], m_currentY, 0);
  } else {
    std::vector<USHORT> rbuf(m_info.m_lx), gbuf(m_info.m_lx), bbuf(m_info.m_lx),
        mbuf(m_info.m_lx);
    TPixelRGBM64 *pix = (TPixelRGBM64 *)buffer;
    for (int i = 0; i < m_info.m_lx; ++i) {
      rbuf[i] = pix->r;
      gbuf[i] = pix->g;
      bbuf[i] = pix->b;
      mbuf[i] = pix->m;
      ++pix;
    }
    new_putrow(m_header, &rbuf[0], m_currentY, 0);
    new_putrow(m_header, &gbuf[0], m_currentY, 1);
    new_putrow(m_header, &bbuf[0], m_currentY, 2);
    if (m_header->zsize == 4) new_putrow(m_header, &mbuf[0], m_currentY, 3);
  }
  ++m_currentY;
}

//=========================================================

Tiio::SgiWriterProperties::SgiWriterProperties()
    : m_pixelSize("Bits Per Pixel")
    , m_endianness("Endianness")
    , m_compressed("RLE-Compressed", false) {
  m_pixelSize.addValue(L"24 bits");
  m_pixelSize.addValue(L"32 bits");
  m_pixelSize.addValue(L"48 bits");
  m_pixelSize.addValue(L"64 bits");
  m_pixelSize.addValue(L"8 bits (Greyscale)");
  m_pixelSize.setValue(L"32 bits");
  bind(m_pixelSize);
  bind(m_compressed);
  m_endianness.addValue(L"Big Endian");
  m_endianness.addValue(L"Little Endian");
  bind(m_endianness);
}

void Tiio::SgiWriterProperties::updateTranslation() {
  m_pixelSize.setQStringName(tr("Bits Per Pixel"));
  m_pixelSize.setItemUIName(L"24 bits", tr("24 bits"));
  m_pixelSize.setItemUIName(L"32 bits", tr("32 bits"));
  m_pixelSize.setItemUIName(L"48 bits", tr("48 bits"));
  m_pixelSize.setItemUIName(L"64 bits", tr("64 bits"));
  m_pixelSize.setItemUIName(L"8 bits (Greyscale)", tr("8 bits (Greyscale)"));
  m_endianness.setQStringName(tr("Endianness"));
  m_endianness.setItemUIName(L"Big Endian", tr("Big Endian"));
  m_endianness.setItemUIName(L"Little Endian", tr("Little Endian"));
  m_compressed.setQStringName(tr("RLE-Compressed"));
}
