

/*
guardare DAFARE
write_lut_image ??? quando ?

*/

#include "autoadjust.h"
#include "cleanupcommon.h"

#define TO_ABS(x)                                                              \
  {                                                                            \
    if ((x) < 0) (x) = -(x);                                                   \
  }

typedef struct big { UINT lo, hi; } BIG;
#define CLEAR_BIG(B) ((B).lo = 0, (B).hi = 0, (B))
#define ADD_BIG(B, X)                                                          \
  ((B).lo += (UINT)(X), (B).hi += (B).lo >> 30, (B).lo &= 0x3fffffff, (B))
#define ADD_BIG_BIG(B1, B2)                                                    \
  ((B1).lo += (B2).lo, (B1).hi += (B2).hi, (B1).hi += (B1).lo >> 30,           \
   (B1).lo &= 0x3fffffff, (B1))
#define BIG_TO_DOUBLE(B) ((double)(B).hi * (double)0x40000000 + (double)(B).lo)

static int Black = 0;

static int Ref_cum[256];
static int Ref_edgelen;

/*---------------------------------------------------------------------------*/

void autoadj_set_black_value(int black) { Black = black; }

/*===========================================================================*/

static void smooth_func256(float func[256], int rad) {
  int i, j, k;
  float smooth[256];

  for (i = 0; i < 256; i++) {
    k = i - rad;
    notLessThan(0, k);
    smooth[i] = func[k] / 2;
    k         = i + rad;
    notMoreThan(255, k);
    smooth[i] += func[k] / 2;
    for (j = i - rad + 1; j <= i + rad - 1; j++) {
      k = j;
      notLessThan(0, k);
      notMoreThan(255, k);
      smooth[i] += func[k];
    }
    smooth[i] /= 2 * rad;
  }
  for (i = 0; i < 256; i++) func[i] = smooth[i];
}

/*---------------------------------------------------------------------------*/

static void build_cum(int histo[256], int cum[256]) {
  int i;

  cum[0] = histo[0];
  for (i = 1; i < 256; i++) cum[i] = cum[i - 1] + histo[i];
}

/*===========================================================================*/

static int Window_x0, Window_y0, Window_x1, Window_y1;

void set_autoadjust_window(int x0, int y0, int x1, int y1) {
  Window_x0 = x0;
  Window_y0 = y0;
  Window_x1 = x1;
  Window_y1 = y1;
  if (Window_x0 > Window_x1) std::swap(Window_x0, Window_x1);
  if (Window_y0 > Window_y1) std::swap(Window_y0, Window_y1);
}

/*---------------------------------------------------------------------------*/

static void get_virtual_buffer(const TRasterImageP &image, int *p_lx, int *p_ly,
                               int *p_wrap, UCHAR **p_buffer) {
  int x0, y0, x1, y1;
  int x_margin, y_margin;
  int lx, ly, wrap;
  UCHAR *buffer;

  TRasterGR8P ras8(image->getRaster());

  assert(ras8);
  double xdpi, ydpi;
  image->getDpi(xdpi, ydpi);

  /* BORDO DI MEZZO CENTIMETRO */
  x_margin = troundp(mmToPixel(5.0, xdpi));
  y_margin = troundp(mmToPixel(5.0, ydpi));
  x0       = Window_x0 + x_margin;
  y0       = Window_y0 + y_margin;
  x1       = Window_x1 - x_margin;
  y1       = Window_y1 - y_margin;
  notLessThan(x0 + 9, x1);
  notLessThan(y0 + 9, y1);
  notLessThan(0, x0);
  notMoreThan(ras8->getLx() - 1, x0);
  notLessThan(0, y0);
  notMoreThan(ras8->getLy() - 1, y0);
  notLessThan(0, x1);
  notMoreThan(ras8->getLx() - 1, x1);
  notLessThan(0, y1);
  notMoreThan(ras8->getLy() - 1, y1);

  lx     = x1 - x0 + 1;
  ly     = y1 - y0 + 1;
  wrap   = ras8->getWrap();
  buffer = (UCHAR *)ras8->getRawData() + x0 + y0 * wrap;

  *p_lx     = lx;
  *p_ly     = ly;
  *p_wrap   = wrap;
  *p_buffer = buffer;
}

/*===========================================================================*/

