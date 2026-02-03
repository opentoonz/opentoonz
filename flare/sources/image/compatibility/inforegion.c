#include <stdio.h>
#include "inforegion.h"
#include "tnz4.h"
/*---------------------------------------------------------------------------*/

#ifndef min
#define min(a, b) (((a) < (b)) ? (a) : (b))
#endif
#ifndef max
#define max(a, b) (((a) > (b)) ? (a) : (b))
#endif

void getInfoRegion(INFO_REGION *region, int x1_out, int y1_out, int x2_out,
                   int y2_out, int scale, int width_in, int height_in) {
  /*
*    I suffissi _in e _out sono relativi alle immagini di
*    input e output, cioe' all'immagine sorgente (input)
*    ca cui prendere (leggere) la regione voluta (output).
*/

  int x1_in, y1_in, x2_in, y2_in;

#define SWAP(a, b)                                                             \
  {                                                                            \
    int tmp;                                                                   \
    tmp = a;                                                                   \
    a   = b;                                                                   \
    b   = tmp;                                                                 \
  }

  /* Scambia le coordinate della regione da leggere se invertite */
  if (x1_out > x2_out) SWAP(x1_out, x2_out);
  if (y1_out > y2_out) SWAP(y1_out, y2_out);

#ifdef DEBUG
  /* Controllo della consistenza delle coordinate della regione */
  if (((x2_out - x1_out) < 0) || ((y2_out - y1_out) < 0))
    printf("warning: bad region coord.\n");
#endif

  /* Copia delle coordinate della regione per region globale */
  region->x1 = x1_out;
  region->y1 = y1_out;
  region->x2 = x2_out;
  region->y2 = y2_out;

  /* Imposta le dimensioni dell'immagine in output */
  if (scale <= 0) {
    printf("error: scale value negative or zero\n");
    return;
  } else {
    region->xsize = ((x2_out - x1_out + 1) - 1) / scale + 1;
    region->ysize = ((y2_out - y1_out + 1) - 1) / scale + 1;
  }

  /* Passo sull'asse delle y e delle x */
  region->step = scale;

  /* Coordinate immagine sorgente */
  x1_in = 0;
  y1_in = 0;
  x2_in = width_in - 1;
  y2_in = height_in - 1;

  /* Dimensioni immagine sorgente */
  region->lx_in = width_in;
  region->ly_in = height_in;

  /* Numero di righe e colonne dell'immagine sorgente da scandire */
  region->scanNcol = region->xsize;
  region->scanNrow = region->ysize;

  /* Coordinate all'interno dell'immagine sorgente, da cui deve
* partire la scansione.
*/
  region->startScanRow = y1_out;
  region->startScanCol = x1_out - x1_in;

  /*
* Questi offset sono relativi al buffer di uscita, nel caso
* in cui una parte della regione sfora rispetto all'immagine
* sorgente.
*/
  region->x_offset = 0;
  region->y_offset = 0;

  /* La regione sfora sulla destra e sulla sinistra */
  if (x2_out > x2_in && x1_out < x1_in) {
    region->scanNcol     = width_in / scale;
    region->x_offset     = (x1_in - x1_out) / scale;
    region->startScanCol = 0;
  } else {
    /* La regione sfora solo sulla destra */
    if (x2_out > x2_in) {
      region->scanNcol = (x2_in - x1_out) / scale + 1;
      region->x_offset = 0;
    } else
        /* La regione sfora solo sulla sinistra */
        if (x1_out < x1_in) {
      region->x_offset     = (x1_in - x1_out) / scale;
      region->scanNcol     = (x2_out - x1_in) / scale + 1;
      region->startScanCol = 0;
    }
  }

  /* La regione sfora in alto e in basso */
  if (y2_out > y2_in && y1_out < y1_in) {
    region->scanNrow     = height_in / scale;
    region->y_offset     = (y1_in - y1_out) / scale;
    region->startScanRow = 0;
  } else {
    /* La regione sfora solo in alto */
    if (y2_out > y2_in) {
      region->scanNrow = (y2_in - y1_out) / scale + 1;
      region->y_offset = 0;
    } else
        /* La regione sfora solo in basso */
        if (y1_out < y1_in) {
      region->scanNrow     = (y2_out - y1_in) / scale + 1;
      region->y_offset     = (y1_in - y1_out) / scale;
      region->startScanRow = 0;
    }
  }
}

