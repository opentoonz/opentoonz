

#include "autopos.h"
#include "cleanupcommon.h"

#include <sstream>

using namespace CleanupTypes;

/*
guardare DAFARE
guardare assumo
autoalign rgb ->ora viene chiamata con un buffer rgbm ! modificare
opportunamente

fare resize e realloc size dello stack a 65000 unita'

*/

#define SECURITY_MARGIN_MM 4.0

static bool Debug_flag = false;

/*===========================================================================*/
/*

     AUTOALIGNMENT


*/

#define AUTOAL_BLACK_COLS 2
#define AUTOAL_WHITE_COLS 2
#define AUTOAL_THRESHOLD 160

static bool autoalign_gr8(UCHAR *buffer_gr8, int wrap, int lx, int ly,
                         int pix_origin, int dpix_dx, int dpix_dy,
                         int strip_width) {
  int first_x[2], dx_dcol[2], target[2];
  int i, x, y, cols;
  int col_value, threshold;
  int consec_black_cols, consec_white_cols, black_strip_edge;
  UCHAR *pix, *origin;
  int delta_x, delta_pix;

  origin = buffer_gr8 + pix_origin;

  first_x[0] = 0;
  dx_dcol[0] = 1;
  target[0]  = strip_width / 2;
  first_x[1] = lx - 1;
  dx_dcol[1] = -1;
  target[1]  = lx - 1 - strip_width / 2;

  threshold = AUTOAL_THRESHOLD * ly;

  for (i = 0; i < 2; i++) {
    consec_black_cols = 0;
    consec_white_cols = 0;
    black_strip_edge  = 0;
    for (x = first_x[i], cols = 0; cols < strip_width;
         x += dx_dcol[i], cols++) {
      col_value = 0;
      pix       = origin + x * dpix_dx;
      for (y = 0; y < ly; y++) {
        col_value += *pix;
        pix += dpix_dy;
      }
      if (col_value < threshold) {
        consec_white_cols = 0;
        consec_black_cols++;
        if (consec_black_cols >= AUTOAL_BLACK_COLS) black_strip_edge = x;
      } else {
        consec_black_cols = 0;
        consec_white_cols++;
        if (consec_white_cols >= AUTOAL_WHITE_COLS && black_strip_edge)
          goto found;
      }
    }
  }
  return false;

found:

  for (x = first_x[i], cols = 0; cols < strip_width; x += dx_dcol[i], cols++) {
    pix = origin + x * dpix_dx;
    for (y = 0; y < ly; y++) {
      *pix = 255;
      pix += dpix_dy;
    }
  }
  delta_x   = target[i] - black_strip_edge;
  delta_pix = delta_x * dpix_dx;
  if (delta_x > 0) {
    for (x = lx - 1 - delta_x; x >= 0; x--) {
      pix = origin + x * dpix_dx;
      for (y = 0; y < ly; y++) {
        *(pix + delta_pix) = *pix;
        pix += dpix_dy;
      }
    }
    for (x = lx - delta_x; x < lx; x++) {
      pix = origin + x * dpix_dx;
      for (y = 0; y < ly; y++) {
        *pix = 255;
        pix += dpix_dy;
      }
    }
  } else if (delta_x < 0) {
    for (x = -delta_x; x < lx; x++) {
      pix = origin + x * dpix_dx;
      for (y = 0; y < ly; y++) {
        *(pix + delta_pix) = *pix;
        pix += dpix_dy;
      }
    }
    for (x = 0; x < -delta_x; x++) {
      pix = origin + x * dpix_dx;
      for (y = 0; y < ly; y++) {
        *pix = 255;
        pix += dpix_dy;
      }
    }
  }
  return true;
}

/*---------------------------------------------------------------------------*/

static bool autoalign_rgb(TPixel32 *buffer_rgb, int wrap, int lx, int ly,
                         int pix_origin, int dpix_dx, int dpix_dy,
                         int strip_width) {
  int first_x[2], dx_dcol[2], target[2];
  int i, x, y, cols;
  int col_value, threshold;
  int consec_black_cols, consec_white_cols, black_strip_edge;
  TPixel32 *pix, *origin;
  int delta_x, delta_pix;

  origin = buffer_rgb + pix_origin;

  first_x[0] = 0;
  dx_dcol[0] = 1;
  target[0]  = strip_width / 2;
  first_x[1] = lx - 1;
  dx_dcol[1] = -1;
  target[1]  = lx - 1 - strip_width / 2;

  threshold = AUTOAL_THRESHOLD * ly;

  for (i = 0; i < 2; i++) {
    consec_black_cols = 0;
    consec_white_cols = 0;
    black_strip_edge  = 0;
    for (x = first_x[i], cols = 0; cols < strip_width;
         x += dx_dcol[i], cols++) {
      col_value = 0;
      pix       = origin + x * dpix_dx;
      for (y = 0; y < ly; y++) {
        col_value += (pix->r * 2 + pix->g * 5 + pix->b) >> 3;
        pix += dpix_dy;
      }
      if (col_value < threshold) {
        consec_white_cols = 0;
        consec_black_cols++;
        if (consec_black_cols >= AUTOAL_BLACK_COLS) black_strip_edge = x;
      } else {
        consec_black_cols = 0;
        consec_white_cols++;
        if (consec_white_cols >= AUTOAL_WHITE_COLS && black_strip_edge)
          goto found;
      }
    }
  }
  return false;

found:

  for (x = first_x[i], cols = 0; cols < strip_width; x += dx_dcol[i], cols++) {
    pix = origin + x * dpix_dx;
    for (y = 0; y < ly; y++) {
      pix->r = pix->g = pix->b = 255;
      pix += dpix_dy;
    }
  }
  delta_x   = target[i] - black_strip_edge;
  delta_pix = delta_x * dpix_dx;
  if (delta_x > 0) {
    for (x = lx - 1 - delta_x; x >= 0; x--) {
      pix = origin + x * dpix_dx;
      for (y = 0; y < ly; y++) {
        (pix + delta_pix)->r = pix->r;
        (pix + delta_pix)->g = pix->g;
        (pix + delta_pix)->b = pix->b;
        pix += dpix_dy;
      }
    }
    for (x = lx - delta_x; x < lx; x++) {
      pix = origin + x * dpix_dx;
      for (y = 0; y < ly; y++) {
        pix->r = pix->g = pix->b = 255;
        pix += dpix_dy;
      }
    }
  } else if (delta_x < 0) {
    for (x = -delta_x; x < lx; x++) {
      pix = origin + x * dpix_dx;
      for (y = 0; y < ly; y++) {
        (pix + delta_pix)->r = pix->r;
        (pix + delta_pix)->g = pix->g;
        (pix + delta_pix)->b = pix->b;
        pix += dpix_dy;
      }
    }
    for (x = 0; x < -delta_x; x++) {
      pix = origin + x * dpix_dx;
      for (y = 0; y < ly; y++) {
        pix->r = pix->g = pix->b = 255;
        pix += dpix_dy;
      }
    }
  }
  return true;
}

