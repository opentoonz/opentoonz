

/*---------------------------------------------------------------------------*/
/* Vesione dell'Autofill aggiornata al 22.11.94 (Fabrizio Grisoli)           */
/*---------------------------------------------------------------------------*/
/* aggiornata il 20.8.98 per CM24 (W.T.) */

#include "autofill.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "toonz/toonzimageutils.h"
#include "tw/tw.h"
#include "toonz/fill.h"
#include "toonz/ttilesaver.h"
#include "toonz4.6/tmacro.h"
/*
#include "tropcm.h"
#include "timage_io.h"
#include "tlevel_io.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/tscenehandle.h"
#include "tools/tool.h"*/

#define PRINTF

struct s_segm {
  int xa, xb, region;
};

typedef struct big { UINT lo, hi; } BIG;
#define ADD_BIG(B, X)                                                          \
  ((B).lo += (UINT)(X), (B).hi += (B).lo >> 30, (B).lo &= 0x3fffffff, (B))
#define ASS_BIG(B, X)                                                          \
  ((B).lo = (UINT)(X), (B).hi = (B).lo >> 30, (B).lo &= 0x3fffffff, (B))
#define ADD_BIG_BIG(B1, B2)                                                    \
  ((B1).lo += (B2).lo, (B1).hi += (B2).hi, (B1).hi += (B1).lo >> 30,           \
   (B1).lo &= 0x3fffffff, (B1))
#define BIG_TO_DOUBLE(B) ((double)(B).hi * (double)0x40000000 + (double)(B).lo)

#define BORDER_TOO 1
#define NO_BORDER 0
#define DIM_TRESH 0.00005
#define AMB_TRESH 130000
#define MIN_SIZE 20

struct vicine {
  int region_id;
  struct vicine *next;
};

struct s_fabri_region {
  int active, nextfree, x, y, x1, y1, x2, y2, lx, ly, xm, ym, npix, lxa, lxb,
      tone, color_id, per, holes, match;
  BIG bx, by;
  BIG bx2, by2;
  struct vicine *vicini;
};

struct s_fabri_region_list {
  struct s_fabri_region *array;
  int size, n, lx, ly;
};

static struct s_fabri_region_list F_reference = {0, 0, 0}, F_work = {0, 0, 0};

static int F_ref_bx = 0;
static int F_ref_by = 0;
static int F_wor_bx = 0;
static int F_wor_by = 0;
static int Dx_f = 0, DP_f = 0, Df_f = 0;
static int Dx_t = 0, DP_t = 0, Df_t = 0;

/*---------------------------------------------------------------------------*/
/*-- Local Prototipes -------------------------------------------------------*/

static int trova_migliore_padre(int prob_vector[], int *att_best);
static int match(int prob[], int padre, int *fro, int *to);

static void scan_fabri_regions(TRasterCM32P ras,
                               struct s_fabri_region_list *rlst, int mode,
                               int x1, int y1, int x2, int y2);
static void rinomina(int r1, int r2, int r_num, struct s_fabri_region_list *rl);
static void aggiungi(int r1, int r2, struct s_fabri_region_list *rlst);
static void rimuovi_tutti(int r1, struct s_fabri_region_list *rlst);
static void fondi(struct s_fabri_region_list *rlst, int r1, int r2);
static void assign_prob3(int prob[], int i, int j);
static void find_best(int prob_vector[], int *col, int to);
static void free_list(struct vicine **vic);
static int somma_quadrati(int x, int y);

/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
void rect_autofill_learn(const TToonzImageP &imgToLearn, int x1, int y1, int x2,
                         int y2)
/*---------------------------------------------------------------------------*/
{
  int i;
  double pbx, pby;
  double abx, aby;
  int tot_pix = 0;

  if ((x2 - x1) * (y2 - y1) < MIN_SIZE) return;

  pbx = pby = 0.0;
  abx = aby = 0.0;

  TRasterCM32P ras = imgToLearn->getRaster();

  if (F_reference.array) {
    for (i = 0; i < F_reference.n; i++) free_list(&F_reference.array[i].vicini);
    free(F_reference.array);
  }

  F_reference.array = 0;
  F_reference.size  = 0;
  F_reference.n     = 0;
  F_reference.lx    = 0;
  F_reference.ly    = 0;

  scan_fabri_regions(ras, &F_reference, 1, x1, y1, x2, y2);

  for (i = 0; i < F_reference.n; i++) {
    F_reference.array[i].match = -1;
    F_reference.array[i].color_id =
        ras->pixels(F_reference.array[i].y)[F_reference.array[i].x].getPaint();
    pbx += BIG_TO_DOUBLE(F_reference.array[i].bx);
    pby += BIG_TO_DOUBLE(F_reference.array[i].by);
    abx = BIG_TO_DOUBLE(F_reference.array[i].bx);
    aby = BIG_TO_DOUBLE(F_reference.array[i].by);
    tot_pix += F_reference.array[i].npix;
  }

  if (tot_pix) {
    F_ref_bx = pbx / tot_pix;
    F_ref_by = pby / tot_pix;
  } else {
    F_ref_bx = 0;
    F_ref_by = 0;
  }
}

/*----------------------------------------------------------------------------*/
bool rect_autofill_apply(const TToonzImageP &imgToApply, int x1, int y1, int x2,
                         int y2, bool selective, TTileSetCM32 *tileSet)