/*---------------------------------------------------------------------------*/

int get_info_region(EXT_INFO_REGION *region, int x1_out, int y1_out, int x2_out,
                    int y2_out, int scale, int width_in, int height_in,
                    int orientation) {
  /*
*    I suffissi _in e _out sono relativi alle immagini di
*    input e output, cioe' all'immagine sorgente (input)
*    ca cui prendere (leggere) la regione voluta (output).
*/

  int x1_in, y1_in, x2_in, y2_in;
  int appo, appoNcol, appoNrow;

#define SWAP(a, b)                                                             \
  {                                                                            \
    int tmp;                                                                   \
    tmp = a;                                                                   \
    a   = b;                                                                   \
    b   = tmp;                                                                 \
  }

  /* Scambia le coordinate della regione da leggere se invertite */
  if (x1_out > x2_out) SWAP(x1_out, x2_out);
  if (y1_out > y2_out) SWAP(y1_out, y2_out);

  /* Controllo della consistenza delle coordinate della regione */
  if (((x2_out - x1_out) < 1) || ((y2_out - y1_out) < 1)) {
    printf("error: bad image read region coordinates\n");
    return FALSE;
  }

  /* Copia delle coordinate della regione per region globale */
  region->x1 = x1_out;
  region->y1 = y1_out;
  region->x2 = x2_out;
  region->y2 = y2_out;

  /* Imposta le dimensioni dell'immagine in output */
  if (scale <= 0) {
    printf("error: scale value negative or zero\n");
    return FALSE;
  } else {
    region->xsize = (x2_out - x1_out) / scale + 1;
    region->ysize = (y2_out - y1_out) / scale + 1;
  }

  /* Passo sull'asse delle y e delle x */
  region->step = scale;

  /* Coordinate immagine sorgente */
  x1_in = 0;
  y1_in = 0;
  x2_in = width_in - 1;
  y2_in = height_in - 1;

  /* Dimensioni immagine sorgente */
  region->lx_in = width_in;
  region->ly_in = height_in;

  /* Numero di righe e colonne dell'immagine sorgente da scandire */
  region->scanNcol = region->xsize;
  region->scanNrow = region->ysize;

  /* Coordinate all'interno dell'immagine sorgente, da cui deve
* partire la scansione.
*/
  region->startScanRow = y1_out;
  region->startScanCol = x1_out;

  /*
* Questi offset sono relativi al buffer di uscita, nel caso
* in cui una parte della regione sfora rispetto all'immagine
* sorgente.
*/
  region->x_offset = 0;
  region->y_offset = 0;

  /* La regione sfora sulla destra e sulla sinistra */
  if (x2_out > x2_in && x1_out < x1_in) {
    region->scanNcol     = (width_in - 1) / scale /* +1 */;
    region->x_offset     = (x1_in - x1_out + scale - 1) / scale;
    region->startScanCol = 0;
  } else {
    /* La regione sfora solo sulla destra */
    if (x2_out > x2_in) {
      region->scanNcol = (x2_in - x1_out) / scale /* +1 */;
      region->x_offset = 0;
    } else
        /* La regione sfora solo sulla sinistra */
        if (x1_out < x1_in) {
      region->scanNcol     = (x2_out - x1_in) / scale /* +1 */;
      region->x_offset     = (x1_in - x1_out + scale - 1) / scale;
      region->startScanCol = 0;
    }
  }

  /* La regione sfora in alto e in basso */
  if (y2_out > y2_in && y1_out < y1_in) {
    region->scanNrow     = (height_in - 1) / scale /* +1 */;
    region->y_offset     = (y1_in - y1_out + scale - 1) / scale;
    region->startScanRow = 0;
  } else {
    /* La regione sfora solo in alto */
    if (y2_out > y2_in) {
      region->scanNrow = (y2_in - y1_out) / scale /* +1 */;
      region->y_offset = 0;
    } else
        /* La regione sfora solo in basso */
        if (y1_out < y1_in) {
      region->scanNrow     = (y2_out - y1_in) / scale /* +1 */;
      region->y_offset     = (y1_in - y1_out + scale - 1) / scale;
      region->startScanRow = 0;
    }
  }

  appoNcol = min((region->scanNcol * scale), width_in);
  appoNrow = min((region->scanNrow * scale), height_in);

  switch (orientation) {
  case TNZ_TOPLEFT:
    region->buf_inc = 1;
    region->y_offset += region->scanNrow - 1;
    region->verso_x = 0;
    region->verso_y = -1;
    region->sxpix   = region->startScanCol;
    region->sypix   = height_in - region->startScanRow - appoNrow;
    region->sypix   = max(0, region->sypix);
    break;
  case TNZ_TOPRIGHT:
    region->buf_inc = -1;
    region->y_offset += region->scanNrow - 1;
    region->x_offset += region->scanNcol - 1;
    region->verso_x = 0;
    region->verso_y = -1;
    region->sxpix   = width_in - region->startScanCol - appoNcol;
    region->sypix   = height_in - region->startScanRow - appoNrow;
    region->sxpix   = max(0, region->sxpix);
    region->sypix   = max(0, region->sypix);
    break;
  case TNZ_BOTRIGHT:
    region->buf_inc = -1;
    region->x_offset += region->scanNcol - 1;
    region->verso_x = 0;
    region->verso_y = 1;
    region->sxpix   = width_in - region->startScanCol - appoNcol;
    region->sypix   = region->startScanRow;
    break;
  case TNZ_BOTLEFT:
    region->buf_inc = 1;
    region->verso_x = 0;
    region->verso_y = 1;
    region->sxpix   = region->startScanCol;
    region->sypix   = region->startScanRow;
    break;
  case TNZ_LEFTOP:
    region->buf_inc = -region->xsize;
    region->y_offset += region->scanNrow - 1;
    region->verso_x = 1;
    region->verso_y = 0;
    region->sxpix   = height_in - region->startScanRow - appoNrow;
    region->sypix   = region->startScanCol;
    break;
  case TNZ_RIGHTOP:
    region->buf_inc = -region->xsize;
    region->y_offset += region->scanNrow - 1;
    region->x_offset += region->scanNcol - 1;
    region->verso_x = -1;
    region->verso_y = 0;
    if ((region->sxpix = height_in - region->startScanRow - appoNrow) < 0)
      region->sxpix = 0;
    if ((region->sypix = width_in - region->startScanCol - appoNcol) < 0)
      region->sypix = 0;
    break;
  case TNZ_RIGHTBOT:
    region->buf_inc = region->xsize;
    region->x_offset += region->scanNcol - 1;
    region->verso_x = -1;
    region->verso_y = 0;
    region->sxpix   = region->startScanRow;
    region->sypix   = width_in - region->startScanCol - appoNcol;
    break;
  case TNZ_LEFTBOT:
    region->buf_inc = region->xsize;
    region->verso_x = 1;
    region->verso_y = 0;
    region->sxpix   = region->startScanRow;
    region->sypix   = region->startScanCol;
    break;
  default:
    printf("error: bad orientation type\n");
    return FALSE;
  }
  /*  Se le righe sono verticali, allora ... */
  if (orientation > 4) {
    appo             = region->lx_in;
    region->lx_in    = region->ly_in;
    region->ly_in    = appo;
    appo             = region->scanNcol;
    region->scanNcol = region->scanNrow;
    region->scanNrow = appo;
  }
  return TRUE;
}

/*-------------------------------------------------------------------------*/

void print_info_region(EXT_INFO_REGION *region) {
  if (!region) return;

  printf("IMAGE INPUT:\n");
  printf(" size              (lx_in, ly_in)........ (%d,%d)\n", region->lx_in,
         region->ly_in);
  printf(" start offset      (sScanCol, sScanRow).. (%d,%d)\n",
         region->startScanCol, region->startScanRow);
  printf(" region size       (scanNcol, scanNrow).. (%d,%d)\n",
         region->scanNcol, region->scanNrow);
  printf(" bottom-left       (sxpix, sypix)........ (%d,%d)\n", region->sxpix,
         region->sypix);
  printf(" scale             (step)................ (   %d)\n", region->step);

  printf("IMAGE OUTPUT:\n");
  printf(" size              (xsize, ysize)........ (%d,%d)\n", region->xsize,
         region->ysize);
  printf(" start offset      (x_offset, y_offset).. (%d,%d)\n",
         region->x_offset, region->y_offset);
  printf(" verso             (verso_x, verso_y).... (%d,%d)\n", region->verso_x,
         region->verso_y);
  printf(" buffer increment  (buf_inc)............. (   %d)\n",
         region->buf_inc);
}