/*---------------------------------------------------------------------------*/

bool do_autoalign(const TRasterImageP &image) {
  int wrap, lx, ly, mx, my;
  int pix_origin, dpix_dx, dpix_dy;
  int strip_width;

  // assumo che sia sempre orientata nel modo corretto

  TRasterP ras = image->getRaster();
  wrap         = ras->getWrap();
  assert(TRaster32P(ras) ||
         TRasterGR8P(ras));  // per ricordare di gestire le img bw!

  // assumo TOR_BOTLEFT:__OR TOR_BOTRIGHT:__OR TOR_TOPLEFT:__OR TOR_TOPRIGHT:
  double dpix, dpiy;
  image->getDpi(dpix, dpiy);
  strip_width = (int)mmToPixel(5.0, dpix);
  lx          = ras->getLx();
  ly          = ras->getLy();

  mx = lx - 1;
  my = ly - 1;

  // assumo  CASE TOR_BOTLEFT:
  pix_origin = 0;
  dpix_dx    = 1;
  dpix_dy    = wrap;

  TRasterGR8P ras8(ras);
  TRaster32P ras32(ras);
  ras->lock();
  bool ret = false;

  if (ras8)
    ret = autoalign_gr8(ras8->getRawData(), wrap, lx, ly, pix_origin, dpix_dx,
                        dpix_dy, strip_width);

  else if (ras32)
    ret = autoalign_rgb(ras32->pixels(), wrap, lx, ly, pix_origin, dpix_dx,
                        dpix_dy, strip_width);
  else
    assert(!"Unsupported pixel type");

  ras->unlock();

  return false;
}

/*===========================================================================*/
/*

     AUTOCENTERING


*/
/*
 * Calcoli in millimetri per questa funzione che alla fine restituisce un
 * valore per la striscia di ricerca direttamente in pixel
 */

int compute_strip_pixel(FDG_INFO *fdg, double dpi) {
  int i, strip_size_pix;
  double half_size, max_half_size, strip_size_mm;

  max_half_size = -1.0;
  for (i = 0; i < (int)fdg->dots.size(); i++) {
    half_size = (double)fdg->dots[i].lx * 0.5;
    if (max_half_size < half_size) max_half_size = half_size;
  }
  strip_size_mm =
      fdg->dist_ctr_hole_to_edge + max_half_size + SECURITY_MARGIN_MM;

  strip_size_pix = (int)mmToPixel(strip_size_mm, dpi);

  if (Debug_flag)
    printf("Controllo una striscia larga %g mm e %d pixels\n", strip_size_mm,
           strip_size_pix);

  return strip_size_pix;
}

/*---------------------------------------------------------------------------*/

#define SQMM_TO_SQPIXEL(area, x_res, y_res)                                    \
  ((double)((x_res) * (y_res)) * (double)(area) * ((1.0 / 25.4) * (1.0 / 25.4)))

/*---------------------------------------------------------------------------*/
void convert_dots_mm_to_pixel(DOT *dots, int nd, double x_res, double y_res) {
  int i;

  for (i = 0; i < nd; i++) {
    dots[i].x1   = troundp(mmToPixel(dots[i].x1, x_res));
    dots[i].y1   = troundp(mmToPixel(dots[i].y1, y_res));
    dots[i].x2   = troundp(mmToPixel(dots[i].x2, x_res));
    dots[i].y2   = troundp(mmToPixel(dots[i].y2, y_res));
    dots[i].x    = (float)mmToPixel(dots[i].x, x_res);
    dots[i].y    = (float)mmToPixel(dots[i].y, y_res);
    dots[i].lx   = troundp(mmToPixel(dots[i].lx, x_res));
    dots[i].ly   = troundp(mmToPixel(dots[i].ly, y_res));
    dots[i].area = troundp(SQMM_TO_SQPIXEL(dots[i].area, x_res, y_res));
  }
  return;
}

/*---------------------------------------------------------------------------*/

static char *Done       = 0;
static int Done_rowsize = 0;
static int Done_colsize = 0;
#define DONE_MASK(I, J) (1 << (((I) + (J)*Done_rowsize) & 7))
#define DONE_BYTE(I, J) (((I) + (J)*Done_rowsize) >> 3)
#define SET_DONE(I, J) (Done[DONE_BYTE(I, J)] |= DONE_MASK(I, J))
#define NOT_DONE(I, J) (!(Done[DONE_BYTE(I, J)] & DONE_MASK(I, J)))

static int Pix_ystep = 0;

typedef struct big { unsigned lo, hi; } BIG;
#define CLEARBIG(B) ((B).lo = 0, (B).hi = 0, (B))
#define ADDBIG(B, X)                                                           \
  ((B).lo += (unsigned)(X), (B).hi += (B).lo >> 30, (B).lo &= 0x3fffffff, (B))
#define BIG_TO_DOUBLE(B) ((double)(B).hi * (double)0x40000000 + (double)(B).lo)

#define IS_BLACK_GR8(PIX) (*(PIX) < 110)
#define IS_VERY_BLACK_GR8(PIX) (*(PIX) < 30)
#define BLACK_WEIGHT_GR8(PIX) (256 - *(PIX))

#define RGBR(PIX) ((PIX)->r << 1)
#define RGBG(PIX) ((PIX)->g << 2)
#define RGBB(PIX) ((PIX)->b)
#define RGBVAL(PIX) (RGBR(PIX) + RGBG(PIX) + RGBB(PIX))

#define IS_BLACK_RGB(PIX) (RGBVAL(PIX) < 110 * 7)
#define IS_VERY_BLACK_RGB(PIX) (RGBVAL(PIX) < 30 * 7)
#define BLACK_WEIGHT_RGB(PIX) ((256 * 7 - RGBVAL(PIX)) >> 3)

static BIG Xsum, Ysum, Weightsum;
static int Xmin, Xmax, Ymin, Ymax, Npix;
#ifdef RECURSIVE_VERSION
static int Level, Max_level;
#endif
static bool Very_black_found;

#ifdef DAFARE
#ifdef RECURSIVE_VERSION
static int Black_pixel = 0;
#endif
#endif

#ifdef DAFARE
static int find_dots_bw(const TRasterP &img, int strip_width,
                        PEGS_SIDE pegs_side, DOT dotarray[], int dotarray_size,
                        int max_area);
#endif
static int find_dots_gr8(const TRasterGR8P &img, int strip_width,
                         PEGS_SIDE pegs_side, DOT dotarray[], int dotarray_size,
                         int max_area);
static int find_dots_rgb(const TRaster32P &img, int strip_width,
                         PEGS_SIDE pegs_side, DOT dotarray[], int dotarray_size,
                         int max_area);