/*----------------------------------------------------------------------------*/
{
  double pbx, pby;
  double abx, aby;
  int i, j;
  int tot_pix = 0;
  int *prob_vector;
  int fro, to;
  int col;
  int valore;
  int padre;
  // int temp_prob, att_match;
  TRasterCM32P ras = imgToApply->getRaster();

  if ((x2 - x1) * (y2 - y1) < MIN_SIZE) return false;

  if (F_reference.n <= 0 || F_reference.array == 0) return false;

  pbx = pby = 0.0;
  abx = aby = 0.0;

  /* Resetta gli eventuali accoppiamenti fatti precedentemente */
  for (i = 0; i < F_reference.n; i++) F_reference.array[i].match = -1;

  if (F_work.array) {
    for (i = 0; i < F_work.n; i++) free_list(&F_work.array[i].vicini);
    free(F_work.array);
  }
  F_work.array = 0;
  F_work.size  = 0;
  F_work.n     = 0;
  F_work.lx    = 0;
  F_work.ly    = 0;

  scan_fabri_regions(ras, &F_work, 1, x1, y1, x2, y2);
  if (F_work.n <= 0 || F_work.array == 0) return false;

  if (abs(F_work.lx * F_work.ly - F_reference.lx * F_reference.ly) >
      0.1 * (F_work.lx * F_work.ly + F_reference.lx * F_reference.ly))
    return false;

  for (i = 0; i < F_work.n; i++) {
    F_work.array[i].match = -1;
    pbx += BIG_TO_DOUBLE(F_work.array[i].bx);
    pby += BIG_TO_DOUBLE(F_work.array[i].by);
    abx = BIG_TO_DOUBLE(F_work.array[i].bx);
    aby = BIG_TO_DOUBLE(F_work.array[i].by);
    tot_pix += F_work.array[i].npix;
  }

  F_wor_bx = pbx / tot_pix;
  F_wor_by = pby / tot_pix;

  prob_vector = (int *)calloc(3 * F_work.n * F_reference.n, sizeof(int));

  for (i = 0; i < F_reference.n; i++)
    for (j = 0; j < F_work.n; j++) assign_prob3(prob_vector, i, j);

  Dx_f = Dx_f / F_reference.n;
  DP_f = DP_f / F_reference.n;
  Df_f = Df_f / F_reference.n;

  Dx_t = Dx_t / F_work.n;
  DP_t = DP_t / F_work.n;
  Df_t = Df_t / F_work.n;

#ifdef SELECTIVE
  if (selective) {
    // Accoppia non trasparenti

    for (i = 0; i < F_work.n; i++)
      if (F_work.array[i].color_id != 0) {
        temp_prob = 0;
        att_match = F_reference.n;
        /* Se non verra' aggiornato in seguito non verra' *
* comunque piu' cambiato di colore               */
        for (j = 0; j < F_reference.n; j++)
          if ((F_reference.array[i].match < 0) &&
              ((prob_vector[i * F_reference.n + j] / 1000.0) *
                   (prob_vector[i * F_reference.n + j] / 1000.0) *
                   (prob_vector[2 * (F_work.n * F_reference.n) +
                                i * F_reference.n + j] /
                    1500.0) >
               temp_prob)) {
            att_match = j;
            temp_prob = (prob_vector[i * F_reference.n + j] / 1000.0) *
                        (prob_vector[i * F_reference.n + j] / 1000.0) *
                        (prob_vector[2 * (F_work.n * F_reference.n) +
                                     i * F_reference.n + j] /
                         1500.0);
          }
        F_work.array[i].match = att_match;
        if (att_match < F_reference.n) F_reference.array[att_match].match = i;
      }

    /*--------------------------------------------------------------------------*/
  }
#endif

  bool recomputeBBox = false;
  FillParameters params;
  params.m_emptyOnly = selective;
  for (i = 0; i < F_reference.n && i < F_work.n; i++) {
    padre  = trova_migliore_padre(prob_vector, &fro);
    valore = match(prob_vector, padre, &fro, &to);
    if (valore > AMB_TRESH) {
      F_work.array[to].match       = fro;
      F_reference.array[fro].match = to;
      F_work.array[to].color_id    = F_reference.array[fro].color_id;
      if ((F_work.array[to].color_id != 0) && (valore != 0)) {
        params.m_p       = TPoint(F_work.array[to].x, F_work.array[to].y);
        params.m_styleId = F_work.array[to].color_id;
        TTileSaverCM32 tileSaver(ras, tileSet);
        recomputeBBox = fill(ras, params, &tileSaver) || recomputeBBox;
      }
    }
  }
  /*..........................................................................*/
  /* Matching basato sulla probabilita' di colore                             */
  /*..........................................................................*/
  if (false) {
    bool recomputeBBox = false;
    for (i = 0; i < F_work.n; i++) {
      if (F_work.array[i].match < 0) {
        find_best(prob_vector, &col, i);
        F_work.array[i].match    = 1;
        F_work.array[i].color_id = col;
        if (F_work.array[i].color_id != 0) {
          params.m_p       = TPoint(F_work.array[i].x, F_work.array[i].y);
          params.m_styleId = F_work.array[i].color_id;

          TTileSaverCM32 tileSaver(ras, tileSet);
          recomputeBBox = fill(ras, params, &tileSaver) || recomputeBBox;
        }
      }
    }
  }
  free(prob_vector);
  return recomputeBBox;
}

/*---------------------------------------------------------------------------*/
/* Versione del Learning delle aree di Fabrizio                              */
/*...........................................................................*/
void autofill_learn(const TToonzImageP &imgToLearn)
/*---------------------------------------------------------------------------*/
{
  int i;
  double pbx, pby;
  double abx, aby;
  int tot_pix = 0;

  pbx = pby = 0.0;
  abx = aby = 0.0;

  TRasterCM32P ras = imgToLearn->getRaster();

  if (F_reference.array) {
    for (i = 0; i < F_reference.n; i++) free_list(&F_reference.array[i].vicini);
    free(F_reference.array);
  }

  F_reference.array = 0;
  F_reference.size  = 0;
  F_reference.n     = 0;
  F_reference.lx    = 0;
  F_reference.ly    = 0;

  scan_fabri_regions(ras, &F_reference, 0, 0, 0, 0, 0);

  for (i = 0; i < F_reference.n; i++) {
    F_reference.array[i].match = -1;
    F_reference.array[i].color_id =
        ras->pixels(F_reference.array[i].y)[F_reference.array[i].y].getPaint();
    pbx += BIG_TO_DOUBLE(F_reference.array[i].bx);
    pby += BIG_TO_DOUBLE(F_reference.array[i].by);
    abx = BIG_TO_DOUBLE(F_reference.array[i].bx);
    aby = BIG_TO_DOUBLE(F_reference.array[i].by);
    tot_pix += F_reference.array[i].npix;
  }
  F_ref_bx = pbx / tot_pix;
  F_ref_by = pby / tot_pix;
}