void black_eq_algo(const TRasterImageP &image) {
  int lx, ly, wrap;
  int x, y, grey /*, width*/;
  int d_histo[256][256];
  int d, dd, m;
  UCHAR *buffer, *pix, *north, *south;
  UCHAR prev, darkest;
  long n;
  float mean_d[256];
  BIG s;
  int max_d_grey;
  float max_d;
  float fac;
  int val;
  UCHAR lut[256];
  image->getRaster()->lock();

  get_virtual_buffer(image, &lx, &ly, &wrap, &buffer);

  for (grey = 0; grey < 256; grey++) {
    for (d = 0; d < 256; d++) d_histo[grey][d] = 0;
  }
  for (y = 1; y < ly - 1; y++) {
    pix   = buffer + y * wrap + 1;
    north = pix + wrap;
    south = pix - wrap;
    for (x = 1; x < lx - 1; x++, pix++, north++, south++) {
      m = 2 * ((int)north[0] + (int)pix[-1] + 2 * (int)pix[0] + (int)pix[1] +
               (int)south[0]) +
          (int)north[-1] + (int)north[1] + (int)south[-1] + (int)south[1];
      m = (m + 8) >> 4;
      d = (int)north[-1] + (int)north[0] * 2 + (int)north[1] - (int)south[-1] -
          (int)south[0] * 2 - (int)south[1];
      TO_ABS(d)
      dd = (int)north[-1] + (int)pix[-1] * 2 + (int)south[-1] - (int)north[1] -
           (int)pix[1] * 2 - (int)south[1];
      TO_ABS(dd)
      if (dd > d) d = dd;
      dd = (int)pix[-1] + (int)north[-1] * 2 + (int)north[0] - (int)pix[1] -
           (int)south[1] * 2 - (int)south[0];
      TO_ABS(dd)
      if (dd > d) d = dd;
      dd = (int)pix[1] + (int)north[1] * 2 + (int)north[0] - (int)pix[-1] -
           (int)south[-1] * 2 - (int)south[0];
      TO_ABS(dd)
      if (dd > d) d = dd;
      d             = (d + 2) >> 2;
      d_histo[m][d]++;
    }
  }
  for (grey = 0; grey < 256; grey++) {
    n = 0;
    CLEAR_BIG(s);
    for (d = 0; d < 256; d++) {
      ADD_BIG(s, d_histo[grey][d] * d);
      n += d_histo[grey][d];
    }
    if (n)
      mean_d[grey] = (float)(BIG_TO_DOUBLE(s) / n);
    else
      mean_d[grey] = 0;
  }
  smooth_func256(mean_d, 5);
  max_d_grey = 0;
  max_d      = 0.0;
  for (grey = 0; grey < 256; grey++) {
    if (max_d < mean_d[grey]) {
      max_d      = mean_d[grey];
      max_d_grey = grey;
    }
  }

  n = 0;
  CLEAR_BIG(s);
  for (y = 0; y < ly; y++) {
    pix     = buffer + y * wrap;
    prev    = 255;
    darkest = 255;
    for (x = 0; x < lx; x++, pix++) {
      if (*pix < max_d_grey) {
        if (prev < max_d_grey)
          notMoreThan(*pix, darkest);
        else
          darkest = *pix;
      } else {
        if (prev < max_d_grey) {
          ADD_BIG(s, darkest);
          n++;
        }
      }
      prev = *pix;
    }
  }
  darkest = troundp(BIG_TO_DOUBLE(s) / n);

  /*
for (grey = 0; grey < darkest; grey++)
lut[grey] = 0;
fac = (float)255 / (float)(255 - darkest);
for (grey = darkest; grey < 256; grey++)
lut[grey] = 255 - troundp ((255 - grey) * fac);
*/
  notLessThan(0, Black);
  ;
  notMoreThan(255, Black);
  fac = (float)(255 - Black) / (float)(255 - darkest);
  for (grey = 0; grey < 256; grey++) {
    val = 255 - troundp((255 - grey) * fac);
    notLessThan(0, val);
    notMoreThan(255, val);
    lut[grey] = val;
  }

  apply_lut(image, lut);
  image->getRaster()->unlock();
}

/*===========================================================================*/

#define MAX_WIDTH 100

/*---------------------------------------------------------------------------*/