static void
#ifdef DAFARE
visit_bw(int i, int j, int x, int y, int bit, UCHAR *byte),
#endif
    visit_gr8(int i, int j, int x, int y, UCHAR *pix),
    visit_rgb(int i, int j, int x, int y, TPixel32 *pix),
    stampa_dot(DOT const *dot);

//! \brief Find the best matching pegs
//!
//! The found pegs are in array dots. The function checks, which of those best
//! fits the reference in
//! reference. The three best matching dots are returned in parameters i, j, k.
static bool compare_dots(DOT const dots[], int ndots, DOT reference[],
                        int ref_dot, int &i, int &j, int &k);

#define REVERSE(byte, bit)                                                     \
  {                                                                            \
    unsigned char mask;                                                        \
    mask = 1 << (bit);                                                         \
    *(byte) ^= mask;                                                           \
  }

/*---------------------------------------------------------------------------*/

typedef struct {
  short ret, bit;
  int x, y, i, j;
  void *ptr;
} STACK_INFO;

static STACK_INFO *Stack    = 0;
static int Stack_alloc_size = 0;
static int Stack_size       = 0;

#define CREATE_STACK                                                           \
  {                                                                            \
    assert(!Stack);                                                            \
    Stack_alloc_size = 65500;                                                  \
    Stack_size       = 0;                                                      \
    Stack = (STACK_INFO *)malloc(Stack_alloc_size * sizeof(STACK_INFO));       \
    if (!Stack) return false;                                                  \
  }

#define DESTROY_STACK                                                          \
  {                                                                            \
    Stack_alloc_size = 0;                                                      \
    Stack_size       = 0;                                                      \
    free(Stack);                                                               \
    Stack = 0;                                                                 \
  }

#define STACK_IS_EMPTY (!Stack_size)

#define PUSH_ONTO_STACK(RET, X, Y, I, J, BIT, PTR)                             \
  {                                                                            \
    if (Stack_size >= Stack_alloc_size) {                                      \
      Stack_alloc_size += 65500;                                               \
      Stack =                                                                  \
          (STACK_INFO *)realloc(Stack, Stack_alloc_size * sizeof(STACK_INFO)); \
      if (!Stack) return;                                                      \
    }                                                                          \
    Stack[Stack_size].ret = (RET);                                             \
    Stack[Stack_size].x   = (X);                                               \
    Stack[Stack_size].y   = (Y);                                               \
    Stack[Stack_size].i   = (I);                                               \
    Stack[Stack_size].j   = (J);                                               \
    Stack[Stack_size].bit = (BIT);                                             \
    Stack[Stack_size].ptr = (PTR);                                             \
    Stack_size++;                                                              \
  }

#define POP_FROM_STACK_U(RET, X, Y, I, J, BIT, PTR)                            \
  {                                                                            \
    Stack_size--;                                                              \
    (RET) = Stack[Stack_size].ret;                                             \
    (X)   = Stack[Stack_size].x;                                               \
    (Y)   = Stack[Stack_size].y;                                               \
    (I)   = Stack[Stack_size].i;                                               \
    (J)   = Stack[Stack_size].j;                                               \
    (BIT) = Stack[Stack_size].bit;                                             \
    (PTR) = (UCHAR *)Stack[Stack_size].ptr;                                    \
  }

#define POP_FROM_STACK_TPIXEL32(RET, X, Y, I, J, BIT, PTR)                     \
  {                                                                            \
    Stack_size--;                                                              \
    (RET) = Stack[Stack_size].ret;                                             \
    (X)   = Stack[Stack_size].x;                                               \
    (Y)   = Stack[Stack_size].y;                                               \
    (I)   = Stack[Stack_size].i;                                               \
    (J)   = Stack[Stack_size].j;                                               \
    (BIT) = Stack[Stack_size].bit;                                             \
    (PTR) = (TPixel32 *)Stack[Stack_size].ptr;                                 \
  }

/*---------------------------------------------------------------------------*/

static int find_dots(const TRasterP &img, int strip_width, PEGS_SIDE pegs_side,
                     DOT dotarray[], int dotarray_size, int max_area) {
  TRaster32P ras32(img);
  if (ras32)
    return find_dots_rgb(ras32, strip_width, pegs_side, dotarray, dotarray_size,
                         max_area);
  TRasterGR8P ras8(img);
  if (ras8)
    return find_dots_gr8(ras8, strip_width, pegs_side, dotarray, dotarray_size,
                         max_area);
  assert(!"Unsupported pixel type");

  return 0;
}