/*-- end of learn_fabrizio --------------------------------------------------*/
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/* Versione del matching delle aree di Fabrizio                              */
/*...........................................................................*/
bool autofill_apply(const TToonzImageP &imgToApply, bool selective,
                    TTileSetCM32 *tileSet)
/*---------------------------------------------------------------------------*/
{
  double pbx, pby;
  double abx, aby;
  int i, j;
  int tot_pix = 0;
  int *prob_vector;
  int fro, to;
  int col;
  int valore;
  int padre;
  //  int  temp_prob, att_match;

  if (F_reference.n <= 0 || F_reference.array == 0) return false;

  pbx = pby = 0.0;
  abx = aby = 0.0;

  TRasterCM32P ras = imgToApply->getRaster();

  /* Resetta gli eventuali accoppiamenti fatti precedentemente */
  for (i = 0; i < F_reference.n; i++) F_reference.array[i].match = -1;

  if (F_work.array) {
    for (i = 0; i < F_work.n; i++) free_list(&F_work.array[i].vicini);
    free(F_work.array);
  }
  F_work.array = 0;
  F_work.size  = 0;
  F_work.n     = 0;
  F_work.lx    = 0;
  F_work.ly    = 0;

  scan_fabri_regions(ras, &F_work, 0, 0, 0, 0, 0);

  if (abs(F_work.lx * F_work.ly - F_reference.lx * F_reference.ly) >
      0.1 * (F_work.lx * F_work.ly + F_reference.lx * F_reference.ly))
    return false;

  for (i = 0; i < F_work.n; i++) {
    F_work.array[i].match = -1;
    pbx += BIG_TO_DOUBLE(F_work.array[i].bx);
    pby += BIG_TO_DOUBLE(F_work.array[i].by);
    abx = BIG_TO_DOUBLE(F_work.array[i].bx);
    aby = BIG_TO_DOUBLE(F_work.array[i].by);
    tot_pix += F_work.array[i].npix;
  }

  F_wor_bx = pbx / tot_pix;
  F_wor_by = pby / tot_pix;

  prob_vector = (int *)calloc(3 * F_work.n * F_reference.n, sizeof(int));

  for (i = 0; i < F_reference.n; i++)
    for (j = 0; j < F_work.n; j++) assign_prob3(prob_vector, i, j);

  Dx_f = Dx_f / F_reference.n;
  DP_f = DP_f / F_reference.n;
  Df_f = Df_f / F_reference.n;

  Dx_t = Dx_t / F_work.n;
  DP_t = DP_t / F_work.n;
  Df_t = Df_t / F_work.n;

#ifdef SELECTIVE
  if (selective) {
    // Accoppia non trasparenti
    for (i = 0; i < F_work.n; i++)
      if (F_work.array[i].color_id != 0) {
        temp_prob = 0;
        att_match = F_reference.n;
        /* Se non verra' aggiornato in seguito non verra' *
* comunque piu' cambiato di colore               */
        for (j = 0; j < F_reference.n; j++)
          if ((F_reference.array[i].match < 0) &&
              ((prob_vector[i * F_reference.n + j] / 1000.0) *
                   (prob_vector[i * F_reference.n + j] / 1000.0) *
                   (prob_vector[2 * (F_work.n * F_reference.n) +
                                i * F_reference.n + j] /
                    1500.0) >
               temp_prob)) {
            att_match = j;
            temp_prob = (prob_vector[i * F_reference.n + j] / 1000.0) *
                        (prob_vector[i * F_reference.n + j] / 1000.0) *
                        (prob_vector[2 * (F_work.n * F_reference.n) +
                                     i * F_reference.n + j] /
                         1500.0);
          }
        F_work.array[i].match = att_match;
        if (att_match < F_reference.n) F_reference.array[att_match].match = i;
      }

    /*--------------------------------------------------------------------------*/
  }
#endif

  bool recomputeBBox = false;
  FillParameters params;
  params.m_emptyOnly = selective;
  for (i = 0; i < F_reference.n && i < F_work.n; i++) {
    padre  = trova_migliore_padre(prob_vector, &fro);
    valore = match(prob_vector, padre, &fro, &to);
    if (valore > AMB_TRESH) {
      F_work.array[to].match       = fro;
      F_reference.array[fro].match = to;
      F_work.array[to].color_id    = F_reference.array[fro].color_id;
      if ((F_work.array[to].color_id != 0) && (valore != 0)) {
        params.m_p       = TPoint(F_work.array[to].x, F_work.array[to].y);
        params.m_styleId = F_work.array[to].color_id;

        TTileSaverCM32 tileSaver(ras, tileSet);
        recomputeBBox = fill(ras, params, &tileSaver) || recomputeBBox;
      }
    }
    /*..........................................................................*/
    /* Matching basato sulla probabilita' di colore */
    /*..........................................................................*/
    if (false) {
      bool recomputeBBox = false;
      for (i = 0; i < F_work.n; i++) {
        if (F_work.array[i].match < 0) {
          find_best(prob_vector, &col, i);
          F_work.array[i].match    = 1;
          F_work.array[i].color_id = col;
          if (F_work.array[i].color_id != 0) {
            params.m_p       = TPoint(F_work.array[i].x, F_work.array[i].y);
            params.m_styleId = F_work.array[i].color_id;

            TTileSaverCM32 tileSaver(ras, tileSet);
            recomputeBBox = fill(ras, params, &tileSaver) || recomputeBBox;
          }
        }
      }
    }
  }
  free(prob_vector);
  return recomputeBBox;
}