void build_lw(const TRasterImageP &image, float lw[256]) {
  int lx, ly, wrap;
  int x, y, grey, width;
  int d_histo[256][256];
  int d, dd;
  int lw_histo[256][MAX_WIDTH + 1];
  int x_start[256];
  static int *y_start[256];
  static int y_start_alloc = 0;
  UCHAR *buffer, *pix, *north, *south;
  UCHAR cur_grey, x_prev_grey, x_next_grey, y_prev_grey, y_next_grey;
  int x_grad, y_grad;
  long n;
  float mean_d[256];
  BIG s;
  int max_d_grey;
  float max_d;
  long max_d_n;

  get_virtual_buffer(image, &lx, &ly, &wrap, &buffer);

  if (y_start_alloc < lx) {
    for (grey = 0; grey < 256; grey++) {
      delete[] y_start[grey];
      y_start[grey] = new int[lx];
      // TREALLOC (y_start[grey], lx);
    }
    y_start_alloc = lx;
  }
  for (grey = 0; grey < 256; grey++) {
    for (d = 0; d < 256; d++) d_histo[grey][d] = 0;
    for (x = 0; x < lx; x++) y_start[grey][x] = -MAX_WIDTH - 1;
    for (width = 0; width <= MAX_WIDTH; width++) lw_histo[grey][width] = 0;
  }

  for (y = 1; y < ly - 1; y++) {
    for (grey = 0; grey < 256; grey++) x_start[grey] = -MAX_WIDTH - 1;
    pix                                              = buffer + y * wrap + 1;
    north                                            = pix + wrap;
    south                                            = pix - wrap;
    for (x = 1; x < lx - 1; x++, pix++, north++, south++) {
      d = (int)north[-1] + (int)north[0] * 2 + (int)north[1] - (int)south[-1] -
          (int)south[0] * 2 - (int)south[1];
      TO_ABS(d)
      dd = (int)north[-1] + (int)pix[-1] * 2 + (int)south[-1] - (int)north[1] -
           (int)pix[1] * 2 - (int)south[1];
      TO_ABS(dd)
      if (dd > d) d = dd;
      dd = (int)pix[-1] + (int)north[-1] * 2 + (int)north[0] - (int)pix[1] -
           (int)south[1] * 2 - (int)south[0];
      TO_ABS(dd)
      if (dd > d) d = dd;
      dd = (int)pix[1] + (int)north[1] * 2 + (int)north[0] - (int)pix[-1] -
           (int)south[-1] * 2 - (int)south[0];
      TO_ABS(dd)
      if (dd > d) d = dd;
      d >>= 3;
      d_histo[*pix][d]++;

      cur_grey    = pix[0];
      x_prev_grey = pix[-1];
      x_next_grey = pix[1];
      y_prev_grey = south[0];
      y_next_grey = north[0];
      x_grad      = (int)x_next_grey - (int)x_prev_grey;
      y_grad      = (int)y_next_grey - (int)y_prev_grey;
      if (cur_grey < x_prev_grey) {
        if (x_grad < 0 && x_grad < y_grad && x_grad < -y_grad)
          for (grey = cur_grey; grey < x_prev_grey; grey++) x_start[grey] = x;
        else
          for (grey       = cur_grey; grey < x_prev_grey; grey++)
            x_start[grey] = -MAX_WIDTH - 1;
      } else if (cur_grey > x_prev_grey) {
        if (x_grad > 0 && x_grad > y_grad && x_grad > -y_grad)
          for (grey = x_prev_grey; grey < cur_grey; grey++) {
            width = x - x_start[grey];
            if (width <= MAX_WIDTH) lw_histo[grey][width]++;
          }
      }
      if (cur_grey < y_prev_grey) {
        if (y_grad < 0 && y_grad < x_grad && y_grad < -x_grad)
          for (grey          = cur_grey; grey < y_prev_grey; grey++)
            y_start[grey][x] = y;
        else
          for (grey          = cur_grey; grey < y_prev_grey; grey++)
            y_start[grey][x] = -MAX_WIDTH - 1;
      } else if (cur_grey > y_prev_grey) {
        if (y_grad > 0 && y_grad > x_grad && y_grad > -x_grad)
          for (grey = y_prev_grey; grey < cur_grey; grey++) {
            width = y - y_start[grey][x];
            if (width <= MAX_WIDTH) lw_histo[grey][width]++;
          }
      }
    }
  }
  for (grey = 0; grey < 256; grey++) {
    n = 0;
    CLEAR_BIG(s);
    for (width = 0; width <= MAX_WIDTH; width++) {
      ADD_BIG(s, lw_histo[grey][width] * width);
      n += lw_histo[grey][width];
    }
    if (n)
      lw[grey] = (float)(BIG_TO_DOUBLE(s) / n);
    else
      lw[grey] = 0.0;
  }
  max_d_grey = 0;
  max_d      = 0.0;
  for (grey = 0; grey < 256; grey++) {
    n = 0;
    CLEAR_BIG(s);
    for (d = 0; d < 256; d++) {
      ADD_BIG(s, d_histo[grey][d] * d);
      n += d_histo[grey][d];
    }
    if (n)
      mean_d[grey] = (float)(BIG_TO_DOUBLE(s) / n);
    else
      mean_d[grey] = 0.0;
    if (max_d < mean_d[grey]) {
      max_d      = mean_d[grey];
      max_d_grey = grey;
    }
  }

  max_d_n = 0;
  for (width = 0; width <= MAX_WIDTH; width++)
    max_d_n += lw_histo[max_d_grey][width];
  for (grey = max_d_grey - 1; grey >= 0; grey--) {
    if (!lw[grey]) break;
    n = 0;
    for (width = 0; width <= MAX_WIDTH; width++) n += lw_histo[grey][width];
    if (n < max_d_n / 10) break;
  }
  for (; grey >= 0; grey--) lw[grey] = 0.0;
  for (grey = max_d_grey + 1; grey < 256; grey++) {
    if (!lw[grey]) break;
    if (255 - grey < (255 - max_d_grey) / 2) break;
    if (mean_d[grey] < max_d / 2.0) break;
  }
  for (; grey < 256; grey++) lw[grey] = 0.0;
}

/*---------------------------------------------------------------------------*/

void build_lw_lut(float ref_lw[256], float lw[256], UCHAR lut[256]) {
  /* crea una lut tale che l'immagine con il profilo di linewidths lw
venga mappata in un'immagine con il profilo di riferimento ref_lw.
Le lw sono non decrescenti e delimitate da 0.0 li' dove non sono valide
*/

  int i, j;
  float bot_ref_lw, top_ref_lw, bot_lw, top_lw;
  int bot_ref_gr, top_ref_gr, bot_gr, top_gr;
  float min_lw, max_lw;
  int min_ref_gr, max_ref_gr, min_gr, max_gr;
  float fac;

  for (i = 0; !ref_lw[i]; i++) {
  }
  bot_ref_lw = ref_lw[i];
  bot_ref_gr = i;
  for (i = 255; !ref_lw[i]; i--) {
  }
  top_ref_lw = ref_lw[i];
  top_ref_gr = i;
  for (i = 0; !lw[i]; i++) {
  }
  bot_lw = lw[i];
  bot_gr = i;
  for (i = 255; !lw[i]; i--) {
  }
  top_lw = lw[i];
  top_gr = i;

  min_lw = std::max(bot_ref_lw, bot_lw);
  max_lw = std::min(top_ref_lw, top_lw);

  if (min_lw >= max_lw) {
    for (i = 0; i < 256; i++) lut[i] = i;
    return;
  }

  for (i = bot_ref_gr; ref_lw[i] < min_lw; i++) {
  }
  min_ref_gr = i;
  for (i = top_ref_gr; ref_lw[i] > max_lw; i--) {
  }
  max_ref_gr = i;
  for (i = bot_gr; lw[i] < min_lw; i++) {
  }
  min_gr = i;
  for (i = top_gr; lw[i] > max_lw; i--) {
  }
  max_gr = i;

  j = min_ref_gr;
  for (i = min_gr; i <= max_gr; i++) {
    while (ref_lw[j] < lw[i] && j < max_ref_gr) j++;
    lut[i] = j;
  }
  fac = (float)min_ref_gr / (float)min_gr;
  for (i = 0; i < min_gr; i++) lut[i] = troundp(i * fac);
  fac = (float)(255 - max_ref_gr) / (float)(255 - max_gr);
  for (i = 255; i > max_gr; i--) lut[i] = 255 - troundp((255 - i) * fac);

  /***
printf ("-----\n\lut:\n\n");
for (i = 255; i >= 0; i--)
printf ("%4d :%4u\n", i, lut[i]);
printf ("\n");
***/
}

/*===========================================================================*/