/*---------------------------------------------------------------------------*/
#ifdef DAFARE
static int find_dots_bw(const TRasterP &img, int strip_width,
                        PEGS_SIDE pegs_side, DOT dotarray[], int dotarray_size,
                        int max_area) {
  int n_dots, ins, shift;
  int x, y;  // coords in img coord system
  int i, j;  // coords in done coord system
  int x0, y0, xsize, ysize, xlast, ylast, bit;
  UCHAR *byte, *buffer;
  int dot_lx, dot_ly;
  float dot_x, dot_y;
  bool vertical;

  if (img->type == RAS_WB)
    Black_pixel = 1;
  else if (img->type == RAS_BW)
    Black_pixel = 0;
  else {
    TERROR("find dots error: bad image type");
    return 0;
  }

  switch (pegs_side) {
  case PEGS_BOTTOM:
  case PEGS_TOP:
    x0       = 0;
    y0       = pegs_side == PEGS_BOTTOM ? 0 : img->ly - strip_width;
    xsize    = img->lx;
    ysize    = strip_width;
    vertical = false;
    break;

  case PEGS_LEFT:
  case PEGS_RIGHT:
    x0       = pegs_side == PEGS_LEFT ? 0 : img->lx - strip_width;
    y0       = 0;
    xsize    = strip_width;
    ysize    = img->ly;
    vertical = true;
    break;

  default: {
    std::ostringstream os;
    os << "find dots internal error: pegs_side = " << std::hex << pegs_side
       << '\0';
    throw TCleanupException(os.str().c_str());
    x0 = y0 = xsize = ysize = 0;
    vertical = false;
  }
  }
  xlast        = x0 + xsize - 1;
  ylast        = y0 + ysize - 1;
  n_dots       = 0;
  Done_rowsize = xsize + 2;
  Done_colsize = ysize + 2;
  Done = (char *)calloc((size_t)((Done_rowsize * Done_colsize + 7) >> 3),
                        sizeof(char));
  if (!Done) {
    throw TCleanupException("find_dots: out of memory");
  }
  for (i = 0; i < Done_rowsize; i++) {
    SET_DONE(i, 0);
    SET_DONE(i, Done_colsize - 1);
  }
  for (j = 0; j < Done_colsize; j++) {
    SET_DONE(0, j);
    SET_DONE(Done_rowsize - 1, j);
  }
  buffer    = (UCHAR *)img->buffer;
  Pix_ystep = (img->wrap + 7) >> 3;

  if (Debug_flag) {
    printf("Zona di scansione: (%d,%d) -- (%d,%d)\n", x0, y0, xlast, ylast);
    printf("wrap: %d\n", img->wrap);
  }

#ifdef RECURSIVE_VERSION
  Max_level = max_area * 6 / 5;
#endif

  CREATE_STACK
  for (j = 1, y = y0; y <= ylast; j++, y++) {
    bit  = 7 - (x0 & 7);
    byte = buffer + (x0 >> 3) + y * Pix_ystep;
    for (i = 1, x = x0; x <= xlast; i++, x++) {
      if (NOT_DONE(i, j) && ((*byte >> bit) & 1) == Black_pixel) {
        CLEARBIG(Xsum);
        CLEARBIG(Ysum);
        CLEARBIG(Weightsum);
        Xmin = Xmax = x;
        Ymin = Ymax = y;
        Npix        = 0;
#ifdef RECURSIVE_VERSION
        Level = 0;
#endif
        visit_bw(i, j, x, y, bit, byte);
        dot_lx = Xmax - Xmin + 1;
        dot_ly = Ymax - Ymin + 1;
        if (Npix < max_area * 3 / 2 && dot_lx > 3 && dot_lx < (xsize >> 1) &&
            dot_ly > 3 && dot_ly < (ysize >> 1) && Xmin > x0 && Xmax < xlast &&
            Ymin > y0 && Ymax < ylast) {
          dot_x = BIG_TO_DOUBLE(Xsum) / BIG_TO_DOUBLE(Weightsum);
          dot_y = BIG_TO_DOUBLE(Ysum) / BIG_TO_DOUBLE(Weightsum);
          if (vertical) {
            for (ins = 0; ins < n_dots; ins++)
              if (dotarray[ins].y > dot_y) break;
          } else {
            for (ins = 0; ins < n_dots; ins++)
              if (dotarray[ins].x > dot_x) break;
          }
          for (shift            = n_dots - 1; shift >= ins; shift--)
            dotarray[shift + 1] = dotarray[shift];
          n_dots++;
          dotarray[ins].x1   = Xmin;
          dotarray[ins].x2   = Xmax;
          dotarray[ins].y1   = Ymin;
          dotarray[ins].y2   = Ymax;
          dotarray[ins].lx   = dot_lx;
          dotarray[ins].ly   = dot_ly;
          dotarray[ins].area = Npix;
          dotarray[ins].x    = dot_x;
          dotarray[ins].y    = dot_y;
          if (n_dots >= dotarray_size) goto end_loop;
        }
      }
      if (bit == 0) {
        bit = 7;
        byte++;
      } else
        bit--;
    }
  }
end_loop:
  DESTROY_STACK
  free(Done);
  Done = NIL;
  return n_dots;
}
#endif
/*---------------------------------------------------------------------------*/

static int find_dots_gr8(const TRasterGR8P &img, int strip_width,
                         PEGS_SIDE pegs_side, DOT dotarray[], int dotarray_size,
                         int max_area) {
  int n_dots, ins, shift;
  int x, y; /* coords in img coord system */
  int i, j; /* coords in done coord system */
  int x0, y0, xsize, ysize, xlast, ylast;
  UCHAR *pix, *buffer;
  int dot_lx, dot_ly;
  float dot_x, dot_y;
  bool vertical;

  switch (pegs_side) {
  case PEGS_BOTTOM:
  case PEGS_TOP:
    x0       = 0;
    y0       = pegs_side == PEGS_BOTTOM ? 0 : img->getLy() - strip_width;
    xsize    = img->getLx();
    ysize    = strip_width;
    vertical = false;
    break;
  case PEGS_LEFT:
  case PEGS_RIGHT:
    x0       = pegs_side == PEGS_LEFT ? 0 : img->getLx() - strip_width;
    y0       = 0;
    xsize    = strip_width;
    ysize    = img->getLy();
    vertical = true;
    break;
  default: {
    std::ostringstream os;
    os << "find dots internal error: pegs_side = " << std::hex << pegs_side
       << '\0';
    throw TCleanupException(os.str().c_str());
    x0 = y0 = xsize = ysize = 0;
    vertical = false;
  }
  }

  xlast        = x0 + xsize - 1;
  ylast        = y0 + ysize - 1;
  n_dots       = 0;
  Done_rowsize = xsize + 2;
  Done_colsize = ysize + 2;
  Done = (char *)calloc((size_t)((Done_rowsize * Done_colsize + 7) >> 3),
                        sizeof(char));
  if (!Done) {
    throw TCleanupException("find_dots: out of memory");
  }
  for (i = 0; i < Done_rowsize; i++) {
    SET_DONE(i, 0);
    SET_DONE(i, Done_colsize - 1);
  }
  for (j = 0; j < Done_colsize; j++) {
    SET_DONE(0, j);
    SET_DONE(Done_rowsize - 1, j);
  }

  Pix_ystep = img->getWrap();
  if (Debug_flag) {
    printf("Zona di scansione: (%d,%d) -- (%d,%d)\n", x0, y0, xlast, ylast);
    printf("wrap: %d\n", img->getWrap());
  }
#ifdef RECURSIVE_VERSION
  Max_level = max_area * 6 / 5;
#endif

  img->lock();

  buffer = (UCHAR *)img->getRawData();
  CREATE_STACK
  for (j = 1, y = y0; y <= ylast; j++, y++)
    for (i = 1, x = x0, pix = buffer + x0 + y * Pix_ystep; x <= xlast;
         i++, x++, pix++)
      if (NOT_DONE(i, j) && IS_BLACK_GR8(pix)) {
        CLEARBIG(Xsum);
        CLEARBIG(Ysum);
        CLEARBIG(Weightsum);
        Xmin = Xmax = x;
        Ymin = Ymax = y;
        Npix        = 0;
#ifdef RECURSIVE_VERSION
        Level = 0;
#endif
        visit_gr8(i, j, x, y, pix);
        dot_lx = Xmax - Xmin + 1;
        dot_ly = Ymax - Ymin + 1;
        if (Npix < max_area * 3 / 2 && dot_lx > 3 && dot_lx < (xsize >> 1) &&
            dot_ly > 3 && dot_ly < (ysize >> 1) && Xmin > x0 && Xmax < xlast &&
            Ymin > y0 && Ymax < ylast) {
          dot_x = (float)(BIG_TO_DOUBLE(Xsum) / BIG_TO_DOUBLE(Weightsum));
          dot_y = (float)(BIG_TO_DOUBLE(Ysum) / BIG_TO_DOUBLE(Weightsum));
          if (vertical) {
            for (ins = 0; ins < n_dots; ins++)
              if (dotarray[ins].y > dot_y) break;
          } else {
            for (ins = 0; ins < n_dots; ins++)
              if (dotarray[ins].x > dot_x) break;
          }
          for (shift            = n_dots - 1; shift >= ins; shift--)
            dotarray[shift + 1] = dotarray[shift];
          n_dots++;
          dotarray[ins].x1   = Xmin;
          dotarray[ins].x2   = Xmax;
          dotarray[ins].y1   = Ymin;
          dotarray[ins].y2   = Ymax;
          dotarray[ins].lx   = dot_lx;
          dotarray[ins].ly   = dot_ly;
          dotarray[ins].area = Npix;
          dotarray[ins].x    = dot_x;
          dotarray[ins].y    = dot_y;
          if (n_dots >= dotarray_size) goto end_loop;
        }
      }
end_loop:
  DESTROY_STACK

  free(Done);
  Done = 0;
  img->unlock();
  return n_dots;
}