/*-- end of apply_fabrizio --------------------------------------------------*/
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/* Scansione delle aree di Fabrizio                                          */
/*...........................................................................*/
static void scan_fabri_regions(TRasterCM32P ras,
                               struct s_fabri_region_list *rlst, int mode,
                               int ex1, int ey1, int ex2, int ey2)
/*---------------------------------------------------------------------------*/
{
  int firstfree;
  TPixelCM32 *pix;
  int xa, xb;
  int xp, x, y, x1, y1, x2, y2, tmp;
  int dx, dy, i, j, h, n, tone, maxtone;
  int rold, rcur, nold, ncur;
  int precedente;
  struct s_segm *row[2];
  int *ultima_zona;
  int region_id, region_old, keep, discard, vicina;
  bool border_tooX1, border_tooY1, border_tooX2, border_tooY2;

  x1           = ex1;
  x2           = ex2;
  y1           = ey1;
  y2           = ey2;
  TRect rect   = ras->getBounds();
  border_tooX1 = x1 == rect.x0;
  border_tooY1 = y1 == rect.y0;
  border_tooX2 = x2 == rect.x1;
  border_tooY2 = y2 == rect.y1;

  dx = x2 - x1 + 1;
  dy = y2 - y1 + 1;

  rlst->lx = dx;
  rlst->ly = dy;

  row[0]      = (struct s_segm *)calloc(dx, sizeof(struct s_segm));
  row[1]      = (struct s_segm *)calloc(dx, sizeof(struct s_segm));
  ultima_zona = (int *)calloc(dx, sizeof(int));
  rold        = 1;
  rcur        = 0;
  nold = ncur = 0;
  vicina      = -1;

  for (tmp = 0; tmp < dx; tmp++) ultima_zona[tmp] = -1;

  /*.. Inizializza la struttura
   * ...............................................*/
  rlst->size  = 100;
  rlst->array = (struct s_fabri_region *)calloc(rlst->size,
                                                sizeof(struct s_fabri_region));
  rlst->n   = 0;
  firstfree = -1;
  for (tmp = 0; tmp < rlst->size; tmp++) {
    rlst->array[tmp].vicini = NULL;
    rlst->array[tmp].holes  = 0;
  }
  /*...........................................................................*/

  for (y = y1; y <= y2; y++) {
    vicina = -1;
    pix    = ras->pixels(y) + x1;
    x      = x1;
    PRINTF("Cerca Area \n");
    while (x <= x2) {
      while (x <= x2 && pix->getTone() < pix->getMaxTone()) {
        pix++;
        x++;
      }              /*cerca una area */
      if (x <= x2) { /* non ho raggiunto il bordo */
        PRINTF("Prima del Bordo \n");
        xa      = x;
        maxtone = pix->getTone();
        xp      = x;
        while (x <= x2 && pix->getTone() >= pix->getMaxTone()) {
          tone = pix->getTone();
          if (tone > maxtone) {
            maxtone = tone;
            xp      = x;
          }
          pix++;
          x++;
        }
        xb = x - 1;

        PRINTF("Trovata Area \n");
        region_id = -1;

        for (j = 0; j < nold && row[rold][j].xa <= xb; j++) {
          if (row[rold][j].xb >= xa && row[rold][j].xa <= xb) {
            region_old = row[rold][j].region;
            if (region_id == region_old) rlst->array[region_id].holes++;
            if (region_id < 0) {
              region_id = region_old;
              rlst->array[region_id].per +=
                  2 * (xb - xa + 1) + 2 -
                  2 * (std::min(row[rold][j].xb, xb) -
                       std::max(row[rold][j].xa, xa) + 1);
            } else if (region_id != region_old) {
              /* CONTATTO FRA region_id E region_old */
              PRINTF("Contatto tra %d e %d \n", region_id, region_old);
              keep    = region_old;
              discard = region_id;
              if (keep > discard) {
                tmp     = keep;
                keep    = discard;
                discard = tmp;
              }
              /*.. UNISCO LE DUE REGIONI..................... */
              if (rlst->array[discard].x1 < rlst->array[keep].x1)
                rlst->array[keep].x1 = rlst->array[discard].x1;
              if (rlst->array[discard].y1 < rlst->array[keep].y1)
                rlst->array[keep].y1 = rlst->array[discard].y1;
              if (rlst->array[discard].x2 > rlst->array[keep].x2)
                rlst->array[keep].x2 = rlst->array[discard].x2;
              if (rlst->array[discard].y2 > rlst->array[keep].y2)
                rlst->array[keep].y2 = rlst->array[discard].y2;

              rlst->array[keep].holes += rlst->array[discard].holes;
              rlst->array[keep].npix += rlst->array[discard].npix;
              rlst->array[keep].per += rlst->array[discard].per -
                                       2 * (std::min(row[rold][j].xb, xb) -
                                            std::max(row[rold][j].xa, xa) + 1);

              fondi(rlst, keep, discard);

              ADD_BIG_BIG(rlst->array[keep].by, rlst->array[discard].by);
              ADD_BIG_BIG(rlst->array[keep].bx, rlst->array[discard].bx);
              ADD_BIG_BIG(rlst->array[keep].by2, rlst->array[discard].by2);
              ADD_BIG_BIG(rlst->array[keep].bx2, rlst->array[discard].bx2);
              if (rlst->array[keep].tone < rlst->array[discard].tone) {
                rlst->array[keep].x    = rlst->array[discard].x;
                rlst->array[keep].y    = rlst->array[discard].y;
                rlst->array[keep].tone = rlst->array[discard].tone;
              }
              /* OGNI RIFERIMENTO A 'discard' DIVENTA UN RIF.  A 'keep' */
              for (h = 0; h < nold; h++)
                if (row[rold][h].region == discard) row[rold][h].region = keep;
              for (h = 0; h < ncur; h++)
                if (row[rcur][h].region == discard) row[rcur][h].region = keep;
              region_id                                                 = keep;
              rinomina(discard, keep, rlst->n, rlst);
              for (tmp = 0; tmp < dx; tmp++)
                if (ultima_zona[tmp] == discard) ultima_zona[tmp] = keep;
              /* AGGIUNGO discard AL POOL DEI LIBERI */
              rlst->array[discard].active   = 0;
              rlst->array[discard].nextfree = firstfree;
              free_list(&rlst->array[discard].vicini);
              rlst->array[discard].vicini = NULL;

              firstfree = discard;
            } /* end elseif */
          }   /* end of if */
        }     /* end of for */
        PRINTF("Region ID %d %d %d %d \n", region_id, xa, xb, y);
        PRINTF("Memorizza Area \n");
        if (region_id >= 0) {
          rlst->array[region_id].lxb                                    = xb;
          rlst->array[region_id].lxa                                    = xa;
          if (xa < rlst->array[region_id].x1) rlst->array[region_id].x1 = xa;
          if (xb > rlst->array[region_id].x2) rlst->array[region_id].x2 = xb;
          if (y < rlst->array[region_id].y1) rlst->array[region_id].y1  = y;
          if (y > rlst->array[region_id].y2) rlst->array[region_id].y2  = y;
          rlst->array[region_id].npix += xb - xa + 1;
          ADD_BIG(rlst->array[region_id].by, (xb - xa + 1) * y);
          ADD_BIG(rlst->array[region_id].bx, (xb - xa + 1) * (xb + xa) / 2);
          ADD_BIG(rlst->array[region_id].by2, (xb - xa + 1) * y * y);
          ADD_BIG(rlst->array[region_id].bx2, somma_quadrati(xa, xb));
          if (rlst->array[region_id].tone < maxtone ||
              (rlst->array[region_id].x2 - rlst->array[region_id].x1) <
                  xb - xa) {
            rlst->array[region_id].x    = xp;
            rlst->array[region_id].y    = y;
            rlst->array[region_id].tone = maxtone;
          }
        } else {
          /* CREO UNA NUOVA REGIONE */
          if (firstfree < 0) {
            if (rlst->n >= rlst->size) {
              rlst->size += 50;
              rlst->array = (struct s_fabri_region *)realloc(
                  rlst->array, rlst->size * sizeof(struct s_fabri_region));
              for (tmp = rlst->size - 1; tmp >= rlst->size - 50; tmp--)
                rlst->array[tmp].vicini = NULL;
            }
            region_id = rlst->n++;
          } else {
            region_id = firstfree;
            firstfree = rlst->array[region_id].nextfree;
          }
          rlst->array[region_id].active   = 1;
          rlst->array[region_id].vicini   = NULL;
          rlst->array[region_id].nextfree = 0;
          rlst->array[region_id].x        = xp;
          rlst->array[region_id].y        = y;
          rlst->array[region_id].x1       = xa;
          rlst->array[region_id].y1       = y;
          rlst->array[region_id].x2       = xb;
          rlst->array[region_id].y2       = y;
          rlst->array[region_id].npix     = xb - xa + 1;
          rlst->array[region_id].holes    = 0;
          rlst->array[region_id].per      = 2 * (xb - xa + 1) + 2;
          rlst->array[region_id].lxb      = xb;
          rlst->array[region_id].lxa      = xa;
          ASS_BIG(rlst->array[region_id].by, (xb - xa + 1) * y);
          ASS_BIG(rlst->array[region_id].bx, (xb - xa + 1) * (xb + xa) / 2);
          ASS_BIG(rlst->array[region_id].by2, (xb - xa + 1) * y * y);
          ASS_BIG(rlst->array[region_id].bx2, somma_quadrati(xa, xb));
          rlst->array[region_id].tone     = maxtone;
          rlst->array[region_id].color_id = pix->getPaint();
        }

        /* MEMORIZZO LA REGIONE CORRENTE */
        precedente = -1;
        /*-----------------------------------------------------
.. Controlla se esiste vicinanza verticale ...........*/
        for (tmp = xa; tmp <= xb; tmp++) {
          if (ultima_zona[tmp - x1] != precedente) {
            aggiungi(ultima_zona[tmp - x1], region_id, rlst);
            precedente = ultima_zona[tmp - x1];
          }
          ultima_zona[tmp - x1] = region_id;
        }
        /*-----------------------------------------------------*/
        aggiungi(vicina, region_id, rlst);
        vicina = region_id;
        if (ncur >= dx - 1) {
          printf("autofill: row overflow dx=%d\n", dx);
          exit(1);
        }
        row[rcur][ncur].xa     = xa;
        row[rcur][ncur].xb     = xb;
        row[rcur][ncur].region = region_id;
        ncur++;
      } /* end of if#1 */
    }   /* end of while#1 */
    rcur = 1 - rcur;
    rold = 1 - rold;
    nold = ncur;
    ncur = 0;
  } /* end of for#1 */
  n = 0;
  /* mostra_vicini(rlst); */
  for (i = 0; i < rlst->n; i++)
    if ((rlst->array[i].active) && ((rlst->array[i].x1 > x1 || border_tooX1) &&
                                    (rlst->array[i].x2 < x2 || border_tooX2) &&
                                    (rlst->array[i].y1 > y1 || border_tooY1) &&
                                    (rlst->array[i].y2 < y2 || border_tooY2))) {
      if (n < i) {
        rlst->array[n]        = rlst->array[i];
        rlst->array[n].vicini = rlst->array[i].vicini;
        rinomina(i, n, rlst->n, rlst);
        for (tmp                                      = 0; tmp < dx; tmp++)
          if (ultima_zona[tmp] == i) ultima_zona[tmp] = n;
      }
      rlst->array[n].lx = rlst->array[n].x2 - rlst->array[n].x1 + 1;
      rlst->array[n].ly = rlst->array[n].y2 - rlst->array[n].y1 + 1;
      rlst->array[n].xm = (rlst->array[n].x2 + rlst->array[n].x1) / 2;
      rlst->array[n].ym = (rlst->array[n].y2 + rlst->array[n].y1) / 2;
      n++;
    } else
      rimuovi_tutti(i, rlst);
  /* mostra_vicini(rlst); */
  rlst->n = n;
  free(row[0]);
  free(row[1]);
  free(ultima_zona);
}
/*- end of scan_fabri_regions --------------------------------------------*/
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
/*  Rinomina tutti i vicini regione1 in regione2                             */
/*...........................................................................*/
static void rinomina(int regione1, int regione2, int num_regioni,
                     struct s_fabri_region_list *rlst)
