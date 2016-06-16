#pragma once

#ifndef _UDIT_H
#define _UDIT_H

#include "pixel.h"
#include "toonz.h"

/*------------------------------------------------------------------------*/

/****************** UDIT 4.1 VERSION **************************/

/**************************************************************/
/*********   GENERAL STRUCTURE DECLARATIONS   *****************/
/**************************************************************/

typedef struct UD_RECT {
  int lx, ly;
  double x_pos, y_pos;
} UD_RECT;

typedef struct UD_RENDER_INFO {
  int xsize, ysize;
  int shrink;
} UD_RENDER_INFO;

/**************************************************************/
/************  USER PIXEL TYPES DECLARATIONS   ****************/
/**************************************************************/

#define UD_PIXEL32 LPIXEL
#define UD_PIXEL64 SPIXEL

typedef USHORT UD_CMAPINDEX;

/**************************************************************/
/**********  USER IMAGE STRUCTURES DECLARATIONS   *************/
/**************************************************************/

typedef enum UD_IMG_TYPE {
  UD_NONE = 0,
  UD_CMAPPED,
  UD_RGBM32,
  UD_RGBM64
} UD_IMG_TYPE;

typedef union UD_PIXEL_PTR {
  UD_PIXEL32 p32;
  UD_PIXEL64 p64;
  UD_CMAPINDEX ci;
} UD_PIXEL_PTR;

typedef struct UD_COLORMAP {
  UD_PIXEL32 *buffer;
  int offset, size;
} UD_COLORMAP;

typedef struct UD_USR_IMAGE {
  enum UD_IMG_TYPE type;
  UD_PIXEL_PTR *buffer;
  int scanline;
  int lx, ly;
  double x_pos, y_pos;
  UD_COLORMAP cmap;
} UD_USR_IMAGE;

/**************************************************************/
/*********  UDIT FUNCTION PROTOTYPES DECLARATIONS   ***********/
/**************************************************************/

enum UD_GOP_TYPE { UD_GOP_SOURCE_TO_DEST, UD_GOP_DEST_TO_SOURCE };

typedef int UD_GOP_FUNC_TYPE(enum UD_GOP_TYPE type, int num_rectin,
                             UD_RECT src_rects[], UD_RECT *dst_rect, int argc,
                             const char *argv[], const UD_RENDER_INFO *info);

typedef int UD_ROP_FUNC_TYPE(int num_imgs_in, const UD_USR_IMAGE imgs_in[],
                             const UD_USR_IMAGE *img_out, int argc,
                             const char *argv[], const UD_RENDER_INFO *info);

typedef struct UD_DATA_V41U1 {
  char *version;
  char *udit_name;
  UD_GOP_FUNC_TYPE *gop;
  UD_ROP_FUNC_TYPE *rop;
  int num_args;
  int num_params;
} UD_DATA_V41U1;

/*------------------------------------------------------------------------*/
/*------------------------------------------------------------------------*/
/*------------------------------------------------------------------------*/

/****************** UDIT 4.4 VERSION **************************/

/**************************************************************/
/*********   GENERAL STRUCTURE DECLARATIONS   *****************/
/**************************************************************/

typedef struct UD44_RENDER_INFO {
  int xsize, ysize;
  int shrink;
  int frame;
} UD44_RENDER_INFO;

/**************************************************************/
/************  USER PIXEL TYPES DECLARATIONS   ****************/
/**************************************************************/

#define UD44_PIXEL32 LPIXEL
#define UD44_PIXEL64 SPIXEL

typedef USHORT UD44_CMAPINDEX16;
typedef TUINT32 UD44_CMAPINDEX32;

/**************************************************************/
/**********  USER IMAGE STRUCTURES DECLARATIONS   *************/
/**************************************************************/

typedef enum UD44_IMG_TYPE {
  UD44_NONE = 0,
  UD44_CM16,
  UD44_CM32,
  UD44_RGBM32,
  UD44_RGBM64
} UD44_IMG_TYPE;

typedef union UD44_PIXEL_PTR {
  UD44_PIXEL32 p32;
  UD44_PIXEL64 p64;
  UD44_CMAPINDEX16 ci16;
  UD44_CMAPINDEX32 ci32;
} UD44_PIXEL_PTR;

typedef struct UD44_COLORMAP {
  UD44_PIXEL32 *buffer16;
  UD44_PIXEL32 *penbuf32; /* for > 16 bits, pencil component buffer (") */
  UD44_PIXEL32 *colbuf32; /* for > 16 bits, color  component buffer (") */
  int offset, size;
} UD44_COLORMAP;

typedef struct UD44_USR_IMAGE {
  enum UD44_IMG_TYPE type;
  UD44_PIXEL_PTR *buffer;
  int scanline;
  int lx, ly;
  double x_pos, y_pos;
  UD44_COLORMAP cmap;
  int img_lx, img_ly;
  char *fullpath;
  double img_x_pos, img_y_pos; /*WARNING: leggi sotto!!! */
} UD44_USR_IMAGE;

/* WARNING: i due campi img_x_pos, img_y_pos sono stati aggiunti alla fine,
            e per questo valgono SOLO per le UDIT dei plugin e fx.
            essi rappresentano, insieme a img_lx, img_ly
            (che quindi hanno un significato diverso nel caso UDIT user
   defined),
            il rect che contiene  TUTTA l'immagine di input transformata dagli
             fx seguenti alla udit corrente.
            Quindi, il rect rappresentato da lx, ly, x_pos, y_pos
            deve essere contenuto  o coincidente con il rect suddetto.  */

/**************************************************************/
/*********  UDIT FUNCTION PROTOTYPES DECLARATIONS   ***********/
/**************************************************************/

enum UD44_GOP_TYPE { UD44_GOP_SOURCE_TO_DEST, UD44_GOP_DEST_TO_SOURCE };

typedef int UD44_GOP_FUNC_TYPE(enum UD44_GOP_TYPE type, int num_rectin,
                               UD44_USR_IMAGE src_rects[],
                               UD44_USR_IMAGE *dst_rect, int argc,
                               const char *argv[],
                               const UD44_RENDER_INFO *info);

typedef int UD44_ROP_FUNC_TYPE(int num_imgs_in, const UD44_USR_IMAGE imgs_in[],
                               const UD44_USR_IMAGE *img_out, int argc,
                               const char *argv[],
                               const UD44_RENDER_INFO *info);

typedef struct UDIT_DATA_V44U1 {
  char *version;
  char *udit_name;
  UD44_GOP_FUNC_TYPE *gop;
  UD44_ROP_FUNC_TYPE *rop;
  int num_args;
  int num_params;
} UDIT_DATA_V44U1;

#define UD44_RECT UD44_USR_IMAGE

#endif