/*---------------------------------------------------------------------------*/

static int find_dots_rgb(const TRaster32P &img, int strip_width,
                         PEGS_SIDE pegs_side, DOT dotarray[], int dotarray_size,
                         int max_area) {
  int n_dots, ins, shift;
  int x, y; /* coords in img coord system */
  int i, j; /* coords in done coord system */
  int x0, y0, xsize, ysize, xlast, ylast;
  TPixel32 *pix, *buffer;
  int dot_lx, dot_ly;
  float dot_x, dot_y;
  bool vertical;

  assert(img->getPixelSize() ==
         4); /*questo e' per ricordare che l'algo e' per RGB*/
  switch (pegs_side) {
  case PEGS_BOTTOM:
  case PEGS_TOP:
    x0       = 0;
    y0       = pegs_side == PEGS_BOTTOM ? 0 : img->getLy() - strip_width;
    xsize    = img->getLx();
    ysize    = strip_width;
    vertical = false;
    break;
  case PEGS_LEFT:
  case PEGS_RIGHT:
    x0       = pegs_side == PEGS_LEFT ? 0 : img->getLx() - strip_width;
    y0       = 0;
    xsize    = strip_width;
    ysize    = img->getLy();
    vertical = true;
    break;
  default: {
    std::ostringstream os;
    os << "find dots internal error: pegs_side = " << std::hex << pegs_side
       << '\0';
    throw TCleanupException(os.str().c_str());
    x0 = y0 = xsize = ysize = 0;
    vertical = false;
    break;
  }
  }
  xlast        = x0 + xsize - 1;
  ylast        = y0 + ysize - 1;
  n_dots       = 0;
  Done_rowsize = xsize + 2;
  Done_colsize = ysize + 2;
  Done = (char *)calloc((size_t)((Done_rowsize * Done_colsize + 7) >> 3),
                        sizeof(char));
  if (!Done) {
    throw TCleanupException("find_dots: out of memory");
  }
  for (i = 0; i < Done_rowsize; i++) {
    SET_DONE(i, 0);
    SET_DONE(i, Done_colsize - 1);
  }
  for (j = 0; j < Done_colsize; j++) {
    SET_DONE(0, j);
    SET_DONE(Done_rowsize - 1, j);
  }
  buffer    = img->pixels();
  Pix_ystep = img->getWrap();
  if (Debug_flag) {
    printf("Zona di scansione: (%d,%d) -- (%d,%d)\n", x0, y0, xlast, ylast);
    printf("wrap: %d\n", img->getWrap());
  }
#ifdef RECURSIVE_VERSION
  Max_level = max_area * 6 / 5;
#endif

  CREATE_STACK
  for (j = 1, y = y0; y <= ylast; j++, y++)
    for (i = 1, x = x0, pix = buffer + x0 + y * Pix_ystep; x <= xlast;
         i++, x++, pix++)
      if (NOT_DONE(i, j) && IS_BLACK_RGB(pix)) {
        CLEARBIG(Xsum);
        CLEARBIG(Ysum);
        CLEARBIG(Weightsum);
        Xmin = Xmax = x;
        Ymin = Ymax = y;
        Npix        = 0;
#ifdef RECURSIVE_VERSION
        Level = 0;
#endif
        visit_rgb(i, j, x, y, pix);
        dot_lx = Xmax - Xmin + 1;
        dot_ly = Ymax - Ymin + 1;
        if (Npix < max_area * 3 / 2 && dot_lx > 3 && dot_lx < (xsize >> 1) &&
            dot_ly > 3 && dot_ly < (ysize >> 1) && Xmin > x0 && Xmax <= xlast &&
            Ymin > y0 && Ymax <= ylast) {
          dot_x = (float)(BIG_TO_DOUBLE(Xsum) / BIG_TO_DOUBLE(Weightsum));
          dot_y = (float)(BIG_TO_DOUBLE(Ysum) / BIG_TO_DOUBLE(Weightsum));
          if (vertical) {
            for (ins = 0; ins < n_dots; ins++)
              if (dotarray[ins].y > dot_y) break;
          } else {
            for (ins = 0; ins < n_dots; ins++)
              if (dotarray[ins].x > dot_x) break;
          }
          for (shift            = n_dots - 1; shift >= ins; shift--)
            dotarray[shift + 1] = dotarray[shift];
          n_dots++;
          dotarray[ins].x1   = Xmin;
          dotarray[ins].x2   = Xmax;
          dotarray[ins].y1   = Ymin;
          dotarray[ins].y2   = Ymax;
          dotarray[ins].lx   = dot_lx;
          dotarray[ins].ly   = dot_ly;
          dotarray[ins].area = Npix;
          dotarray[ins].x    = dot_x;
          dotarray[ins].y    = dot_y;
          if (n_dots >= dotarray_size) goto end_loop;
        }
      }
end_loop:
  DESTROY_STACK
  free(Done);
  Done = 0;
  return n_dots;
}