/*---------------------------------------------------------------------------*/
{
  struct vicine *appo, *old, appo1;
  int i;
  bool trovato = false;

  PRINTF("Rinomino %d %d Regioni: %d \n", regione1, regione2, num_regioni);
  /* mostra_vicini(rlst);  */
  for (i = 0; i < num_regioni; i++)
    if (rlst->array[i].active) {
      trovato = ((i == regione2) || (i == regione1));
      PRINTF(">> %d \n", i);
      old  = NULL;
      appo = rlst->array[i].vicini;
      while (appo != NULL) {
        if ((appo->region_id == regione2) || (appo->region_id == regione1)) {
          if (trovato)
            if (old != NULL) {
              PRINTF("A%d %d \n", i, appo->region_id);
              old->next = appo->next;
              free(appo);
              appo = old->next;
              /* mostra_vicini(rlst); */
            } else {
              PRINTF("B%d %d \n", i, appo->region_id);
              rlst->array[i].vicini = appo->next;
              appo1                 = *appo;
              free(appo);
              appo = appo1.next;
            }
          else /* non ho ancora trovato */
          {
            PRINTF("S%d \n", appo->region_id);
            appo->region_id = regione2;
            old             = appo;
            appo            = appo->next;
            trovato         = true;
          }
        } else /* region_id non e' region2 o regione 1 */
        {
          PRINTF("D%d \n", appo->region_id);
          old  = appo;
          appo = appo->next;
        }
      }
    }
  PRINTF("Rinomino %d %d\n", regione1, regione2);
  /*   mostra_vicini(rlst);  */
}

