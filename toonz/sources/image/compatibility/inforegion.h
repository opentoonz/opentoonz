#pragma once

#ifndef __INFO_REGION_H__
#define __INFO_REGION_H__

#define TNZ_TOPLEFT 1
#define TNZ_TOPRIGHT 2
#define TNZ_BOTRIGHT 3
#define TNZ_BOTLEFT 4
#define TNZ_LEFTOP 5
#define TNZ_RIGHTOP 6
#define TNZ_RIGHTBOT 7
#define TNZ_LEFTBOT 8

typedef struct {
  int x1, y1, x2, y2;
  int x_offset, y_offset;         /*  offset all'interno della regione   */
  int xsize, ysize;               /*      dimensioni della regione       */
  int scanNrow, scanNcol;         /* righe e col. dell'immagine da scan. */
  int startScanRow, startScanCol; /*   offset nell'immagine da scandire  */
  int step;                       /*          fattore di scale           */
  int lx_in, ly_in;               /*    dimensioni immag. da scandire    */
} INFO_REGION;

typedef struct {
  int x1, y1, x2, y2;
  int x_offset, y_offset;         /*  offset all'interno della regione   */
  int xsize, ysize;               /*      dimensioni della regione       */
  int scanNrow, scanNcol;         /* righe e col. dell'immagine da scan. */
  int startScanRow, startScanCol; /*   offset nell'immagine da scandire  */
  int step;                       /*          fattore di scale           */
  int lx_in, ly_in;               /*    dimensioni immag. da scandire    */
  int verso_x, verso_y;           /* verso di scrittura nel buffer dest. */
  int buf_inc;                    /* incremento tra due pix. consecutivi */
  int sxpix, expix, sypix, eypix; /* pixel estremi del buffer di input   */
} EXT_INFO_REGION;

int get_info_region(EXT_INFO_REGION *region, int x1_out, int y1_out, int x2_out,
                    int y2_out, int scale, int width_in, int height_in,
                    int orientation);

void getInfoRegion(INFO_REGION *region, int x1_out, int y1_out, int x2_out,
                   int y2_out, int scale, int width_in, int height_in);

void print_info_region(EXT_INFO_REGION *region);

#endif /* __INFO_REGION_H__ */