/*---------------------------------------------------------------------------*/
#ifdef DAFARE
static void visit_bw(int i, int j, int x, int y, int bit, UCHAR *byte) {
  int ret, next_bit, prev_bit;
  UCHAR *next_byte, *prev_byte;

start:
  ADDBIG(Xsum, x);
  ADDBIG(Ysum, y);
  ADDBIG(Weightsum, 1);
  if (x < Xmin) Xmin = x;
  if (x > Xmax) Xmax = x;
  if (y < Ymin) Ymin = y;
  if (y > Ymax) Ymax = y;
  Npix++;
  SET_DONE(i, j);

  next_bit = bit - 1;
  if (next_bit < 0) {
    next_bit  = 7;
    next_byte = byte + 1;
  } else
    next_byte = byte;
  if (NOT_DONE(i + 1, j) && ((*next_byte >> next_bit) & 1) == Black_pixel) {
    PUSH_ONTO_STACK(1, x, y, i, j, bit, byte)
    i++;
    x++;
    bit  = next_bit;
    byte = next_byte;
    goto start;
  return_1:;
  }
  prev_bit = bit + 1;
  if (prev_bit > 7) {
    prev_bit  = 0;
    prev_byte = byte - 1;
  } else
    prev_byte = byte;
  if (NOT_DONE(i - 1, j) && ((*prev_byte >> prev_bit) & 1) == Black_pixel) {
    PUSH_ONTO_STACK(2, x, y, i, j, bit, byte)
    i--;
    x--;
    bit  = prev_bit;
    byte = prev_byte;
    goto start;
  return_2:;
  }
  if (NOT_DONE(i, j + 1) && ((*(byte + Pix_ystep) >> bit) & 1) == Black_pixel) {
    PUSH_ONTO_STACK(3, x, y, i, j, bit, byte)
    j++;
    y++;
    byte += Pix_ystep;
    goto start;
  return_3:;
  }
  if (NOT_DONE(i, j - 1) && ((*(byte - Pix_ystep) >> bit) & 1) == Black_pixel) {
    PUSH_ONTO_STACK(4, x, y, i, j, bit, byte)
    j--;
    y--;
    byte -= Pix_ystep;
    goto start;
  return_4:;
  }
  if (!STACK_IS_EMPTY) {
    POP_FROM_STACK_U(ret, x, y, i, j, bit, byte);
    switch (ret) {
    case 1:
      goto return_1;
    case 2:
      goto return_2;
    case 3:
      goto return_3;
    case 4:
      goto return_4;
    default:
      abort();
    }
  }
}
#endif
/*---------------------------------------------------------------------------*/

#ifdef RECURSIVE_VERSION

static void visit_bw(int i, int j, int x, int y, int bit, UCHAR *byte) {
  int next_bit, prev_bit;
  UCHAR *next_byte, *prev_byte;

  if (Level >= Max_level)
    return;
  else
    Level++;

  ADDBIG(Xsum, x);
  ADDBIG(Ysum, y);
  ADDBIG(Weightsum, 1);

  if (x < Xmin) Xmin = x;
  if (x > Xmax) Xmax = x;
  if (y < Ymin) Ymin = y;
  if (y > Ymax) Ymax = y;
  Npix++;
  SET_DONE(i, j);

  next_bit = bit - 1;
  if (next_bit < 0) {
    next_byte = byte + 1;
    next_bit  = 7;
  } else
    next_byte = byte;

  prev_bit = bit + 1;
  if (prev_bit > 7) {
    prev_byte = byte - 1;
    prev_bit  = 0;
  } else
    prev_byte = byte;

  if (NOT_DONE(i + 1, j) && ((*next_byte >> next_bit) & 1) == Black_pixel)
    visit_bw(i + 1, j, x + 1, y, next_bit, next_byte);

  if (NOT_DONE(i - 1, j) && ((*prev_byte >> prev_bit) & 1) == Black_pixel)
    visit_bw(i - 1, j, x - 1, y, prev_bit, prev_byte);

  if (NOT_DONE(i, j + 1) && ((*(byte + Pix_ystep) >> bit) & 1) == Black_pixel)
    visit_bw(i, j + 1, x, y + 1, bit, byte + Pix_ystep);

  if (NOT_DONE(i, j - 1) && ((*(byte - Pix_ystep) >> bit) & 1) == Black_pixel)
    visit_bw(i, j - 1, x, y - 1, bit, byte - Pix_ystep);

  Level--;
}

#endif

/*---------------------------------------------------------------------------*/

static void visit_gr8(int i, int j, int x, int y, UCHAR *pix) {
  int weight, ret, dummy;

start:
  weight = BLACK_WEIGHT_GR8(pix);
  ADDBIG(Xsum, x * weight);
  ADDBIG(Ysum, y * weight);
  ADDBIG(Weightsum, weight);
  if (IS_VERY_BLACK_GR8(pix)) Very_black_found = true;
  if (x < Xmin) Xmin                           = x;
  if (x > Xmax) Xmax                           = x;
  if (y < Ymin) Ymin                           = y;
  if (y > Ymax) Ymax                           = y;
  Npix++;
  SET_DONE(i, j);

  if (NOT_DONE(i + 1, j) && IS_BLACK_GR8(pix + 1)) {
    PUSH_ONTO_STACK(1, x, y, i, j, 0, pix)
    i++;
    x++;
    pix++;
    goto start;
  return_1:;
  }
  if (NOT_DONE(i - 1, j) && IS_BLACK_GR8(pix - 1)) {
    PUSH_ONTO_STACK(2, x, y, i, j, 0, pix)
    i--;
    x--;
    pix--;
    goto start;
  return_2:;
  }
  if (NOT_DONE(i, j + 1) && IS_BLACK_GR8(pix + Pix_ystep)) {
    PUSH_ONTO_STACK(3, x, y, i, j, 0, pix)
    j++;
    y++;
    pix += Pix_ystep;
    goto start;
  return_3:;
  }
  if (NOT_DONE(i, j - 1) && IS_BLACK_GR8(pix - Pix_ystep)) {
    PUSH_ONTO_STACK(4, x, y, i, j, 0, pix)
    j--;
    y--;
    pix -= Pix_ystep;
    goto start;
  return_4:;
  }
  if (!STACK_IS_EMPTY) {
    POP_FROM_STACK_U(ret, x, y, i, j, dummy, pix);
    switch (ret) {
    case 1:
      goto return_1;
    case 2:
      goto return_2;
    case 3:
      goto return_3;
    case 4:
      goto return_4;
    default:
      abort();
    }
  }
}

/*---------------------------------------------------------------------------*/

#ifdef RECURSIVE_VERSION

static void visit_gr8(int i, int j, int x, int y, UCHAR *pix) {
  int weight;

  if (Level >= Max_level)
    return;
  else
    Level++;

  weight = BLACK_WEIGHT_GR8(pix);
  ADDBIG(Xsum, x * weight);
  ADDBIG(Ysum, y * weight);
  ADDBIG(Weightsum, weight);
  if (IS_VERY_BLACK_GR8(pix)) Very_black_found = true;
  if (x < Xmin) Xmin                           = x;
  if (x > Xmax) Xmax                           = x;
  if (y < Ymin) Ymin                           = y;
  if (y > Ymax) Ymax                           = y;
  Npix++;
  SET_DONE(i, j);

  if (NOT_DONE(i + 1, j) && IS_BLACK_GR8(pix + 1))
    visit_gr8(i + 1, j, x + 1, y, pix + 1);
  if (NOT_DONE(i - 1, j) && IS_BLACK_GR8(pix - 1))
    visit_gr8(i - 1, j, x - 1, y, pix - 1);
  if (NOT_DONE(i, j + 1) && IS_BLACK_GR8(pix + Pix_ystep))
    visit_gr8(i, j + 1, x, y + 1, pix + Pix_ystep);
  if (NOT_DONE(i, j - 1) && IS_BLACK_GR8(pix - Pix_ystep))
    visit_gr8(i, j - 1, x, y - 1, pix - Pix_ystep);

  Level--;
}