/*- end of rinomina ---------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/* Aggiunge a regione2 il vicino regione1 e viceversa                        */
/*...........................................................................*/
static void aggiungi(int regione1, int regione2,
                     struct s_fabri_region_list *rlst)
/*---------------------------------------------------------------------------*/
{
  struct vicine *appo1, *appo2;

  /*  if((regione1==0)||(regione2==0)) */
  PRINTF("Aggiungo %d a %d\n", regione1, regione2);
  if ((regione1 != -1) && (regione2 != -1) && (regione2 != regione1)) {
    if (rlst->array[regione1].active) {
      appo1 = rlst->array[regione1].vicini;
      while ((appo1 != NULL) && (appo1->region_id != regione2))
        appo1 = appo1->next;
      if (appo1 == NULL) {
        appo1            = (struct vicine *)calloc(1, sizeof(struct vicine));
        appo1->next      = rlst->array[regione1].vicini;
        appo1->region_id = regione2;
        rlst->array[regione1].vicini = appo1;
      }
    }

    if (rlst->array[regione2].active) {
      appo2 = rlst->array[regione2].vicini;
      while ((appo2 != NULL) && (appo2->region_id != regione1))
        appo2 = appo2->next;
      if (appo2 == NULL) {
        appo2            = (struct vicine *)calloc(1, sizeof(struct vicine));
        appo2->next      = rlst->array[regione2].vicini;
        appo2->region_id = regione1;
        rlst->array[regione2].vicini = appo2;
      }
    }
  }
  PRINTF("Aggiunto %d a %d\n", regione1, regione2);
}

/*- end of aggiungi ---------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/*  Aggiunge i vicini di r2 a quelli di r1                                   */
/*...........................................................................*/
static void fondi(struct s_fabri_region_list *rlst, int r1, int r2)
/*---------------------------------------------------------------------------*/
{
  struct vicine *appo1, *appo2, *appo3;

  PRINTF("Fondi %d %d \n", r1, r2);
  /* mostra_vicini(rlst); */
  appo2 = rlst->array[r2].vicini;
  while (appo2 != NULL) {
    appo1 = rlst->array[r1].vicini;
    while ((appo1 != NULL) && (appo1->region_id != appo2->region_id) &&
           (appo2->region_id != r1))
      appo1 = appo1->next;
    if (appo1 == NULL) {
      appo3            = (struct vicine *)calloc(1, sizeof(struct vicine));
      appo3->next      = rlst->array[r1].vicini;
      appo3->region_id = appo2->region_id;
      rlst->array[r1].vicini = appo3;
      appo2                  = appo2->next;
    } else {
      appo2 = appo2->next;
    }
  }
  PRINTF("Fuso %d %d \n", r1, r2);
  /* mostra_vicini(rlst); */
}

/*- end of fondi ------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/*  Mostra i vicini di ogni regione                                          */
/*...........................................................................*/
static void mostra_vicini(struct s_fabri_region_list *rlst)
/*---------------------------------------------------------------------------*/
{
  int i;
  struct vicine *appo;

  for (i = 0; i < rlst->n; i++)
    if (rlst->array[i].active) {
      PRINTF("Region: %d ::", i);
      appo = rlst->array[i].vicini;
      while (appo != NULL) {
        PRINTF("%d ", appo->region_id);
        appo = appo->next;
      }
      PRINTF("\n");
    } else
      PRINTF("Region: %d inactive \n", i);
}

/*- end of mostra_vicini ----------------------------------------------------*/
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/*  Rimuove r1 come vicino in ogni regione                                   */
/*...........................................................................*/
static void rimuovi_tutti(int r1, struct s_fabri_region_list *rlst)
/*---------------------------------------------------------------------------*/
{
  struct vicine *appo, *old;
  int i;

  PRINTF("Elimino %d \n", r1);
  /* mostra_vicini(rlst);  */
  for (i = 0; i < rlst->n; i++)
    if (rlst->array[i].active) {
      PRINTF(">> %d \n", i);
      old  = NULL;
      appo = rlst->array[i].vicini;
      while (appo != NULL) {
        if (appo->region_id == r1) {
          PRINTF("A%d %d\n", appo->region_id, old->region_id);
          if (old != NULL) {
            old->next = appo->next;
            /* free(appo); */
            appo = old->next;
            /* mostra_vicini(rlst); */
          } else {
            rlst->array[i].vicini = appo->next;
            /* free(appo); */
            appo = rlst->array[i].vicini;
          }
        } else /* region_id non e' region2 o regione 1 */
        {
          PRINTF("D%d \n", appo->region_id);
          old  = appo;
          appo = appo->next;
        }
      }
    }
  /*  mostra_vicini(rlst); */
}