void build_gr_cum(const TRasterImageP &image, int cum[256]) {
  int lx, ly, wrap, true_lx, true_ly;
  int i, x, y;
  UCHAR *pix, *buffer;
  int histo[256], raster_is_savebox;

  get_virtual_buffer(image, &lx, &ly, &wrap, &buffer);

  for (i = 0; i < 256; i++) histo[i] = 0;
  for (y = 0; y < ly; y++) {
    pix = buffer + y * wrap;
    for (x = 0; x < lx; x++) histo[*pix++]++;
  }

  raster_is_savebox = 1;
  TRect saveBox     = image->getSavebox();
  if ((saveBox.getLx() > 0 && saveBox.getLx() < image->getRaster()->getLx()) ||
      (saveBox.getLy() > 0 && saveBox.getLy() < image->getRaster()->getLy()))
    raster_is_savebox = 0;

  if (raster_is_savebox) {
    true_lx = saveBox.getLx() ? saveBox.getLx() : image->getRaster()->getLx();
    true_ly = saveBox.getLy() ? saveBox.getLy() : image->getRaster()->getLy();
  } else {
    true_lx = image->getRaster()->getLx();
    true_ly = image->getRaster()->getLy();
  }
  histo[255] += true_lx * true_ly - lx * ly;

  build_cum(histo, cum);
}

/*---------------------------------------------------------------------------*/

void build_gr_lut(int ref_cum[256], int cum[256], UCHAR lut[256]) {
  /* crea una lut tale che l'immagine con la distribuzione cumulativa cum
venga mappata in un'immagine con la cumulativa di riferimento ref_cum
*/
  int i, j, n;

  j = 0;
  for (i = 0; i < 256; i++) {
    n = cum[i];
    while (ref_cum[j] < n) j++;
    lut[i] = j;
  }
}

/*===========================================================================*/

struct edge_config {
  const char *str;
  int val;
};

static struct edge_config Edge_base[] = {
    {"     "
     "  X  "
     "     ",
     0},

    {"X    "
     "  X  "
     "     ",
     0},

    {"  X  "
     "  X  "
     "     ",
     0},

    {"X X  "
     "  X  "
     "     ",
     5},

    {"X   X"
     "  X  "
     "     ",
     20},

    {"X    "
     "  X X"
     "     ",
     24},

    {"X    "
     "  X  "
     "    X",
     28},

    {"  X  "
     "  X X"
     "     ",
     7},

    {"  X  "
     "  X  "
     "  X  ",
     20},

    {"X X X"
     "  X  "
     "     ",
     10},

    {"X X  "
     "  X X"
     "     ",
     12},

    {"X X  "
     "  X  "
     "    X",
     26},

    {"X X  "
     "  X  "
     "  X  ",
     22},

    {"X X  "
     "  X  "
     "X    ",
     22},

    {"X X  "
     "X X  "
     "     ",
     5},

    {"X   X"
     "  X  "
     "    X",
     34},

    {"X   X"
     "  X  "
     "  X  ",
     34},

    {"X    "
     "  X X"
     "  X  ",
     24},

    {"  X  "
     "  X X"
     "  X  ",
     10},

    {"X X X"
     "  X X"
     "     ",
     12},

    {"X X X"
     "  X  "
     "    X",
     24},

    {"X X X"
     "  X  "
     "  X  ",
     24},

    {"X X  "
     "  X X"
     "    X",
     14},

    {"X X  "
     "  X X"
     "  X  ",
     12},

    {"X X  "
     "  X X"
     "X    ",
     22},

    {"X X  "
     "X X X"
     "     ",
     10},

    {"X X  "
     "  X  "
     "  X X",
     24},

    {"X X  "
     "  X  "
     "X   X",
     32},

    {"X X  "
     "X X  "
     "    X",
     24},

    {"X X  "
     "  X  "
     "X X  ",
     20},

    {"X   X"
     "  X  "
     "X   X",
     40},

    {"  X  "
     "X X X"
     "  X  ",
     0},

    {"X X X"
     "X X X"
     "     ",
     10},

    {"X X X"
     "X X  "
     "    X",
     22},

    {"X X  "
     "X X X"
     "    X",
     12},

    {"X   X"
     "X X X"
     "    X",
     22},

    {"  X X"
     "X X X"
     "    X",
     12},

    {"X X X"
     "  X X"
     "    X",
     14},

    {"X X  "
     "X X X"
     "  X  ",
     0},

    {"X   X"
     "X X X"
     "  X  ",
     10},

    {"X   X"
     "X X  "
     "  X X",
     20},

    {"X   X"
     "X X  "
     "X   X",
     30},

    {"X X X"
     "X X X"
     "    X",
     12},

    {"X X X"
     "X X X"
     "  X  ",
     0},

    {"X X X"
     "X X  "
     "  X X",
     10},

    {"  X X"
     "X X X"
     "X X  ",
     0},

    {"X X X"
     "X X  "
     "X   X",
     20},

    {"X   X"
     "X X X"
     "X   X",
     20},

    {"X X X"
     "X X X"
     "  X X",
     0},

    {"X X X"
     "X X X"
     "X   X",
     10},

    {"X X X"
     "X X X"
     "X X X",
     0},

    {0, 0},
};

static int Edge_value[256];
static int Edge_init_done = 0;

static int edge_int(const char x[8]);
static void edge_rotate(char x[8]);
static void edge_mirror(char x[8]);

/*---------------------------------------------------------------------------*/