#endif

/*---------------------------------------------------------------------------*/

static void visit_rgb(int i, int j, int x, int y, TPixel32 *pix) {
  int weight, ret, dummy;

start:
  weight = BLACK_WEIGHT_RGB(pix);
  ADDBIG(Xsum, x * weight);
  ADDBIG(Ysum, y * weight);
  ADDBIG(Weightsum, weight);
  if (IS_VERY_BLACK_RGB(pix)) Very_black_found = true;
  if (x < Xmin) Xmin                           = x;
  if (x > Xmax) Xmax                           = x;
  if (y < Ymin) Ymin                           = y;
  if (y > Ymax) Ymax                           = y;
  Npix++;
  SET_DONE(i, j);

  if (NOT_DONE(i + 1, j) && IS_BLACK_RGB(pix + 1)) {
    PUSH_ONTO_STACK(1, x, y, i, j, 0, pix)
    i++;
    x++;
    pix += 1;
    goto start;
  return_1:;
  }
  if (NOT_DONE(i - 1, j) && IS_BLACK_RGB(pix - 1)) {
    PUSH_ONTO_STACK(2, x, y, i, j, 0, pix)
    i--;
    x--;
    pix -= 1;
    goto start;
  return_2:;
  }
  if (NOT_DONE(i, j + 1) && IS_BLACK_RGB(pix + Pix_ystep)) {
    PUSH_ONTO_STACK(3, x, y, i, j, 0, pix)
    j++;
    y++;
    pix += Pix_ystep;
    goto start;
  return_3:;
  }
  if (NOT_DONE(i, j - 1) && IS_BLACK_RGB(pix - Pix_ystep)) {
    PUSH_ONTO_STACK(4, x, y, i, j, 0, pix)
    j--;
    y--;
    pix -= Pix_ystep;
    goto start;
  return_4:;
  }
  if (!STACK_IS_EMPTY) {
    POP_FROM_STACK_TPIXEL32(ret, x, y, i, j, dummy, pix);
    switch (ret) {
    case 1:
      goto return_1;
    case 2:
      goto return_2;
    case 3:
      goto return_3;
    case 4:
      goto return_4;
    default:
      abort();
    }
  }
}

/*---------------------------------------------------------------------------*/

#ifdef RECURSIVE_VERSION

static void visit_rgb(int i, int j, int x, int y, TPixel32 *pix) {
  int weight;

  if (Level >= Max_level)
    return;
  else
    Level++;

  weight = BLACK_WEIGHT_RGB(pix);
  ADDBIG(Xsum, x * weight);
  ADDBIG(Ysum, y * weight);
  ADDBIG(Weightsum, weight);
  if (IS_VERY_BLACK_RGB(pix)) Very_black_found = true;
  if (x < Xmin) Xmin                           = x;
  if (x > Xmax) Xmax                           = x;
  if (y < Ymin) Ymin                           = y;
  if (y > Ymax) Ymax                           = y;
  Npix++;
  SET_DONE(i, j);

  if (NOT_DONE(i + 1, j) && IS_BLACK_RGB(pix + 1))
    visit_rgb(i + 1, j, x + 1, y, pix + 1);
  if (NOT_DONE(i - 1, j) && IS_BLACK_RGB(pix - 1))
    visit_rgb(i - 1, j, x - 1, y, pix - 1);
  if (NOT_DONE(i, j + 1) && IS_BLACK_RGB(pix + Pix_ystep))
    visit_rgb(i, j + 1, x, y + 1, pix + Pix_ystep);
  if (NOT_DONE(i, j - 1) && IS_BLACK_RGB(pix - Pix_ystep))
    visit_rgb(i, j - 1, x, y - 1, pix - Pix_ystep);

  Level--;
}

#endif

#define PERCENT (40.0 / 100.0)
/*---------------------------------------------------------------------------*/
/*
 * Attenzione: tutti i controlli e i calcoli vengono fatti in pixel.
 * Quindi bisogna convertire tutti i valori in pixel prima di arrivare a
 * questo livello.
 * Inoltre: la pegs_side si riferisce alle coordinate di raster.
 */
bool get_image_rotation_and_center(const TRasterP &img, int strip_width,
                                  PEGS_SIDE pegs_side, double *p_ang,
                                  double *cx, double *cy, DOT ref[],
                                  int ref_dot) {
  double angle;
  int i;
  bool found;
  float dx, dy;
  DOT _dotarray[MAX_DOT];
  DOT *dotarray = _dotarray;
  int ndot;
  int max_area, min_area;

  *p_ang = 0.0;

  if (Debug_flag) {
    for (i = 0; i < ref_dot; i++) {
      printf("Reference dot <%d>\n", i);
      stampa_dot(ref + i);
    }
  }

  max_area = 0;
  if (ref_dot > 0) {
    min_area = ref[0].area;
  }
  for (i = 0; i < ref_dot; i++) {
    if (ref[i].area > max_area) {
      max_area = ref[i].area;
    }
    if (ref[i].area < min_area) {
      min_area = ref[i].area;
    }
  }

  ndot = find_dots(img, strip_width, pegs_side, dotarray, MAX_DOT, max_area);
  if (Debug_flag) printf(">>>> %d dots found\n", ndot);

  i = 0;
  while (i < ndot)  // elimino i dots troppo piccoli
  {
    if (dotarray[i].area < min_area * PERCENT) {
      for (int j = i; j < ndot - 1; j++) dotarray[j] = dotarray[j + 1];
      ndot--;
    } else
      i++;
  }

  /* controllo il pattern delle perforazioni  */
  if (ndot <= 1) {
    return false;
  }

  int indexArray[3] = {0, 1, 2};
  found             = compare_dots(dotarray, ndot, ref, ref_dot, indexArray[0],
                       indexArray[1], indexArray[2]);

  if (Debug_flag)
    for (i = 0; i < ndot; i++) {
      printf("**** Dot[%d]\n", i);
      stampa_dot(dotarray + i);
    }

  if (!found) return false;

  angle = 0;
  for (i = 0; i < 2; i++) {
    dx = dotarray[indexArray[i + 1]].x - dotarray[indexArray[i]].x;
    dy = dotarray[indexArray[i + 1]].y - dotarray[indexArray[i]].y;
    switch (pegs_side) {
    case PEGS_LEFT:
    case PEGS_RIGHT:
      angle += dy == 0.0 ? M_PI_2 : atan(dx / dy);
      break;
    default:
      angle -= dx == 0.0 ? M_PI_2 : atan(dy / dx);
      break;
    }
  }

  *p_ang = angle / 2;

  // Now calculate the center, we have to get the offset of the center for the
  // dot at point indexArray[1]
  // from the reference and then use the angle to calculate the offsets for the
  // center.
  //
  // It is assumed, that the holes are all on one line.
  float pegWidth =
      sqrt((ref[ref_dot - 1].x - ref[0].x) * (ref[ref_dot - 1].x - ref[0].x) +
           (ref[ref_dot - 1].y - ref[0].y) * (ref[ref_dot - 1].y - ref[0].y));
  float refPegOffset = sqrt(
      (ref[indexArray[1]].x - ref[0].x) * (ref[indexArray[1]].x - ref[0].x) +
      (ref[indexArray[1]].y - ref[0].y) * (ref[indexArray[1]].y - ref[0].y));
  *cx = dotarray[indexArray[1]].x +
        cos(*p_ang) * (pegWidth / 2.0f - refPegOffset);
  *cy = dotarray[indexArray[1]].y +
        sin(*p_ang) * (pegWidth / 2.0f - refPegOffset);

  if (Debug_flag) {
    printf("\nang: %g\ncx : %g\ncy : %g\n\n", *p_ang, *cx, *cy);
  }

  return true;
}