/*- end of rimuovi_tutti ----------------------------------------------------*/
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/*  Assegna la probabilita' di transizione i->j    (3)                       */
/*...........................................................................*/
static void assign_prob3(int prob[], int i, int j)
/*---------------------------------------------------------------------------*/
{
  double delta_posx1, delta_posy1, delta_posx2, delta_posy2;
  double delta_momx1, delta_momy1, delta_momx2, delta_momy2;
  int delta_pix, delta_pix_max;
  double delta_pos, delta_pos_max, delta_forma_mom;
  int delta_forma1, delta_forma2, delta_forma;

  delta_posx1 =
      BIG_TO_DOUBLE(F_reference.array[i].bx) / F_reference.array[i].npix -
      F_ref_bx;
  delta_posy1 =
      BIG_TO_DOUBLE(F_reference.array[i].by) / F_reference.array[i].npix -
      F_ref_by;

  delta_posx2 =
      BIG_TO_DOUBLE(F_work.array[j].bx) / F_work.array[j].npix - F_wor_bx;
  delta_posy2 =
      BIG_TO_DOUBLE(F_work.array[j].by) / F_work.array[j].npix - F_wor_by;

  /* Cosi' calcolo il modulo della differenza */

  delta_pos = sqrt((delta_posx2 - delta_posx1) * (delta_posx2 - delta_posx1) +
                   (delta_posy2 - delta_posy1) * (delta_posy2 - delta_posy1));
  delta_pos_max = sqrt((double)(F_work.lx * F_work.lx + F_work.ly * F_work.ly));

  prob[i + j * F_reference.n] =
      ROUNDP(1000 * (1 - (delta_pos / delta_pos_max)));

  delta_pix = abs(F_reference.array[i].npix - F_work.array[j].npix);
  delta_pix_max =
      std::max((F_work.lx * F_work.ly), (F_reference.lx * F_reference.ly));

  prob[(F_work.n * F_reference.n) + i + (j * F_reference.n)] =
      ROUNDP(1000 * (1 - ((double)delta_pix /
                          (F_reference.array[i].npix + F_work.array[j].npix))));

  delta_momx1 =
      BIG_TO_DOUBLE(F_reference.array[i].bx2) / F_reference.array[i].npix -
      (BIG_TO_DOUBLE(F_reference.array[i].bx) / F_reference.array[i].npix *
       BIG_TO_DOUBLE(F_reference.array[i].bx) / F_reference.array[i].npix);

  delta_momy1 =
      BIG_TO_DOUBLE(F_reference.array[i].by2) / F_reference.array[i].npix -
      (BIG_TO_DOUBLE(F_reference.array[i].by) / F_reference.array[i].npix *
       BIG_TO_DOUBLE(F_reference.array[i].by) / F_reference.array[i].npix);

  delta_momx2 = BIG_TO_DOUBLE(F_work.array[j].bx2) / F_work.array[j].npix -
                (BIG_TO_DOUBLE(F_work.array[j].bx) / F_work.array[j].npix *
                 BIG_TO_DOUBLE(F_work.array[j].bx) / F_work.array[j].npix);
  delta_momy2 = BIG_TO_DOUBLE(F_work.array[j].by2) / F_work.array[j].npix -
                (BIG_TO_DOUBLE(F_work.array[j].by) / F_work.array[j].npix *
                 BIG_TO_DOUBLE(F_work.array[j].by) / F_work.array[j].npix);

  delta_forma_mom =
      abs(sqrt(delta_momx1 + delta_momy1) - sqrt(delta_momx2 + delta_momy2));

  delta_forma1 = ROUNDP(
      1000 * (((double)F_reference.array[i].per / F_reference.array[i].npix -
               2 / sqrt((double)F_reference.array[i].npix / 3.14)) /
              (2 - 2 / sqrt((double)F_reference.array[i].npix / 3.14))));

  delta_forma2 =
      ROUNDP(1000 * (((double)F_work.array[j].per / F_work.array[j].npix -
                      2 / sqrt((double)F_work.array[j].npix / 3.14)) /
                     (2 - 2 / sqrt((double)F_work.array[j].npix / 3.14))));

  delta_forma = ROUNDP(1000 * (1 - (delta_forma_mom / delta_pos_max)));

  prob[(2 * F_work.n * F_reference.n) + i + (j * F_reference.n)] = delta_forma;

  Dx_f += ROUNDP(sqrt(delta_posx1 * delta_posx1 + delta_posy1 * delta_posy1));
  DP_f += F_reference.array[i].npix;
  Df_f += ROUNDP(sqrt(delta_momx1 * delta_momx1 + delta_momy1 * delta_momy1));

  Dx_t += ROUNDP(sqrt(delta_posx2 * delta_posx2 + delta_posy2 * delta_posy2));
  DP_t += F_work.array[j].npix;
  Df_t += ROUNDP(sqrt(delta_momx2 * delta_momx2 + delta_momy2 * delta_momy2));
}
/*- end of assign_prob ------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/*  Trova il colore migliore per una regione                                 */
/*...........................................................................*/
static void find_best(int prob[], int *col, int to)
/*---------------------------------------------------------------------------*/
{
  float att_max = 0.0;
  int i, j, numero;
  float color_value;

  for (i = 0; i < F_reference.n; i++) {
    color_value = 0.0;
    numero      = 0;
    for (j = 0; j < F_reference.n; j++) {
      if (F_reference.array[i].color_id == F_reference.array[j].color_id) {
        color_value +=
            (prob[to * F_reference.n + j] / 1000.0) *
            (prob[to * F_reference.n + j] / 1000.0) *
            /*   (prob[(F_work.n*F_reference.n)+to*F_reference.n+j]/1500.0)*
             */ (prob[2 * (F_work.n * F_reference.n) + to * F_reference.n + j] /
                 1500.0);
        numero++;
      }
    }
    if ((color_value / numero) > att_max) {
      att_max = color_value / numero;
      PRINTF("Nuovo Massimo %f \n", att_max);
      *col = F_reference.array[i].color_id;
    }
  }
}
/*- end of find_best --------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/*  libera la memoria occupata da una lista                                  */
/*...........................................................................*/
static void free_list(struct vicine **vic)
/*---------------------------------------------------------------------------*/
{
  if (*vic != NULL) {
    free_list(&(*vic)->next);
    free(*vic);
    *vic = NULL;
  }
}
/*- end of free_list --------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/*  Somma il quadrato dei numeri da x a y o viceversa                        */
/*...........................................................................*/
static int somma_quadrati(int x, int y)
/*---------------------------------------------------------------------------*/
{
  int somma = 0;
  int i;

  for (i = std::min(x, y); i <= std::max(x, y); i++) somma += i * i;
  return somma;
}
/*- end of somma_quadrati ---------------------------------------------------*/
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/*  Scegli il punto da cui proseguire l'accoppiamento                        */
/*...........................................................................*/
static int trova_migliore_padre(int prob_vector[], int *att_my)
/*---------------------------------------------------------------------------*/
{
  int att_best, att_second, att_max_diff = 0;
  int att_choice = -1, i, j, k;
  struct vicine *a1, *a2;

  PRINTF("Inizio Ricerca del Padre \n");
  for (k = 0; k < F_reference.n; k++)
    if (F_reference.array[k].match >= 0) {
      PRINTF("%d Gia' Accoppiato\n", k);
      a1 = F_reference.array[k].vicini;
      while (a1 != NULL) {
        i = a1->region_id;
        if (F_reference.array[i].match < 0) {
          PRINTF("%d vicino di %d Non Accoppiato\n", i, k);
          a2         = F_work.array[F_reference.array[k].match].vicini;
          att_best   = 0;
          att_second = 0;
          while (a2 != NULL) {
            PRINTF("Coppia %d %d \n", i, j);
            j = a2->region_id;
            if (F_work.array[j].match < 0) {
              if (prob_vector[j * F_reference.n + i] *
                      prob_vector[(F_work.n * F_reference.n) +
                                  j * F_reference.n + i] *
                      prob_vector[2 * (F_work.n * F_reference.n) +
                                  j * F_reference.n + i] >
                  att_second)
                att_second = prob_vector[j * F_reference.n + i] *
                             prob_vector[(F_work.n * F_reference.n) +
                                         j * F_reference.n + i] *
                             prob_vector[2 * (F_work.n * F_reference.n) +
                                         j * F_reference.n + i];

              if (prob_vector[j * F_reference.n + i] *
                      prob_vector[(F_work.n * F_reference.n) +
                                  j * F_reference.n + i] *
                      prob_vector[2 * (F_work.n * F_reference.n) +
                                  j * F_reference.n + i] >
                  att_best) {
                att_second = att_best;
                att_best   = prob_vector[j * F_reference.n + i] *
                           prob_vector[(F_work.n * F_reference.n) +
                                       j * F_reference.n + i] *
                           prob_vector[2 * (F_work.n * F_reference.n) +
                                       j * F_reference.n + i];
              }
            }
            a2 = a2->next;
          }
          if ((att_best - att_second) > att_max_diff) {
            att_max_diff = att_best - att_second;
            att_choice   = F_reference.array[k].match;
            *att_my      = i;
            PRINTF("Nuovo Coeff.: %d \n", att_max_diff);
          }
        }
        a1 = a1->next;
      }
    }
  PRINTF("Finita Ricerca del Padre \n");
  return att_choice;
}