static void edge_init(void) {
  int b;
  const char *str;
  int val;
  char x[8];

  for (b = 0; Edge_base[b].str; b++) {
    str  = Edge_base[b].str;
    val  = Edge_base[b].val;
    x[0] = str[0];
    x[1] = str[2];
    x[2] = str[4];
    x[3] = str[5];
    x[4] = str[9];
    x[5] = str[10];
    x[6] = str[12];
    x[7] = str[14];

    Edge_value[edge_int(x)] = val;
    edge_rotate(x);
    Edge_value[edge_int(x)] = val;
    edge_rotate(x);
    Edge_value[edge_int(x)] = val;
    edge_rotate(x);
    Edge_value[edge_int(x)] = val;
    edge_rotate(x);
    edge_mirror(x);
    Edge_value[edge_int(x)] = val;
    edge_rotate(x);
    Edge_value[edge_int(x)] = val;
    edge_rotate(x);
    Edge_value[edge_int(x)] = val;
    edge_rotate(x);
    Edge_value[edge_int(x)] = val;
  }
  Edge_init_done = 1;
}

/*---------------------------------------------------------------------------*/

static int edge_int(const char x[8]) {
  return (x[0] != ' ') << 7 | (x[1] != ' ') << 6 | (x[2] != ' ') << 5 |
         (x[3] != ' ') << 4 | (x[4] != ' ') << 3 | (x[5] != ' ') << 2 |
         (x[6] != ' ') << 1 | (x[7] != ' ');
}

/*---------------------------------------------------------------------------*/

static void edge_rotate(char x[8]) {
  char tmp;

  tmp  = x[0];
  x[0] = x[2];
  x[2] = x[7];
  x[7] = x[5];
  x[5] = tmp;
  tmp  = x[1];
  x[1] = x[4];
  x[4] = x[6];
  x[6] = x[3];
  x[3] = tmp;
}

/*---------------------------------------------------------------------------*/

static void edge_mirror(char x[8]) {
  char tmp;

  tmp  = x[0];
  x[0] = x[2];
  x[2] = tmp;
  tmp  = x[3];
  x[3] = x[4];
  x[4] = tmp;
  tmp  = x[5];
  x[5] = x[7];
  x[7] = tmp;
}

/*---------------------------------------------------------------------------*/

void histo_l_algo(const TRasterImageP &image, int reference) {
  int lx, ly, wrap;
  int x, y, grey, ref_grey, m, d, dd;
  UCHAR *buffer, *pix, *north, *south;
  int g_histo[256];
  int d_histo[256][256];
  int cum[256];
  long n;
  BIG s;
  float mean_d[256];
  int max_d_grey;
  float max_d;
  int conf;
  int edgelen;
  float fac;
  UCHAR lut[256];

  if (!Edge_init_done) edge_init();

  get_virtual_buffer(image, &lx, &ly, &wrap, &buffer);

  for (grey = 0; grey < 256; grey++) {
    g_histo[grey] = 0;
    for (d = 0; d < 256; d++) d_histo[grey][d] = 0;
  }

  /* istogramma dei grigi e istogramma delle derivate */
  for (y = 1; y < ly - 1; y++) {
    pix   = buffer + y * wrap + 1;
    north = pix + wrap;
    south = pix - wrap;
    for (x = 1; x < lx - 1; x++, pix++, north++, south++) {
      m = 2 * ((int)north[0] + (int)pix[-1] + 2 * (int)pix[0] + (int)pix[1] +
               (int)south[0]) +
          (int)north[-1] + (int)north[1] + (int)south[-1] + (int)south[1];
      m = (m + 8) >> 4;
      d = (int)north[-1] + (int)north[0] * 2 + (int)north[1] - (int)south[-1] -
          (int)south[0] * 2 - (int)south[1];
      TO_ABS(d)
      dd = (int)north[-1] + (int)pix[-1] * 2 + (int)south[-1] - (int)north[1] -
           (int)pix[1] * 2 - (int)south[1];
      TO_ABS(dd)
      if (dd > d) d = dd;
      dd = (int)pix[-1] + (int)north[-1] * 2 + (int)north[0] - (int)pix[1] -
           (int)south[1] * 2 - (int)south[0];
      TO_ABS(dd)
      if (dd > d) d = dd;
      dd = (int)pix[1] + (int)north[1] * 2 + (int)north[0] - (int)pix[-1] -
           (int)south[-1] * 2 - (int)south[0];
      TO_ABS(dd)
      if (dd > d) d = dd;
      d             = (d + 2) >> 2;
      d_histo[m][d]++;
      g_histo[*pix]++;
    }
  }
  build_cum(g_histo, cum);

  /* costruzione pendenze medie */
  for (grey = 0; grey < 256; grey++) {
    n = 0;
    CLEAR_BIG(s);
    for (d = 0; d < 256; d++) {
      ADD_BIG(s, d_histo[grey][d] * d);
      n += d_histo[grey][d];
    }
    if (n)
      mean_d[grey] = (float)(BIG_TO_DOUBLE(s) / n);
    else
      mean_d[grey] = 0.0;
  }
  smooth_func256(mean_d, 5);

  /* determinazione grigio di massima pendenza */
  max_d_grey = 0;
  max_d      = 0.0;
  for (grey = 0; grey < 256; grey++) {
    if (max_d < mean_d[grey]) {
      max_d      = mean_d[grey];
      max_d_grey = grey;
    }
  }

  /* stima della lunghezza dei bordi */
  edgelen = 0;
  for (y = 1; y < ly - 1; y++) {
    pix   = buffer + y * wrap + 1;
    north = pix + wrap;
    south = pix - wrap;
    conf  = -1;
    for (x = 1; x < lx - 1; x++, pix++, north++, south++) {
      if (pix[0] <= max_d_grey) {
        if (conf >= 0) {
          conf = ((conf << 1) & ((1 << 7) | (1 << 6) | (1 << 2) | (1 << 1))) |
                 ((north[1] <= max_d_grey) << 5) | ((1) << 4) |
                 ((pix[1] <= max_d_grey) << 3) | ((south[1] <= max_d_grey));
        } else {
          conf =
              ((north[-1] <= max_d_grey) << 7) |
              ((north[0] <= max_d_grey) << 6) |
              ((north[1] <= max_d_grey) << 5) | ((pix[-1] <= max_d_grey) << 4) |
              ((pix[1] <= max_d_grey) << 3) | ((south[-1] <= max_d_grey) << 2) |
              ((south[0] <= max_d_grey) << 1) | ((south[1] <= max_d_grey));
        }
        edgelen += Edge_value[conf];
      } else
        conf = -1;
    }
  }

  if (reference) {
    for (grey = 0; grey < 256; grey++) Ref_cum[grey] = cum[grey];
    Ref_edgelen                                      = edgelen;

    return;
  }

  /* normalizza la cumulativa per il numero di linee */
  fac = (float)Ref_edgelen / edgelen;

  /***
printf ("fac: %f\n", fac);
printf ("cum prima:\n");
for (grey = 0; grey < 256; grey++)
printf ("%9d\n", cum[grey]);
***/

  for (grey = 0; grey < 255; grey++) {
    cum[grey] = (int)(cum[grey] * fac);
    notMoreThan(cum[255], cum[grey]);
  }

  /***
printf ("cum dopo:\n");
for (grey = 0; grey < 256; grey++)
printf ("%9d\n", cum[grey]);
printf ("Ref_cum:\n");
for (grey = 0; grey < 256; grey++)
printf ("%9d\n", Ref_cum[grey]);
***/

  /* equalizza l'istogramma */
  ref_grey = 0;
  for (grey = 0; grey < 255; grey++) {
    while (Ref_cum[ref_grey] < cum[grey]) ref_grey++;
    lut[grey] = ref_grey;
  }
  lut[255] = 255;
  /* DAFARE
if (Wl_flag)
write_lut_image (lut);
*/
  apply_lut(image, lut);
}