/*---------------------------------------------------------------------------*/
#define MIN_V 100.0

static bool compare_dots(DOT const dots[], int ndots, DOT reference[],
                        int ref_dot, int &i_ok, int &j_ok, int &k_ok) {
  int found;
  int toll;
  float tolld;
  int i, j, k;
  bool *dot_ok   = 0;
  float *ref_dis = 0, dx, dy;
  float vmin, v, dist_i_j, dist_i_k, dist_j_k, del1, del2;
  float ref_dis_0_1, ref_dis_1_2;

  i_ok = 0;
  j_ok = 0;
  k_ok = 0;

  /* questa funz e' indipendente da posizione e orientamento dei dots */

  if (ndots < 1 || ref_dot < 1) {
    goto error;
  }

  /* controllo quanti dots sono realmente buoni per il confronto */
  dot_ok = (bool *)calloc(ndots, sizeof(bool));
  found  = 0;

  for (i = 0; i < ndots; i++) {
    dot_ok[i] = false;
    for (j = 0; j < ref_dot; j++) {
      toll = (int)((float)reference[j].area * PERCENT);
      if (abs(dots[i].area - reference[j].area) < toll) {
        dot_ok[i] = true;
        found++;
        break;
      }
    }
  }

  if (!found) {
    goto error;
  }

  ref_dis = (float *)calloc(ref_dot, sizeof(float));

  /* calcolo le distanze di riferimento e la tolleranza stessa */

  tolld                              = (float)reference[0].lx;
  if (tolld < reference[0].ly) tolld = (float)reference[0].ly;
  for (i = 1; i < ref_dot; i++) {
    dx                                 = reference[0].x - reference[i].x;
    dy                                 = reference[0].y - reference[i].y;
    ref_dis[i - 1]                     = sqrtf((dx * dx) + (dy * dy));
    if (tolld < reference[i].lx) tolld = (float)reference[i].lx;
    if (tolld < reference[i].ly) tolld = (float)reference[i].ly;
  }

  // I suspect that the following expects symmetry in the holes layout...
  // Besides, if the layout is not symmetric, the peg is supposed to be rotated
  // OR translated
  // switching from, say, top to bottom? (Daniele)

  i_ok = -1;
  v = vmin = 10000000.0;
  for (i = 0; i < ndots - 2; i++) {
    if (!dot_ok[i]) continue;

    for (j = i + 1; j < ndots - 1; j++) {
      if (!dot_ok[j]) continue;
      for (k = j + 1; k < ndots; k++) {
        if (!dot_ok[k]) continue;

        // Build square discrepancies from the reference relative hole distances
        dx       = dots[i].x - dots[j].x;
        dy       = dots[i].y - dots[j].y;
        dist_i_j = sqrtf((dx * dx) + (dy * dy));
        dx       = dots[i].x - dots[k].x;
        dy       = dots[i].y - dots[k].y;
        dist_i_k = sqrtf((dx * dx) + (dy * dy));
        del1     = (dist_i_j - ref_dis[0]);
        del2     = (dist_i_k - ref_dis[1]);
        v        = ((del1 * del1) + (del2 * del2));

        // Furthermore, add discrepancies from the reference hole areas
        v += abs(dots[i].area -
                 reference[0].area);  // fabs since areas are already squared
        v += abs(dots[j].area - reference[1].area);
        v += abs(dots[k].area - reference[2].area);

        if (v < vmin) {
          i_ok = i;
          j_ok = j;
          k_ok = k;
          vmin = v;
        }
      }
    }
  }

  if (Debug_flag) {
    printf("Ho trovato v = %f su %f per %d %d %d \n", v, vmin, i_ok, j_ok,
           k_ok);
    printf("----  Dot <%d>  ----\n", i_ok);
    stampa_dot(dots + i_ok);
    printf("----  Dot <%d>  ----\n", j_ok);
    stampa_dot(dots + j_ok);
    printf("----  Dot <%d>  ----\n", k_ok);
    stampa_dot(dots + k_ok);
  }

  if (i_ok < 0)
    goto error;
  else {
    dx       = dots[i_ok].x - dots[j_ok].x;
    dy       = dots[i_ok].y - dots[j_ok].y;
    dist_i_j = sqrtf((dx * dx) + (dy * dy));

    dx       = dots[k_ok].x - dots[j_ok].x;
    dy       = dots[k_ok].y - dots[j_ok].y;
    dist_j_k = sqrtf((dx * dx) + (dy * dy));

    ref_dis_0_1 = ref_dis[0];

    dx          = reference[1].x - reference[2].x;
    dy          = reference[1].y - reference[2].y;
    ref_dis_1_2 = sqrtf((dx * dx) + (dy * dy));

    if (fabsf(dist_i_j - ref_dis_0_1) >= tolld ||
        fabsf(dist_j_k - ref_dis_1_2) >= tolld) {
      i_ok = 0;
      j_ok = 1;
      k_ok = 2;
    }
  }

  if (ref_dis) free(ref_dis);
  if (dot_ok) free(dot_ok);

  return true;

error:
  if (ref_dis) free(ref_dis);
  if (dot_ok) free(dot_ok);
  return false;
}
/*---------------------------------------------------------------------------*/

static void stampa_dot(DOT const *dot) {
  printf("Dimensioni: %d,\t%d\n", dot->lx, dot->ly);
  printf("Start     : %d,\t%d\n", dot->x1, dot->y1);
  printf("End       : %d,\t%d\n", dot->x2, dot->y2);
  printf("Baricentro: %5.3f,\t%5.3f\n", dot->x, dot->y);
  printf("Area      : %d\n", dot->area);
}