/*---------------------------------------------------------------------------*/
static int match(int prob[], int padre, int *fro, int *to)
/*---------------------------------------------------------------------------*/
{
  int att_max = 0, i, j;
  struct vicine *a2;

  if (padre > -1) {
    a2 = F_work.array[padre].vicini;
    while (a2 != NULL) {
      j = a2->region_id;
      if ((F_work.array[j].match < 0) &&
          (prob[j * F_reference.n + (*fro)] *
               prob[(F_work.n * F_reference.n) + j * F_reference.n + (*fro)] *
               prob[2 * (F_work.n * F_reference.n) + j * F_reference.n +
                    (*fro)] >
           att_max)) {
        att_max =
            prob[j * F_reference.n + (*fro)] *
            prob[(F_work.n * F_reference.n) + j * F_reference.n + (*fro)] *
            prob[2 * (F_work.n * F_reference.n) + j * F_reference.n + (*fro)];
        *to = j;
      }
      a2 = a2->next;
    }
  } else {
    for (i = 0; i < F_reference.n; i++)
      for (j = 0; j < F_work.n; j++)
        if ((F_work.array[j].match < 0) && (F_reference.array[i].match < 0) &&
            (F_work.array[j].npix > DIM_TRESH * F_work.lx * F_work.ly) &&
            (F_reference.array[i].npix >
             DIM_TRESH * F_reference.lx * F_reference.ly)) {
          if (prob[j * F_reference.n + i] *
                  prob[(F_work.n * F_reference.n) + j * F_reference.n + i] *
                  prob[2 * (F_work.n * F_reference.n) + j * F_reference.n + i] >
              att_max) {
            att_max =
                prob[j * F_reference.n + i] *
                prob[(F_work.n * F_reference.n) + j * F_reference.n + i] *
                prob[2 * (F_work.n * F_reference.n) + j * F_reference.n + i];
            *fro = i;
            *to  = j;
          }
        }
  }
  return att_max;
}