/*===========================================================================*/
/*===========================================================================*/

#define MAX_N_CHAINS 100

#define MAX_WIDTH 100

#define MIN_COUNT 100

#define MAX_HGREY (244 >> 1)

/*---------------------------------------------------------------------------*/
#ifdef DAFARE
static int build_th_histo(const TRasterImageP &image,
                          int histo[256 >> 1][MAX_WIDTH + 1]) {
  int lx, ly, wrap, x, y, hgrey, width;
  UCHAR *buffer, *cur, *x_prev, *y_prev, *x_next, *y_next;
  UCHAR cur_hgrey, x_prev_hgrey, y_prev_hgrey, x_next_hgrey, y_next_hgrey;
  int x_grad, y_grad;
  int hgrey_histo[256 >> 1];
  int x_start[256 >> 1];
  static int *y_start[256 >> 1];
  static int y_start_alloc = 0;

  get_virtual_buffer(image, &lx, &ly, &wrap, &buffer);

  if (y_start_alloc < lx) {
    for (hgrey = 0; hgrey<256>> 1; hgrey++) {
      delete[] y_start[hgrey];
      y_start[hgrey] = new int[lx];
      //    TREALLOC (y_start[hgrey], lx);
    }
    y_start_alloc = lx;
  }
  for (hgrey = 0; hgrey<256>> 1; hgrey++) {
    for (x = 0; x < lx; x++) y_start[hgrey][x] = -MAX_WIDTH - 1;
    for (width = 0; width <= MAX_WIDTH; width++) histo[hgrey][width] = 0;
    hgrey_histo[hgrey]                                               = 0;
  }

  for (y = 1; y < ly - 1; y++) {
    for (hgrey = 0; hgrey<256>> 1; hgrey++) x_start[hgrey] = -MAX_WIDTH - 1;
    cur    = buffer + y * wrap + 1;
    x_prev = cur - 1;
    y_prev = cur - wrap;
    x_next = cur + 1;
    y_next = cur + wrap;
    for (x = 1; x < lx - 1;
         x++, cur++, x_prev++, y_prev++, x_next++, y_next++) {
      cur_hgrey    = *cur >> 1;
      x_prev_hgrey = *x_prev >> 1;
      x_next_hgrey = *x_next >> 1;
      y_prev_hgrey = *y_prev >> 1;
      y_next_hgrey = *y_next >> 1;
      hgrey_histo[cur_hgrey]++;
      x_grad = (int)x_next_hgrey - (int)x_prev_hgrey;
      y_grad = (int)y_next_hgrey - (int)y_prev_hgrey;
      if (cur_hgrey < x_prev_hgrey) {
        if (x_grad < 0 && x_grad < y_grad && x_grad < -y_grad)
          for (hgrey       = cur_hgrey; hgrey < x_prev_hgrey; hgrey++)
            x_start[hgrey] = x;
        else
          for (hgrey       = cur_hgrey; hgrey < x_prev_hgrey; hgrey++)
            x_start[hgrey] = -MAX_WIDTH - 1;
      } else if (cur_hgrey > x_prev_hgrey) {
        if (x_grad > 0 && x_grad > y_grad && x_grad > -y_grad)
          for (hgrey = x_prev_hgrey; hgrey < cur_hgrey; hgrey++) {
            width = x - x_start[hgrey];
            if (width <= MAX_WIDTH) histo[hgrey][width]++;
          }
      }
      if (cur_hgrey < y_prev_hgrey) {
        if (y_grad < 0 && y_grad < x_grad && y_grad < -x_grad)
          for (hgrey          = cur_hgrey; hgrey < y_prev_hgrey; hgrey++)
            y_start[hgrey][x] = y;
        else
          for (hgrey          = cur_hgrey; hgrey < y_prev_hgrey; hgrey++)
            y_start[hgrey][x] = -MAX_WIDTH - 1;
      } else if (cur_hgrey > y_prev_hgrey) {
        if (y_grad > 0 && y_grad > x_grad && y_grad > -x_grad)
          for (hgrey = y_prev_hgrey; hgrey < cur_hgrey; hgrey++) {
            width = y - y_start[hgrey][x];
            if (width <= MAX_WIDTH) histo[hgrey][width]++;
          }
      }
    }
  }
  return 1;
}
#endif
/*---------------------------------------------------------------------------*/
#ifdef DAFARE
// SEMBRA NON ESSERE UTILIZZATO!
int eval_image_th(const TRasterImageP &image, int *threshold,
                  float *linewidth) {
#define MIN_COUNT 100
#define MAX_HGREY (244 >> 1)
  int histo[256 >> 1][MAX_WIDTH + 1];
  char is_max[256 >> 1][MAX_WIDTH + 1];
  int chain_map[256 >> 1][MAX_WIDTH + 1];
  int chain_n[MAX_N_CHAINS];
  int n_chains;
  int peak[256 >> 1];
  float mean_width[256 >> 1];
  int min_hgrey, max_hgrey, min_chain_hgrey, max_chain_hgrey;
  int hgrey, width, chain, c, last;
  long n, nx;
  float delta, min_delta;

  TRasterGR8P ras8(image->getRaster());

  if (!ras8) {
    TERROR("image is not RAS_GR8");
    return 0;
  }
  if (!(*threshold != 0 && *linewidth == 0 ||
        *threshold == 0 && *linewidth != 0))
    return 0;

  build_th_histo(image, histo);

  if (*threshold != 0) {
    min_hgrey = *threshold >> 1;
    max_hgrey = *threshold >> 1;
    notLessThan(0, min_hgrey);
    notMoreThan<int>(MAX_HGREY, max_hgrey);
  } else {
    for (hgrey = 0; hgrey<256>> 1; hgrey++)
      for (width = 1; width < MAX_WIDTH; width++)
        if (histo[hgrey][width] >= MIN_COUNT) goto endfor;
  endfor:
    if (hgrey == 256 >> 1) return 0;

    min_hgrey = hgrey;
    max_hgrey = MAX_HGREY;
  }
  if (min_hgrey > max_hgrey) return 0;

  /* costruisci la matrice dai massimi */
  for (hgrey = min_hgrey; hgrey <= max_hgrey; hgrey++) {
    is_max[hgrey][0]         = 0;
    is_max[hgrey][MAX_WIDTH] = 0;
    for (width = 1; width < MAX_WIDTH; width++) {
      is_max[hgrey][width] = histo[hgrey][width] > histo[hgrey][width - 1] &&
                             histo[hgrey][width] > histo[hgrey][width + 1] &&
                             histo[hgrey][width] >= MIN_COUNT;
    }
  }
  /* costruisci la matrice delle dorsali */
  n_chains = 0;
  for (width = 0; width <= MAX_WIDTH; width++) {
    if (is_max[min_hgrey][width] && n_chains < MAX_N_CHAINS) {
      chain_map[min_hgrey][width]          = n_chains++;
      chain_n[chain_map[min_hgrey][width]] = histo[min_hgrey][width];
    } else {
      chain_map[min_hgrey][width] = -1;
    }
  }
  for (hgrey = min_hgrey + 1; hgrey <= max_hgrey; hgrey++) {
    for (width = 0; width <= MAX_WIDTH; width++) {
      if (is_max[hgrey][width]) {
        if (chain_map[hgrey - 1][width] >= 0) {
          chain_map[hgrey][width] = chain_map[hgrey - 1][width];
          chain_n[chain_map[hgrey][width]] += histo[hgrey][width];
        } else if (chain_map[hgrey - 1][width - 1] >= 0 &&
                   chain_map[hgrey - 1][width - 1] !=
                       chain_map[hgrey][width - 2]) {
          chain_map[hgrey][width] = chain_map[hgrey - 1][width - 1];
          chain_n[chain_map[hgrey][width]] += histo[hgrey][width];
        } else if (chain_map[hgrey - 1][width + 1] >= 0) {
          chain_map[hgrey][width] = chain_map[hgrey - 1][width + 1];
          chain_n[chain_map[hgrey][width]] += histo[hgrey][width];
        } else if (n_chains < MAX_N_CHAINS) {
          chain_map[hgrey][width]          = n_chains++;
          chain_n[chain_map[hgrey][width]] = histo[hgrey][width];
        } else {
          chain_map[hgrey][width] = -1;
        }
      } else {
        chain_map[hgrey][width] = -1;
      }
    }
  }
  if (!n_chains) return 0;

  /* scegli la dorsale principale */
  chain = 0;
  for (c                                   = 1; c < n_chains; c++)
    if (chain_n[c] > chain_n[chain]) chain = c;

  /* costruisci il vettore dei massimi */
  for (hgrey = min_hgrey; hgrey <= max_hgrey; hgrey++)
    for (width = 1; width < MAX_WIDTH; width++)
      if (chain_map[hgrey][width] == chain) goto chain_start_found;
chain_start_found:
  min_chain_hgrey       = hgrey;
  peak[min_chain_hgrey] = width;
  for (hgrey = min_chain_hgrey + 1; hgrey <= max_hgrey; hgrey++) {
    if (chain_map[hgrey][width] == chain) {
    } else if (chain_map[hgrey][width + 1] == chain)
      width++;
    else if (chain_map[hgrey][width - 1] == chain)
      width--;
    else
      break;
    peak[hgrey] = width;
  }
  max_chain_hgrey = hgrey - 1;

  /* costruisci il vettore delle medie */
  for (hgrey = min_chain_hgrey; hgrey <= max_chain_hgrey; hgrey++) {
    width = peak[hgrey];
    n     = histo[hgrey][width];
    nx    = n * width;
    last  = n;
    for (width = peak[hgrey] - 1; width > 0; width--) {
      if (histo[hgrey][width] > last) break;
      n += histo[hgrey][width];
      nx += histo[hgrey][width] * width;
      last = histo[hgrey][width];
    }
    last = histo[hgrey][peak[hgrey]];
    for (width = peak[hgrey] + 1; width <= MAX_WIDTH; width++) {
      if (width > peak[hgrey] * 5) break;
      if (histo[hgrey][width] > last) break;
      n += histo[hgrey][width];
      nx += histo[hgrey][width] * width;
      last = histo[hgrey][width];
    }
    mean_width[hgrey] = (float)nx / (float)n;
  }

  if (*threshold != 0) {
    *linewidth = mean_width[*threshold >> 1];
  } else {
    min_delta = 123456789.0;
    for (hgrey = min_chain_hgrey; hgrey <= max_chain_hgrey; hgrey++) {
      delta                = mean_width[hgrey] - *linewidth;
      if (delta < 0) delta = -delta;
      if (delta < min_delta) {
        min_delta  = delta;
        *threshold = hgrey << 1;
      }
    }
  }

  return 1;
}
#endif
/*---------------------------------------------------------------------------*/
#ifdef DAFARE
// capire che cosa fa !?? chi la usa ?
// sembra prendere una gr8, modificare il buffer e cambiare il tipo in BW
void thresh_image(const TRasterImageP &image, int threshold,
                  int oversample_factor) {
  UCHAR *buffer, *cur, *xnext, *ynext, *xynext;
  UCHAR *bigline, *out;
  UCHAR tmp;
  int lx, ly, wrap, outwrap_bytes, outwrap_pix;
  int x, y, bit;
  UINT thresh, thresh_2, thresh_4;

  TRasterGR8P ras8(image->getRaster());

  assert(ras8);
  thresh        = threshold;
  thresh_2      = thresh * 2;
  thresh_4      = thresh * 4;
  buffer        = ras8->getRawData();
  lx            = ras8->getLx();
  ly            = ras8->getLy();
  wrap          = ras8->getWrap();
  outwrap_pix   = lx * oversample_factor;
  outwrap_bytes = (outwrap_pix + 7) / 8;
  TMALLOC(bigline, outwrap_bytes * oversample_factor)

  switch (oversample_factor) {
  case 1:
    for (y = 0; y < ly; y++) {
      cur = buffer + y * wrap;
      out = y == 0 ? bigline : buffer + y * outwrap_bytes;
      tmp = 0;
      bit = 7;
      for (x = 0; x < lx; x++, cur++) {
        if (*cur <= thresh) tmp |= 1 << bit;
        bit--;
        if (bit < 0) {
          *out++ = tmp;
          tmp    = 0;
          bit    = 7;
        }
      }
      if (bit != 7) *out++ = tmp;
      if (y == 0) memcpy(buffer, bigline, outwrap_bytes);
    }
    break;

  case 2:
    for (y = 0; y < ly; y++) {
      cur   = buffer + y * wrap;
      xnext = cur + 1;
      out   = y == 0 ? bigline : buffer + y * 2 * outwrap_bytes;
      tmp   = 0;
      bit   = 7;
      for (x = 0; x < lx; x++, cur++, xnext += x < lx - 1) {
        if (*cur <= thresh) tmp |= 1 << bit;
        bit--;
        if (*cur + *xnext <= thresh_2) tmp |= 1 << bit;
        bit--;
        if (bit < 0) {
          *out++ = tmp;
          tmp    = 0;
          bit    = 7;
        }
      }
      if (bit != 7) *out++ = tmp;

      cur    = buffer + y * wrap;
      xnext  = cur + 1;
      ynext  = y == ly - 1 ? cur : cur + wrap;
      xynext = ynext + 1;
      out    = y == 0 ? bigline + outwrap_bytes
                   : buffer + (y * 2 + 1) * outwrap_bytes;
      tmp = 0;
      bit = 7;
      for (x = 0; x < lx;
           x++, cur++, xnext += x < lx - 1, ynext++, xynext += x < lx - 1) {
        if (*cur + *ynext <= thresh_2) tmp |= 1 << bit;
        bit--;
        if (*cur + *xnext + *ynext + *xynext <= thresh_4) tmp |= 1 << bit;
        bit--;
        if (bit < 0) {
          *out++ = tmp;
          tmp    = 0;
          bit    = 7;
        }
      }
      if (bit != 7) *out++ = tmp;
      if (y == 0) memcpy(buffer, bigline, outwrap_bytes * 2);
    }
    break;

  default:
    assert(0);
  }
  image->ras.type = RAS_WB;
  image->ras.lx   = lx * oversample_factor;
  image->ras.ly   = ly * oversample_factor;
  image->ras.wrap = outwrap_pix;
  image->x_dpi *= oversample_factor;
  image->y_dpi *= oversample_factor;
  image->sb_x0 *= oversample_factor;
  image->sb_y0 *= oversample_factor;
  image->sb_or_img_lx *= oversample_factor;
  image->sb_or_img_ly *= oversample_factor;

  TFREE(bigline)
}
#endif
/*===========================================================================*/
/*===========================================================================*/

void apply_lut(const TRasterImageP &image, UCHAR lut[256]) {
  int x, y, lx, ly, wrap;
  UCHAR *buffer, *pix;

  TRasterGR8P ras8(image->getRaster());
  assert(ras8);

  lx   = ras8->getLx();
  ly   = ras8->getLy();
  wrap = ras8->getWrap();
  ras8->lock();
  buffer = (UCHAR *)ras8->getRawData();

  for (y = 0; y < ly; y++) {
    pix = buffer + y * wrap;
    for (x = 0; x < lx; x++, pix++) *pix = lut[*pix];
  }
  ras8->unlock();
}
