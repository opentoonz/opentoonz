

h64020
s 00077 / 00001 / 08351
d D 1.105 00 / 11 / 24 20 : 06 : 53 vincenzo 105 104
c rop_mirror per cmapped
e
s 00015 / 00003 / 08337
d D 1.104 00 / 02 / 18 15 : 59 : 01 max 104 103
c fix baco in rop_premultiply RGBM64 RGBM64;
rop_copy funz.su RAS_BW, RAS_BW
e
s 00001 / 00000 / 08339
d D 1.103 00 / 01 / 12 12 : 08 : 57 vincenzo 103 102
c rop_copy CM24->GR16
e
s 00005 / 00000 / 08334
d D 1.102 99 / 11 / 25 11 : 02 : 20 max 102 101
c rop mirror gestisce WB e BW
e
s 00002 / 00000 / 08332
d D 1.101 99 / 11 / 12 16 : 22 : 09 vincenzo 101 100
c rop_copy_cm16s12->rgbm
e
s 00002 / 00001 / 08330
d D 1.100 99 / 10 / 28 10 : 10 : 09 max 100 99
c rop_mirror supporta anche rastype RAS_WB
e
s 00016 / 00000 / 08315
d D 1.99 99 / 08 / 20 09 : 53 : 22 vincenzo 99 98
c messaggi(disattivati) di diagnostica sulla memoria
e
s 00090 / 00000 / 08225
d D 1.98 99 / 05 / 17 17 : 28 : 42 vincenzo 98 97
c CM16S12
e
s 00006 / 00021 / 08219
d D 1.97 99 / 04 / 01 17 : 59 : 05 vincenzo 97 96
c conversioni varie utilizzate per salvare i.mov
e
s 00374 / 00191 / 07866
d D 1.96 99 / 03 / 26 19 : 19 : 15 vincenzo 96 95
c rop_xrgb1555
e
s 00484 / 00000 / 07573
d D 1.95 99 / 03 / 03 16 : 08 : 21 max 95 94
c + rop_copy rgbx64->rgbx, rgbx64->rgb16;
+ rop_zoom_out rgbx64->rgbx, rgbx64->rgb16, rgbm64->rgbm
e
s 00013 / 00009 / 07560
d D 1.94 99 / 02 / 25 17:04:34 tross 94 93
c rop_subimage_to_raster anche per RGBM64
e
s 00007 / 00006 / 07562
d D 1.93 99 / 02 / 20 15:51:08 tross 93 92
c corretta rop_clear(sbagliava se y1 != 0)
e
s 00001 / 00001 / 07567
d D 1.92 99 / 02 / 17 19:07:11 vincenzo 92 91
c la clone_raster fa anche gli extra
e
s 00020 / 00016 / 07548
d D 1.91 99 / 02 / 15 17:31:49 tross 91 90
c corretta la clone_raster per CM
e
s 00029 / 00002 / 07535
d D 1.90 99 / 02 / 15 17:22:14 vincenzo 90 89
c RAS_GR16
e
s 00713 / 00426 / 06824
d D 1.89 99 / 01 / 11 18:03:57 tross 89 88
c migliorati extra:corretti bachi e aggiunte funz -
                           corretta
                               rop_custom_fill_cmap_penramp(baco tcheck cm24)
e
s 00034 / 00002 / 07216
d D 1.88 98 / 12 / 14
                        16:
                        21:17 vincenzo 88 87
c ristrutturazione megagalattica per le dll
e
s 00013 / 00000 / 07205
d D 1.87 98 / 10 / 23
                        17:
                        21:44 vincenzo 87 86
c convert_raster
e
s 00007 / 00001 / 07198
d D 1.86 98 / 09 / 24
                        18:
                        32:38 tross 86 85
c messa cmap.info in create_raster()
e
s 00003 / 00000 / 07196
d D 1.85 98 / 09 / 14
                        16:
                        15:32 vincenzo 85 84
c clone_raster mette anchge la cmap
e
s 01500 / 00701 / 05696
d D 1.84 98 / 09 / 14
                        15:
                        57:39 tross 84 83
c modifiche per CM24, velocizzati[un] premult_lpixel
e
s 00044 / 00064 / 06353
d D 1.83 98 / 08 / 14
                        17:
                        14:44 tross 83 82
c tolti UNICM
e
s 00013 / 00014 / 06404
d D 1.82 98 / 08 / 14
                        14:
                        39:55 tross 82 81
c tolto cmap.size
e
s 00348 / 00177 / 06070
d D 1.81 98 / 07 / 16
                        20:
                        02:00 tross 81 80
c rop_copy e rop_zoom_out rgb_rgb16
e
s 00028 / 00000 / 06219
d D 1.80 98 / 07 / 10
                        18:
                        32:08 vincenzo 80 79
c create_subraster e clone_raster
e
s 00005 / 00014 / 06214
d D 1.79 98 / 06 / 09
                        17:
                        29:54 vincenzo 79 78
c eliminata struct cmap_color
e
s 00025 / 00025 / 06203
d D 1.78 98 / 03 / 26
                        01:
                        07:03 tross 78 77
c assert(FALSE)-- > abort()per problemi di GP piena
e
s 00038 / 00000 / 06190
d D 1.77 98 / 03 / 13
                        18:
                        35:34 vincenzo 77 76
c create_raster e release_raster
e
s 00002 / 00002 / 06188
d D 1.76 98 / 03 / 09
                        15:
                        00:15 vincenzo 76 75
c rop_image_to_(sub)raster non inizializzava native_buffer
e
s 00004 / 00004 / 06186
d D 1.75 98 / 02 / 23
                        12:
                        35:57 vincenzo 75 74
c sbagliata scansione raster su premultiply
e
s 00082 / 00002 / 06108
d D 1.74 98 / 02 / 23
                        11:
                        15:21 tross 74 73
c rop_premultiply()
e
s 00056 / 00051 / 06054
d D 1.73 97 / 12 / 01
                        18:
                        26:04 vincenzo 73 72
c rgbx->bw_q, cm16->bw_q
e
s 00020 / 00000 / 06085
d D 1.72 97 / 11 / 28
                        17:
                        31:32 vincenzo 72 71
c gestione RAS_CMxSy(copy, pixbytes pix_to_ras etc)
e
s 00017 / 00001 / 06068
d D 1.71 97 / 11 / 18
                        16:
                        42:31 tross 71 70
c rop_copy RAS_CMxSy
e
s 00033 / 00001 / 06036
d D 1.70 97 / 09 / 26
                        12:
                        25:32 vincenzo 70 69
c rop_copy_rgb16_rgbm
e
s 00010 / 00007 / 06027
d D 1.69 97 / 09 / 23
                        15:
                        53:42 tross 69 68
c migliorata la rop_copy_bw
e
s 00043 / 00003 / 05991
d D 1.68 97 / 09 / 23
                        00:
                        00:23 tross 68 67
c rop_copy_bw_bw
e
s 00038 / 00000 / 05956
d D 1.67 97 / 09 / 22
                        22:
                        20:35 tross 67 66
c rop_copy_rgbx_rgb(vincenzo)
e
s 00006 / 00016 / 05950
d D 1.66 97 / 04 / 01
                        18:
                        46:33 vincenzo 66 65
c esportata la dither
e
s 00037 / 00021 / 05929
d D 1.65 97 / 03 / 27
                        19:
                        54:27 tross 65 64
c rop_copy_rgbm64_rgbm con random round
e
s 00001 / 00001 / 05949
d D 1.64 97 / 01 / 03
                        15:
                        10:39 roberto 64 63
c Corretto bug in rop_copy_90(CASE-- __OR)
e
s 00453 / 00002 / 05497
d D 1.63 96 / 11 / 24
                        22:
                        36:00 tross 63 62
c rop_copy_90_rgb_rgb16, rop_zoom_out_90_rgb_rgb16 / rgbm
e
s 00381 / 00003 / 05118
d D 1.62 96 / 09 / 14
                        21:
                        54:23 tross 62 61
c rop_zoom_out_bw_rgb16 / rgbm
e
s 00577 / 00323 / 04544
d D 1.61 96 / 09 / 12
                        19:
                        44:13 tross 61 60
c rop_zoom_out_bw_rgbm / rgb16, corretto rop_zoom_out_90_gr8_...
e
s 00026 / 00026 / 04841
d D 1.60 96 / 08 / 05
                        18:
                        15:12 vincenzo 60 59
c gl_color->LPIXEL
e
s 00002 / 00001 / 04865
d D 1.59 96 / 07 / 12
                        18:
                        41:01 vincenzo 59 58
c messo RGBM64 rop_pixbytes
e
s 00246 / 00123 / 04620
d D 1.58 96 / 07 / 10
                        17:
                        57:56 vincenzo 58 57
c introdotto rop_copy per RAS_RGBM64
e
s 00280 / 00086 / 04463
d D 1.57 96 / 07 / 10
                        01:
                        01:58 tross 57 56
c rop_zoom_out_90, per ora solo gr8-- > rgb16 / rgbm
e
s 00279 / 00110 / 04270
d D 1.56 96 / 07 / 09
                        16:
                        44:14 tross 56 55
c cmap.offset-- > cmap.info.offset_mask etc.
e
s 00238 / 00065 / 04142
d D 1.55 96 / 07 / 01
                        16:
                        24:22 tross 55 54
c rop_copy_90_bw / gr8_rgb16 
e
s 00205 / 00009 / 04002
d D 1.54 96 / 06 / 27
                        20:
                        46:24 tross 54 53
c rop_copy_bw / gr8_rgb16, rop_zoom_out_gr8_rgb16, aggiornati nomi macro RGB16
e
s 00094 / 00047 / 03917
d D 1.53 96 / 06 / 27
                        04:
                        39:24 tross 53 52
c conversioni pix_type<->ras_type
e
s 00675 / 00343 / 03289
d D 1.52 96 / 06 / 26 02:04:18 tross 52 51
c potrebbero funziare un po' di copy e zoom_out ->rgb16
e
s 00002 / 00001 / 03630
d D 1.51 96 / 05 / 15 22:11:20 tross 51 50
c tmsg.h
e
s 00011 / 00000 / 03620
d D 1.50 96 / 05 / 11 13:34:15 tross 50 49
c variabili non inizializzate
e
s 00001 / 00001 / 03619
d D 1.49 96 / 04 / 10 02:05:56 tross 49 48
c rop_zoom_out_rgb_rgbm(con baco corretto)
e
s 00112 / 00001 / 03508
d D 1.48 96 / 04 / 10 02:01:59 tross 48 47
c 
e
s 00104 / 00010 / 03405
d D 1.47 96 / 03 / 19 18:04:11 tross 47 46
c rop_zoom_out_gr8_rgbm
e
s 00139 / 00010 / 03276
d D 1.46 96 / 03 / 18 20:22:38 tross 46 45
c rop_zoom_out_rgbx
e
s 00003 / 00002 / 03283
d D 1.45 96 / 02 / 08 23:16:51 tross 45 44
c mancava la rop_copy_90 RGB--> RGB_
e
s 00000 / 00002 / 03285
d D 1.44 96 / 02 / 07
                        18:
                        24:05 tross 44 43
c tolta una printf
e
s 00091 / 00000 / 03196
d D 1.43 95 / 11 / 21
                        16:
                        33:23 roberto 43 42
c Introdotta rgb to rgbm
e
s 00004 / 00001 / 03192
d D 1.42 95 / 10 / 09
                        14:
                        57:40 vinz 42 41
c aggiunti i rop_copy RAS_CM16-- > quantized type
e
s 00061 / 00042 / 03132
d D 1.41 95 / 10 / 04
                        18:
                        16:37 tross 41 40
c corretto baco di
                  rop_copy:fatto rop_copy_same()
e
s 00061 / 00001 / 03113
d D 1.40 95 / 10 / 04
                        16:
                        05:47 tross 40 39
c rop_[sub] image_to_raster()
e
s 00095 / 00017 / 03019
d D 1.39 95 / 10 / 04
                        12:
                        27:49 vinz 39 38
c aggiunto il rop_shrink
e
s 00007 / 00007 / 03029
d D 1.38 95 / 09 / 25
                        02:
                        26:09 tross 38 37
c corretto baco(poco visibile)nei rop_zoom_out... _rgbm()
e
s 00001 / 00004 / 03035
d D 1.37 95 / 09 / 20
                        15:
                        57:00 tross 37 36
c migliorate rop_custom_fill_cmap_ramp / buffer
e
s 00060 / 00000 / 02979
d D 1.36 95 / 09 / 19
                        17:
                        22:55 tross 36 35
c rop_custom_fill_cmap_ramp / buffer
e
s 00002 / 00006 / 02977
d D 1.35 95 / 07 / 28
                        21:
                        35:43 tross 35 34
c estetica
e
s 00004 / 00000 / 02979
d D 1.34 95 / 07 / 28
                        17:
                        17:40 grisu 34 33
c corretto un baco sull' offset della cmap
e
s 00001 / 00001 / 02978
d D 1.33 95 / 07 / 28
                        15:
                        22:33 tross 33 32
c corretto baco di un controllo in rop_fill_cmap_buffer
e
s 00119 / 00005 / 02860
d D 1.32 95 / 07 / 27
                        21:
                        36:31 tross 32 31
c rop_fill_cmap_ramp / buffer
e
s 00275 / 00022 / 02590
d D 1.31 95 / 07 / 27
                        00:
                        05:00 tross 31 30
c rop_zoom_out_cm16_rgbm, rop_zoom_out_rgbm
e
s 00003 / 00001 / 02609
d D 1.30 95 / 07 / 09
                        22:
                        23:39 tross 30 29
c messaggio in rop_zoom_out access violation
e
s 00043 / 00034 / 02567
d D 1.29 95 / 07 / 04
                        01:
                        50:38 tross 29 28
c rop_zoom_out a scacchiera, cmap_offset presottratto a varie funz
e
s 00531 / 00005 / 02070
d D 1.28 95 / 07 / 04
                        00:
                        46:11 tross 28 27
c con rop_zoom_out funzionante per cm16_rgb_
e
s 00003 / 00001 / 02072
d D 1.27 95 / 07 / 03
                        15:
                        38:32 tross 27 26
c messaggio completo in caso di access violation
e
s 00086 / 00000 / 01987
d D 1.26 94 / 12 / 28
                        21:
                        07:43 tross 26 25
c aggiunta rop_copy_90_rgbm
e
s 00043 / 00000 / 01944
d D 1.25 94 / 11 / 30
                        22:
                        03:25 tross 25 24
c aggiunto rop_add / remove_white_to / from_cmap
e
s 00001 / 00001 / 01943
d D 1.24 94 / 10 / 26
                        00:
                        28:46 tross 24 23
c corretto baco rop_copy_rgb_rgbm
e
s 00147 / 00014 / 01797
d D 1.23 94 / 09 / 16
                        19:
                        45:57 tross 23 22
c cm8
e
s 00231 / 00000 / 01580
d D 1.22 94 / 09 / 16
                        00:
                        02:27 tross 22 21
c rop_copy_90_bw / gr8_rgbm
e
s 00243 / 00027 / 01337
d D 1.21 94 / 09 / 11
                        20:
                        52:53 tross 21 20
c con rop_copy_90-- > cm16
e
s 00243 / 00000 / 01121
d D 1.20 94 / 09 / 05
                        23:
                        18:47 tross 20 19
c iniziato rop_reduce
e
s 00059 / 00000 / 01062
d D 1.19 94 / 06 / 07
                        07:
                        01:19 tross 19 18
c rop_copy WB->RGBM
e
s 00096 / 00004 / 00966
d D 1.18 94 / 06 / 01
                        00:
                        42:00 tross 18 17
c con cura dimagrante per entrare nella cassetta
e
s 00124 / 00001 / 00846
d D 1.17 94 / 04 / 29
                        23:
                        15:12 tross 17 16
c rop_copy_90_bw_gr8
e
s 00009 / 00009 / 00838
d D 1.16 94 / 04 / 14
                        23:
                        21:49 tross 16 15
c wrap((+7) / 8)
e
s 00155 / 00001 / 00692
d D 1.15 94 / 04 / 06
                        16:
                        19:14 tross 15 14
c rop_copy_90_bw piu' veloce
e
s 00273 / 00068 / 00420
d D 1.14 94 / 04 / 06
                        03:
                        25:39 tross 14 13
c rop_copy_90 pare funzionare per bw e gr8
e
s 00003 / 00003 / 00485
d D 1.13 94 / 03 / 31
                        19:
                        38:31 tross 13 12
c 
e
s 00012 / 00008 / 00476
d D 1.12 94 / 03 / 30
                        21:
                        06:15 tross 12 11
c 
e
s 00001 / 00001 / 00483
d D 1.11 94 / 03 / 30
                        20:
                        46:54 tross 11 10
c 
e
s 00030 / 00048 / 00454
d D 1.10 94 / 03 / 30
                        20:
                        45:37 tross 10 9
c rimesso a posto il wrap(in pixel)
e
s 00007 / 00000 / 00495
d D 1.9 94 / 03 / 30
                        17:
                        30:50 tross 9 8
c rop_pixbytes
e
s 00021 / 00006 / 00474
d D 1.8 94 / 03 / 13
                        17:
                        41:53 tross 8 7
c corretto baco su rop_copy tra dimensioni diverse
e
s 00005 / 00001 / 00475
d D 1.7 94 / 03 / 11
                        14:
                        04:27 tross 7 6
c bachetto rop_copy, per 3.01j
e
s 00187 / 00001 / 00289
d D 1.6 94 / 03 / 11
                        13:
                        48:45 tross 6 5
c per 3.01j
e
s 00024 / 00027 / 00266
d D 1.5 94 / 03 / 02
                        15:
                        14:56 tross 5 4
c cmap per copy bw->cm16 sempre lunga 256
e
s 00125 / 00008 / 00168
d D 1.4 94 / 02 / 28
                        15:
                        53:37 tross 4 3
c rop_copy per RAS_BW e RAS_WB
e
s 00002 / 00000 / 00174
d D 1.3 94 / 02 / 19
                        22:
                        17:18 tross 3 2
c per fullwarn su os_4
e
s 00024 / 00019 / 00150
d D 1.2 94 / 02 / 10
                        14:
                        04:52 tross 2 1
c cambiati i RAS_
e
s 00169 / 00000 / 00000
d D 1.1 94 / 02 / 04
                        22:
                        02:58 tross 1 0
c date and time created 94 / 02 / 04
                        22:
                        02:58 by tross
e
u
U
f e 0
t
T
I 14
D 32

#ifdef DEBUG
#include "stopwatch.h"
#endif

E 32
E 14
I 1
#include <stdio.h>
#include <string.h>
I 40
#include <stdlib.h>
E 40
I 32
#include <assert.h>
E 32
#include "toonz.h"
I 51
#include "tmsg.h"
E 51
D 39
#include "raster.h"
E 39
I 39
D 40
#include "rasterwork.h"
E 40
I 40
#include "raster.h"
E 40
E 39
#include "raster_p.h"

I 99
#define MEMORY_PRINTF
E 99
I 32
#define STW_DISABLE
#include "stopwatch.h"

I 61
#define BUFBYTE(X, Y, BUF, BYTEWRAP, BITOFFS)                                  \
  (((UCHAR *)(BUF))[(((X) + (BITOFFS)) >> 3) + (Y) * (BYTEWRAP)])

#define GET_BIT(X, Y, BUF, BYTEWRAP, BITOFFS)                                  \
  ((BUFBYTE(X, Y, BUF, BYTEWRAP, BITOFFS) >> (7 - (((X) + (BITOFFS)) & 7))) &  \
   (UCHAR)1)

#define SET_BIT_0(X, Y, BUF, BYTEWRAP, BITOFFS)                                \
  (BUFBYTE(X, Y, BUF, BYTEWRAP, BITOFFS) &=                                    \
   ~(1 << (7 - (((X) + (BITOFFS)) & 7))))

#define SET_BIT_1(X, Y, BUF, BYTEWRAP, BITOFFS)                                \
  (BUFBYTE(X, Y, BUF, BYTEWRAP, BITOFFS) |=                                    \
   (1 << (7 - (((X) + (BITOFFS)) & 7))))

#define BITCPY(XO, YO, BUFO, BYTEWRAPO, BITOFFSO, XI, YI, BUFI, BYTEWRAPI,     \
               BITOFFSI)                                                       \
  {                                                                            \
    if (GET_BIT(XI, YI, BUFI, BYTEWRAPI, BITOFFSI))                            \
      SET_BIT_1(XO, YO, BUFO, BYTEWRAPO, BITOFFSO);                            \
    else                                                                       \
      SET_BIT_0(XO, YO, BUFO, BYTEWRAPO, BITOFFSO);                            \
  }

I 62
#define GET_BWBIT(BIT, BUF) (((BUF)[(BIT) >> 3] >> (7 - ((BIT)&7))) & 1)

I 84
#define MAGICFAC (257U * 256U + 1U)

#define MAP24 PIX_CM24_PENMAP_COLMAP_TO_RGBM
#define MAP24_64 PIX_CM24_PENMAP_COLMAP_TO_RGBM64

I 95
#define PIX_RGB16_FROM_USHORT(R, G, B)                                         \
  (((((R)*BYTE_FROM_USHORT_MAGICFAC) + (1 << 23)) >> 16) & 0xf800 |            \
   ((((G)*BYTE_FROM_USHORT_MAGICFAC) + (1 << 23)) >> 21) & 0x07e0 |            \
   ((((B)*BYTE_FROM_USHORT_MAGICFAC) + (1 << 23)) >> 27))

D 96
#define PIX_RGB16_FROM_RGBX64(L) (PIX_RGB16_FROM_USHORT((L).r, (L).g, (L).b))
E 96
I 96
#define PIX_XRGB1555_FROM_USHORT(R, G, B)                                      \
  (((((R)*BYTE_FROM_USHORT_MAGICFAC) + (1 << 23)) >> 17) & 0x7c00 |            \
   ((((G)*BYTE_FROM_USHORT_MAGICFAC) + (1 << 23)) >> 22) & 0x03e0 |            \
   ((((B)*BYTE_FROM_USHORT_MAGICFAC) + (1 << 23)) >> 27))
E 96

I 96
#define PIX_RGB16_FROM_RGBX64(L) (PIX_RGB16_FROM_USHORT((L).r, (L).g, (L).b))
#define PIX_XRGB1555_FROM_RGBX64(L)                                            \
  (PIX_XRGB1555_FROM_USHORT((L).r, (L).g, (L).b))

E 96
E 95
E 84
I 66
D 74

E 74
E 66
E 62
    /*---------------------------------------------------------------------------*/

E 61
D 52

E 52
E 32
I 6
D 14
    /* TOGLIERE QUESTA ROBA (chiedere a Walter) */
E 14 static void rop_copy_90_bw(RASTER *rin, RASTER *rout, int x1, int y1,
                                  int x2, int y2, int newx, int newy,
D 14 int ninety, int flip);
E 14
I 14
		           int mirror, int ninety);
E 14 static void rop_copy_90_gr8(RASTER *rin, RASTER *rout, int x1, int y1,
                                  int x2, int y2, int newx, int newy,
D 14 int ninety, int flip);
E 14
I 14
		            int mirror, int ninety);
I 21 static void rop_copy_90_gr8_cm16(RASTER *rin, RASTER *rout, int x1,
                                       int y1, int x2, int y2, int newx,
                                       int newy, int mirror, int ninety);
I 84 static void rop_copy_90_gr8_cm24(RASTER *rin, RASTER *rout, int x1,
                                       int y1, int x2, int y2, int newx,
                                       int newy, int mirror, int ninety);
E 84
I 22 static void rop_copy_90_gr8_rgbm(RASTER *rin, RASTER *rout, int x1,
                                        int y1, int x2, int y2, int newx,
                                        int newy, int mirror, int ninety);
I 55 static void rop_copy_90_gr8_rgb16(RASTER *rin, RASTER *rout, int x1,
                                        int y1, int x2, int y2, int newx,
                                        int newy, int mirror, int ninety);
E 55
I 23 static void rop_copy_90_cm8_cm16(RASTER *rin, RASTER *rout, int x1,
                                        int y1, int x2, int y2, int newx,
                                        int newy, int mirror, int ninety);
I 84 static void rop_copy_90_cm8_cm24(RASTER *rin, RASTER *rout, int x1,
                                       int y1, int x2, int y2, int newx,
                                       int newy, int mirror, int ninety);
E 84
E 23
E 22
E 21
I 17 static void rop_copy_90_bw_gr8(RASTER *rin, RASTER *rout, int x1, int y1,
                                      int x2, int y2, int newx, int newy,
                                      int mirror, int ninety);
I 21 static void rop_copy_90_bw_cm16(RASTER *rin, RASTER *rout, int x1, int y1,
                                      int x2, int y2, int newx, int newy,
                                      int mirror, int ninety);
I 84 static void rop_copy_90_bw_cm24(RASTER *rin, RASTER *rout, int x1, int y1,
                                      int x2, int y2, int newx, int newy,
                                      int mirror, int ninety);
E 84
I 55 static void rop_copy_90_bw_rgb16(RASTER *rin, RASTER *rout, int x1,
                                        int y1, int x2, int y2, int newx,
                                        int newy, int mirror, int ninety);
E 55
I 22 static void rop_copy_90_bw_rgbm(RASTER *rin, RASTER *rout, int x1,
                                       int y1, int x2, int y2, int newx,
                                       int newy, int mirror, int ninety);
I 26 static void rop_copy_90_rgbm(RASTER *rin, RASTER *rout, int x1, int y1,
                                   int x2, int y2, int newx, int newy,
                                   int mirror, int ninety);
I 63 static void rop_copy_90_rgb_rgb16(RASTER *rin, RASTER *rout, int x1,
                                        int y1, int x2, int y2, int newx,
                                        int newy, int mirror, int ninety);
E 63
I 43 static void rop_copy_90_rgb_rgbm(RASTER *rin, RASTER *rout, int x1,
                                        int y1, int x2, int y2, int newx,
                                        int newy, int mirror, int ninety);
E 43
I 40

I 104 static void rop_copy_bw(RASTER *rin, RASTER *rout, int x1, int y1,
                                int x2, int y2, int newx, int newy);

E 104
    /*---------------------------------------------------------------------------*/

I 88
D 89 static LPIXEL
rop_premult(LPIXEL color) {
  UINT m, mm;
  LPIXEL new_color;

  m = color.m;
  if (m == 255)
    new_color = color;
  else if (m == 0) {
    new_color.r = 0;
    new_color.g = 0;
    new_color.b = 0;
    new_color.m = 0;
  } else {
    mm          = m * MAGICFAC;
    new_color.r = (color.r * mm + (1U << 23)) >> 24;
    new_color.g = (color.g * mm + (1U << 23)) >> 24;
    new_color.b = (color.b * mm + (1U << 23)) >> 24;
    new_color.m = m;
  }
  return new_color;
}

/*---------------------------------------------------------------------------*/

E 89
E 88
D 79 void rop_image_to_raster(IMAGE *img, RASTER *ras, int build_cmap)
E 79
I 79 void rop_image_to_raster(IMAGE *img, RASTER *ras, int get_cmap)
E 79 {
  rop_subimage_to_raster(img, 0, 0, img->pixmap.xsize - 1,
                         img->pixmap.ysize - 1,
D 79 ras, build_cmap);
E 79
I 79
			ras, get_cmap);
E 79
}

/*---------------------------------------------------------------------------*/

void rop_subimage_to_raster (IMAGE *img,
                             int img_x0, int img_y0, int img_x1, int img_y1,
D 79
                             RASTER *ras, int build_cmap)
E 79
I 79
                             RASTER *ras, int get_cmap)
E 79
{
  int i;

  NOT_LESS_THAN(0, img_x0)
  NOT_LESS_THAN(0, img_y0)
  NOT_MORE_THAN(img->pixmap.xsize - 1, img_x1)
  NOT_MORE_THAN(img->pixmap.ysize - 1, img_y1)
  assert(img_x0 <= img_x1);
  assert(img_y0 <= img_y1);
  memset(ras, 0, sizeof(*ras));
  switch (img->type) {
    CASE CMAPPED : ras->type = RAS_CM16;
    D 76 ras->buffer = (USHORT *)img->pixmap.buffer + img_x0
E 76
I 76 ras->native_buffer = ras->buffer = (USHORT *)img->pixmap.buffer + img_x0
E 76 + img_y0 * img->pixmap.xsize;
    I 84 CASE CMAPPED24 : ras->type = RAS_CM24;
    ras->native_buffer               = ras->buffer =
        (ULONG *)img->pixmap.buffer + img_x0 + img_y0 * img->pixmap.xsize;
    E 84 CASE RGB : ras->type = RAS_RGBM;
    D 76 ras->buffer = (ULONG *)img->pixmap.buffer + img_x0
E 76
I 76
D 94 ras->native_buffer = ras->buffer = (ULONG *)img->pixmap.buffer + img_x0
E 94
I 94 ras->native_buffer = ras->buffer = (LPIXEL *)img->pixmap.buffer + img_x0
E 94
E 76 + img_y0 * img->pixmap.xsize;
    I 94 CASE RGB64 : ras->type        = RAS_RGBM64;
    ras->native_buffer                  = ras->buffer =
        (SPIXEL *)img->pixmap.buffer + img_x0 + img_y0 * img->pixmap.xsize;
    E 94
D 78 DEFAULT : assert(FALSE);
    E 78
I 78 DEFAULT : abort();
    E 78
  }
  I 89 ras->extra_mask = img->pixmap.extra_mask;
  if (img->type != CMAPPED24 && img->pixmap.extra_mask)
    ras->native_extra = ras->extra =
        img->pixmap.extra + img_x0 + img_y0 * img->pixmap.xsize;
  else
    ras->native_extra = ras->extra = NIL;

  E 89 ras->wrap       = img->pixmap.xsize;
  ras->lx               = img_x1 - img_x0 + 1;
  ras->ly               = img_y1 - img_y0 + 1;
  D 56 ras->glob_matte = 0xff;
  if (ras->type == RAS_CM16)
    E 56
I 56
D 84 if (ras->type == RAS_CM8 || ras->type == RAS_CM16)
E 84
I 84 if (ras->type == RAS_CM8 || ras->type == RAS_CM16 ||
           ras->type == RAS_CM24)
E 84
E 56 {
      D 56 ras->cmap.offset = img->cmap.offset;
      ras->cmap.size         = img->cmap.size;
      E 56
I 56
D 82 ras->cmap.size        = img->cmap.size;
      E 82 ras->cmap.info   = img->cmap.info;
      E 56
D 79 if (build_cmap) {
        TMALLOC(ras->cmap.buffer, ras->cmap.size)
        for (i                        = 0; i < ras->cmap.size; i++) {
          D 60 ras->cmap.buffer[i].r = img->cmap.buffer[i].red;
          ras->cmap.buffer[i].g       = img->cmap.buffer[i].green;
          ras->cmap.buffer[i].b       = img->cmap.buffer[i].blue;
          ras->cmap.buffer[i].m       = img->cmap.buffer[i].matte;
          E 60
I 60 ras->cmap.buffer[i].r          = img->cmap.buffer[i].r;
          ras->cmap.buffer[i].g       = img->cmap.buffer[i].g;
          ras->cmap.buffer[i].b       = img->cmap.buffer[i].b;
          ras->cmap.buffer[i].m       = img->cmap.buffer[i].m;
          E 60
        }
      }
      E 79
I 79 if (get_cmap)
D 84 ras->cmap.buffer = img->cmap.buffer;
      E 84
I 84 {
        ras->cmap.buffer    = img->cmap.buffer;
        ras->cmap.penbuffer = img->cmap.penbuffer;
        ras->cmap.colbuffer = img->cmap.colbuffer;
      }
      E 84
E 79 else D 84 ras->cmap.buffer = NIL;
      E 84
I 84 {
        ras->cmap.buffer    = NIL;
        ras->cmap.penbuffer = NIL;
        ras->cmap.colbuffer = NIL;
      }
      E 84
    }
}
E 40
E 26
E 22
E 21
E 17
E 14

E 6
    /*---------------------------------------------------------------------------*/

I 88
    /* ATTENZIONE !!!!! Esiste una copia esatta di questa func
in sources/util/image/timage/timg.c, per evitare che la image dipenda dalla
raster; ogni modifica di questa va anche riportata li'. Vincenzo */
 
E 88 int
rop_pixbits(RAS_TYPE rastertype) {
  D 2
      /* attenzione: appena entra il primo tipo non multiplo di 8 bits
* tutto il codice di questo pacchetto va ricontrollato da cima a fondo
*/

E 2 switch (rastertype) {
    D 2 CASE RAS_BW : return 8;
    CASE RAS_CM : return 16;
    E 2
I 2
D 39 CASE RAS_BW : return 1;
    CASE RAS_WB : return 1;
    CASE RAS_GR8 : return 8;
    I 23 CASE RAS_CM8 : return 8;
    E 23 CASE RAS_CM16 : return 16;
    CASE RAS_RGB : return 24;
    CASE RAS_RGB_ : return 32;
    E 2 CASE RAS_RGBM : return 32;
    E 39
I 39 CASE RAS_BW : return 1;
    CASE RAS_MBW16 : return 16;
    CASE RAS_WB : return 1;
    CASE RAS_GR8 : return 8;
    I 90 CASE RAS_GR16 : return 16;
    E 90 CASE RAS_CM8 : return 8;
    D 83 CASE RAS_UNICM8 : return 8;
    E 83
I 72 CASE RAS_CM8S8 : return 8;
    CASE RAS_CM8S4 : return 8;
    E 72 CASE RAS_CM16 : return 16;
    I 98 CASE RAS_CM16S12 : return 16;
    E 98
D 83 CASE RAS_UNICM16 : return 16;
    I 53 CASE RAS_RGB16 : return 16;
    E 83
I 72 CASE RAS_CM16S8 : return 16;
    CASE RAS_CM16S4 : return 16;
    I 84 CASE RAS_CM24 : return 32;
    E 84
I 83 CASE RAS_RGB16 : return 16;
    I 96
D 97 CASE RAS_XRGB1555 : return 16;
    E 97
I 97 CASE RAS_RLEBW : return 1;
    E 97
E 96
E 83
E 72
E 53 CASE RAS_RGB : return 24;
    CASE RAS_RGB_ : return 32;
    CASE RAS_RGBM : return 32;
    I 58 CASE RAS_RGBM64 : return 64;
    CASE RAS_RGB_64 : return 64;
    E 58
E 39 DEFAULT : msg(MSG_IE, "rop_pixbits: invalid rastertype");
  }
  I 3 return 0;
  E 3
}

I 39

E 39
    /*---------------------------------------------------------------------------*/

I 9 int
rop_pixbytes(RAS_TYPE rastertype) {
  return (rop_pixbits(rastertype) + 7) >> 3;
}

/*---------------------------------------------------------------------------*/

E 9 int rop_fillerbits(RAS_TYPE rastertype) {
  switch (rastertype) {
    D 2 CASE RAS_BW : return FALSE;
    CASE RAS_CM : return FALSE;
    E 2
I 2
D 39 CASE RAS_BW : return TRUE;
    CASE RAS_WB : return TRUE;
    CASE RAS_GR8 : return FALSE;
    I 23 CASE RAS_CM8 : return FALSE;
    E 23 CASE RAS_CM16 : return FALSE;
    CASE RAS_RGB : return FALSE;
    CASE RAS_RGB_ : return FALSE;
    E 2 CASE RAS_RGBM : return FALSE;
    E 39
I 39
D 84 CASE RAS_BW : return TRUE;
    CASE RAS_WB : return TRUE;
    CASE RAS_MBW16 : return FALSE;
    CASE RAS_GR8 : return FALSE;
    CASE RAS_CM8 : return FALSE;
    D 83 CASE RAS_UNICM8 : return FALSE;
    E 83 CASE RAS_CM16 : return FALSE;
    D 83 CASE RAS_UNICM16 : return FALSE;
    E 83
I 53 CASE RAS_RGB16 : return FALSE;
    E 53 CASE RAS_RGB : return FALSE;
    CASE RAS_RGB_ : return FALSE;
    CASE RAS_RGBM : return FALSE;
    I 59 CASE RAS_RGBM64 : return FALSE;
    E 59
E 39 DEFAULT : msg(MSG_IE, "rop_fillerbits: invalid rastertype");
    E 84
I 84 CASE RAS_BW : return TRUE;
    CASE RAS_WB : return TRUE;
  DEFAULT:
    return FALSE;
    E 84
  }
  I 3 return 0;
  E 3
}

/*---------------------------------------------------------------------------*/

I 53 RAS_TYPE rop_standard_ras_type_of_pix_type(PIX_TYPE pix_type) {
  switch (pix_type) {
    D 96 CASE PIX_NONE : return RAS_NONE;
    CASE PIX_BW : return RAS_BW;
    CASE PIX_WB : return RAS_WB;
    CASE PIX_GR8 : return RAS_GR8;
    CASE PIX_CM8 : return RAS_CM8;
    CASE PIX_CM16 : return RAS_CM16;
    I 84 CASE PIX_CM24 : return RAS_CM24;
    E 84
I 72 CASE PIX_CM8S8 : return RAS_CM8S8;
    CASE PIX_CM8S4 : return RAS_CM8S4;
    CASE PIX_CM16S8 : return RAS_CM16S8;
    CASE PIX_CM16S4 : return RAS_CM16S4;
    E 72 CASE PIX_RGB16 : return RAS_RGB16;
    CASE PIX_RGB : return RAS_RGB;
    CASE PIX_RGB_ : return RAS_RGB_;
    CASE PIX_RGBM : return RAS_RGBM;
    E 96
I 96 CASE PIX_NONE : return RAS_NONE;
    CASE PIX_BW : return RAS_BW;
    CASE PIX_WB : return RAS_WB;
    CASE PIX_GR8 : return RAS_GR8;
    CASE PIX_CM8 : return RAS_CM8;
    CASE PIX_CM16 : return RAS_CM16;
    CASE PIX_CM24 : return RAS_CM24;
    CASE PIX_CM8S8 : return RAS_CM8S8;
    CASE PIX_CM8S4 : return RAS_CM8S4;
    I 98 CASE PIX_CM16S12 : return RAS_CM16S12;
    E 98 CASE PIX_CM16S8 : return RAS_CM16S8;
    CASE PIX_CM16S4 : return RAS_CM16S4;
    CASE PIX_RGB16 : return RAS_RGB16;
    D 97 CASE PIX_XRGB1555 : return RAS_XRGB1555;
    E 97 CASE PIX_RGB : return RAS_RGB;
    CASE PIX_RGB_ : return RAS_RGB_;
    CASE PIX_RGBM : return RAS_RGBM;
    E 96 DEFAULT :
D 63 assert(FALSE);
    return RAS_NONE;
    E 63
I 63
D 78 assert(FALSE);
    E 78
I 78 abort();
    E 78
E 63
  }
  I 63 return RAS_NONE;
  E 63
}

/*---------------------------------------------------------------------------*/

PIX_TYPE rop_pix_type_of_ras_type(RAS_TYPE ras_type) {
  switch (ras_type) {
    D 96 CASE RAS_NONE : return PIX_NONE;
    CASE RAS_BW : return PIX_BW;
    CASE RAS_WB : return PIX_WB;
    CASE RAS_GR8 : return PIX_GR8;
    I 90 CASE RAS_GR16 : return PIX_RGB16;
    E 90 CASE RAS_CM8 : return PIX_CM8;
    CASE RAS_CM16 : return PIX_CM16;
    CASE RAS_RGB16 : return PIX_RGB16;
    CASE RAS_RGB : return PIX_RGB;
    CASE RAS_RGB_ : return PIX_RGB_;
    CASE RAS_RGBM : return PIX_RGBM;
    CASE RAS_MBW16 : return PIX_BW;
    D 83 CASE RAS_UNICM8 : return PIX_CM8;
    CASE RAS_UNICM16 : return PIX_CM16;
    E 83
I 72 CASE RAS_CM8S8 : return PIX_CM8S8;
    CASE RAS_CM8S4 : return PIX_CM8S4;
    CASE RAS_CM16S8 : return PIX_CM16S8;
    CASE RAS_CM16S4 : return PIX_CM16S4;
    I 84 CASE RAS_CM24 : return PIX_CM24;
    E 96
I 96 CASE RAS_NONE : return PIX_NONE;
    CASE RAS_BW : return PIX_BW;
    CASE RAS_WB : return PIX_WB;
    CASE RAS_GR8 : return PIX_GR8;
    CASE RAS_GR16 : return PIX_RGB16;
    CASE RAS_CM8 : return PIX_CM8;
    CASE RAS_CM16 : return PIX_CM16;
    CASE RAS_RGB16 : return PIX_RGB16;
    D 97 CASE RAS_XRGB1555 : return PIX_XRGB1555;
    E 97 CASE RAS_RGB : return PIX_RGB;
    CASE RAS_RGB_ : return PIX_RGB_;
    CASE RAS_RGBM : return PIX_RGBM;
    CASE RAS_MBW16 : return PIX_BW;
    CASE RAS_CM8S8 : return PIX_CM8S8;
    CASE RAS_CM8S4 : return PIX_CM8S4;
    I 98 CASE RAS_CM16S12 : return PIX_CM16S12;
    E 98 CASE RAS_CM16S8 : return PIX_CM16S8;
    CASE RAS_CM16S4 : return PIX_CM16S4;
    CASE RAS_CM24 : return PIX_CM24;
    E 96
E 84
E 72 DEFAULT :
D 63 assert(FALSE);
    return PIX_NONE;
    E 63
I 63
D 78 assert(FALSE);
    E 78
I 78 abort();
    E 78
E 63
  }
  I 63 return PIX_NONE;
  E 63
}

/*---------------------------------------------------------------------------*/

I 74 void rop_premultiply(RASTER *rin, RASTER *rout) {
  int x, y, wrapin, wrapout, lx, ly;
  LPIXEL *in, *out, val, transp;
  SPIXEL *in64, *out64, val64, transp64;
  UINT m;

  D 84
#define MAGICFAC (257U * 256U + 1U)
E 84
#define PREMULT(V, M)                                                          \
  {                                                                            \
    UINT mm = (M)*MAGICFAC;                                                    \
    (V).r   = ((V).r * mm + (1U << 23)) >> 24;                                 \
    (V).g   = ((V).g * mm + (1U << 23)) >> 24;                                 \
    (V).b   = ((V).b * mm + (1U << 23)) >> 24;                                 \
  }

#define PREMULT64(V, M)                                                        \
  {                                                                            \
    double mf = (double)(M) * (1.0 / 65535.0);                                 \
    (V).r     = (USHORT)((double)((V).r) * mf + 0.5);                          \
    (V).g = (USHORT)((double)((V).g) * mf + 0.5);                              \
    (V).b = (USHORT)((double)((V).b) * mf + 0.5);                              \
  }

      assert(rin->lx == rout->lx && rin->ly == rout->ly);
  wrapin  = rin->wrap;
  wrapout = rout->wrap;
  lx      = rin->lx;
  ly      = rin->ly;
  switch (RASRAS(rin->type, rout->type)) {
    CASE RASRAS(RAS_RGBM64, RAS_RGBM64)
        : transp64.r = transp64.g = transp64.b = transp64.m = 0;
    D 75 for (y = ly; y > 0; y--)
E 75
I 75 for (y = 0; y < ly; y++)
E 75 {
      in64 = ((SPIXEL *)rin->buffer) + y * wrapin;
      out64        = ((SPIXEL *)rout->buffer) + y * wrapout;
      D 75 for (x = lx; x > 0; x--)
E 75
I 75 for (x            = 0; x < lx; x++)
E 75 {
        val64            = *in64++;
        m                = val64.m;
        if (!m) *out64++ = transp64;
        D 104 else if (m == 255U)
E 104
I 104 else if (m == 0xffff)
E 104 *out64++         = val64;
        else {
          PREMULT64(val64, m)
          *out64++ = val64;
        }
      }
    }
    CASE RASRAS(RAS_RGBM, RAS_RGBM)
        : transp.r = transp.g = transp.b = transp.m = 0;
    D 75 for (y = ly; y > 0; y--)
E 75
I 75 for (y = 0; y < ly; y++)
E 75 {
      in = ((LPIXEL *)rin->buffer) + y * wrapin;
      out          = ((LPIXEL *)rout->buffer) + y * wrapout;
      D 75 for (x = lx; x > 0; x--)
E 75
I 75 for (x = 0; x < lx; x++)
E 75 {
        val   = *in++;
        m     = val.m;
        if (!m)
          *out++ = transp;
        else if (m == 255U)
          *out++ = val;
        else {
          PREMULT(val, m)
          *out++ = val;
        }
      }
    }
  DEFAULT:
    D 78 assert(FALSE);
    E 78
I 78 abort();
    E 78
  }
  I 89 if (rout->extra_mask)
      rop_copy_extra(rin, rout, 0, 0, lx - 1, ly - 1, 0, 0);
  E 89
}

/*---------------------------------------------------------------------------*/

E 74
E 53
I 41 static void rop_copy_same(RASTER *rin, RASTER *rout, int x1, int y1,
                                 int x2, int y2, int newx, int newy) {
  UCHAR *rowin, *rowout;
  int rowsize, bytewrapin, bytewrapout, d;
  int pixbits_in, pixbits_out;
  int pixbytes_in, pixbytes_out;

  I 104 if (((rin->type == RAS_BW) && (rout->type == RAS_BW)) ||
             ((rin->type == RAS_WB) && (rout->type == RAS_WB))) {
    rop_copy_bw(rin, rout, x1, y1, x2, y2, newx, newy);
    return;
  }

  E 104 pixbits_in = rop_pixbits(rin->type);
  pixbytes_in       = rop_pixbytes(rin->type);
  pixbits_out       = rop_pixbits(rout->type);
  pixbytes_out      = rop_pixbytes(rout->type);

  /* pixsize compatibili */
  assert(pixbits_in == pixbits_out);

  /* per adesso niente pixel non multipli di 8 bits */
  assert(!rop_fillerbits(rin->type));
  assert(!rop_fillerbits(rout->type));

  /* copia */
  rowsize     = pixbytes_in * (x2 - x1 + 1);
  bytewrapin  = rin->wrap * pixbytes_in;
  bytewrapout = rout->wrap * pixbytes_out;
  rowin = (UCHAR *)rin->buffer + y1 * bytewrapin + ((x1 * pixbits_in) >> 3);
  rowout =
      (UCHAR *)rout->buffer + newy * bytewrapout + ((newx * pixbits_out) >> 3);
  d = y2 - y1 + 1;
  while (d-- > 0) {
    memmove(rowout, rowin, rowsize);
    rowin += bytewrapin;
    rowout += bytewrapout;
  }
  I 89 if (rin->type == RAS_CM24 && rout->type == RAS_CM24)
      CLEAR_NEW_EXTRA_BITS_OF_RECT(rout, newx, newy, newx + x2 - x1,
                                   newy + y2 - y1, rin)
}

/*---------------------------------------------------------------------------*/

void rop_copy_extra(RASTER *rin, RASTER *rout, int x1, int y1, int x2, int y2,
                    int newx, int newy) {
  UCHAR *rowin, *rowout, *pixin, *pixout;
  ULONG *rowin24, *rowout24, *pixin24, *pixout24;
  int wrapin, wrapout, x, y, lx, ly, tmp;

  if (!rout->extra_mask) return;

  if (x1 > x2) {
    tmp = x1;
    x1  = x2;
    x2  = tmp;
  }
  if (y1 > y2) {
    tmp = y1;
    y1  = y2;
    y2  = tmp;
  }
  lx = x2 - x1 + 1;
  ly = y2 - y1 + 1;
  if (rin->extra_mask) {
    wrapin  = rin->wrap;
    wrapout = rout->wrap;
    if (rin->type == RAS_CM24) {
      rowin24 = (ULONG *)rin->buffer + x1 + y1 * wrapin;
      if (rout->type == RAS_CM24) {
        rowout24 = (ULONG *)rout->buffer + newx + newy * wrapout;
        for (y = 0; y < ly; y++) {
          pixin24  = rowin24;
          pixout24 = rowout24;
          for (x = 0; x < lx; x++) {
            *pixout24 = *pixout24 & 0x00ffffff | *pixin24 & 0xff000000;
            pixin24++;
            pixout24++;
          }
          rowin24 += wrapin;
          rowout24 += wrapout;
        }
      } else {
        rowout = rout->extra + newx + newy * wrapout;
        for (y = 0; y < ly; y++) {
          pixin24 = rowin24;
          pixout  = rowout;
          for (x = 0; x < lx; x++) {
            *pixout = (UCHAR)(*pixin24 >> 24);
            pixin24++;
            pixout++;
          }
          rowin24 += wrapin;
          rowout += wrapout;
        }
      }
    } else {
      rowin = rin->extra + x1 + y1 * wrapin;
      if (rout->type == RAS_CM24) {
        rowout24 = (ULONG *)rout->buffer + newx + newy * wrapout;
        for (y = 0; y < ly; y++) {
          pixin    = rowin;
          pixout24 = rowout24;
          for (x = 0; x < lx; x++) {
            *pixout24 = *pixout24 & 0x00ffffff | *pixin << 24;
            pixin++;
            pixout24++;
          }
          rowin += wrapin;
          rowout24 += wrapout;
        }
      } else {
        rowout = rout->extra + newx + newy * wrapout;
        for (y = 0; y < ly; y++) {
          memmove(rowout, rowin, lx);
          rowin += wrapin;
          rowout += wrapout;
        }
      }
    }
  }
  CLEAR_NEW_EXTRA_BITS_OF_RECT(rout, newx, newy, newx + lx - 1, newy + ly - 1,
                               rin)
  E 89
}

/*---------------------------------------------------------------------------*/

E 41
I 18 static void rop_copy_gr8_rgbm(RASTER *rin, RASTER *rout, int x1, int y1,
                                     int x2, int y2, int newx, int newy) {
  LPIXEL *rowout, *pixout;
  UCHAR *rowin, *pixin;
  int wrapin, wrapout;
  int x, lx, ly;
  LPIXEL tmp;

  lx      = x2 - x1 + 1;
  ly      = y2 - y1 + 1;
  wrapin  = rin->wrap;
  wrapout = rout->wrap;

  rowin  = (UCHAR *)rin->buffer + wrapin * y1 + x1;
  rowout = (LPIXEL *)rout->buffer + wrapout * newy + newx;

  while (ly-- > 0) {
    pixin  = rowin;
    pixout = rowout;
    for (x = 0; x < lx; x++) {
      tmp.r     = *pixin;
      tmp.g     = *pixin;
      tmp.b     = *pixin;
      tmp.m     = 0xff;
      *pixout++ = tmp;
      pixin++;
    }
    rowin += wrapin;
    rowout += wrapout;
  }
}

/*---------------------------------------------------------------------------*/

I 20
D 84 static void rop_reduce_gr8_rgbm(RASTER *rin, RASTER *rout, int x1,
                                       int y1, int x2, int y2, int newx,
                                       int newy, int factor) {
  printf("rop_reduce_gr8_rgbm not implemented yet\n");
}

/*---------------------------------------------------------------------------*/

E 84
I 28
static void rop_zoom_out_gr8_rgbm(RASTER *rin, RASTER *rout,
D 31
                                int x1, int y1, int x2, int y2,
			        int newx, int newy, int factor)
E 31
I 31
                                  int x1, int y1, int x2, int y2,
			          int newx, int newy, int abs_zoom_level)
E 31
{
  D 47 printf("rop_zoom_out_gr8_rgbm not implemented yet\n");
  E 47
I 47 UCHAR *rowin, *pixin, *in;
  LPIXEL *rowout, *pixout, valout;
  int tmp;
  int wrapin, wrapout;
  int x, y, lx, ly, xlast, ylast, xrest, yrest, i, j;
  int factor, fac_fac_2_bits;
  int fac_fac, yrest_fac, fac_xrest, yrest_xrest;
  int fac_fac_2, yrest_fac_2, fac_xrest_2, yrest_xrest_2;
  int fac_fac_4;

  lx             = x2 - x1 + 1;
  ly             = y2 - y1 + 1;
  factor         = 1 << abs_zoom_level;
  D 61 xrest    = lx % factor;
  yrest          = ly % factor;
  E 61
I 61 xrest     = lx & (factor - 1);
  yrest          = ly & (factor - 1);
  E 61 xlast    = x2 - xrest + 1;
  ylast          = y2 - yrest + 1;
  fac_fac        = factor * factor;
  fac_fac_2      = fac_fac >> 1;
  fac_fac_4      = fac_fac >> 2;
  fac_fac_2_bits = 2 * abs_zoom_level - 1;
  yrest_fac      = yrest * factor;
  yrest_fac_2    = yrest_fac >> 1;
  fac_xrest      = factor * xrest;
  fac_xrest_2    = fac_xrest >> 1;
  yrest_xrest    = yrest * xrest;
  yrest_xrest_2  = yrest_xrest >> 1;
  wrapin         = rin->wrap;
  wrapout        = rout->wrap;
  valout.m       = 0xff;

  rowin  = (UCHAR *)rin->buffer + wrapin * y1 + x1;
  rowout = (LPIXEL *)rout->buffer + wrapout * newy + newx;
  for (y = y1; y < ylast; y += factor) {
    pixin  = rowin;
    pixout = rowout;
    for (x = x1; x < xlast; x += factor) {
      tmp = 0;
      in  = pixin;
      for (j = 0; j < factor; j += 2) {
        for (i = 0; i < factor; i += 2) {
          tmp += *in;
          in += wrapin;
          in++;
          tmp += *in;
          in -= wrapin;
          in++;
        }
        in += 2 * wrapin - factor;
      }
      valout.r = valout.g = valout.b = (tmp + fac_fac_4) >> fac_fac_2_bits;
      *pixout++                      = valout;
      pixin += factor;
    }
    if (xrest) {
      tmp = 0;
      for (j = 0; j < factor; j++)
        for (i = 0; i < xrest; i++) {
          tmp += pixin[i + j * wrapin];
        }
      valout.r = valout.g = valout.b = (tmp + fac_xrest_2) / fac_xrest;
      *pixout                        = valout;
    }
    rowin += wrapin * factor;
    rowout += wrapout;
  }
  if (yrest) {
    pixin  = rowin;
    pixout = rowout;
    for (x = x1; x < xlast; x += factor) {
      tmp = 0;
      for (j = 0; j < yrest; j++)
        for (i = 0; i < factor; i++) {
          tmp += pixin[i + j * wrapin];
        }
      valout.r = valout.g = valout.b = (tmp + yrest_fac_2) / yrest_fac;
      *pixout++                      = valout;
      pixin += factor;
    }
    if (xrest) {
      tmp = 0;
      for (j = 0; j < yrest; j++)
        for (i = 0; i < xrest; i++) {
          tmp += pixin[i + j * wrapin];
        }
      valout.r = valout.g = valout.b = (tmp + yrest_xrest_2) / yrest_xrest;
      *pixout                        = valout;
    }
  }
  E 47
}

/*---------------------------------------------------------------------------*/

E 28
E 20
E 18
D 2 static void rop_copy_cm_rgbm(RASTER *rin, RASTER *rout, int x1, int y1,
                                   int x2, int y2, int newx, int newy)
E 2
I 2 static void rop_copy_cm16_rgbm(RASTER *rin, RASTER *rout, int x1, int y1,
                                     int x2, int y2, int newx, int newy)
E 2 {
  LPIXEL *rowout, *pixout, *cmap;
  USHORT *rowin, *pixin;
  D 18 int wrapin, wrapout, tmp, cmap_offset;
  E 18
I 18
D 29 int wrapin, wrapout, cmap_offset;
  E 29
I 29 int wrapin, wrapout;
  E 29
E 18 int x, lx, ly;

  D 84 if (!rin->cmap.buffer) {
    D 2 printf("### INTERNAL ERROR - rop_copy_2_4; missing color map\n");
    return;
    E 2
I 2 printf("### INTERNAL ERROR - rop_copy_cm16_rgbm; missing color map\n");
    return;
    E 2
  }
  E 84
D 29 cmap   = rin->cmap.buffer;
  cmap_offset = rin->cmap.offset;
  E 29
I 29
D 56 cmap   = rin->cmap.buffer - rin->cmap.offset;
  E 56
I 56 cmap   = rin->cmap.buffer - rin->cmap.info.offset_mask;
  E 56
E 29 lx     = x2 - x1 + 1;
  ly          = y2 - y1 + 1;
  D 8 wrapin = rin->wrap;
  wrapout     = rout->wrap;
  E 8
I 8
D 10 if ((rin->wrap % sizeof(USHORT)) || (rout->wrap % sizeof(LPIXEL))) {
    printf("### INTERNAL ERROR - rop_copy_cm16_rgbm; bad wrap\n");
    return;
  }
  wrapin      = rin->wrap / sizeof(USHORT);
  wrapout     = rout->wrap / sizeof(LPIXEL);
  E 10
I 10 wrapin = rin->wrap;
  wrapout     = rout->wrap;
  E 10
E 8

D 2 rowin   = (USHORT *)rin->buffer + wrapin * y1 + x1;
  rowout      = (LPIXEL *)rout->buffer + wrapout * newy + newx;
  E 2
I 2 rowin   = (USHORT *)rin->buffer + wrapin * y1 + x1;
  rowout      = (LPIXEL *)rout->buffer + wrapout * newy + newx;
  E 2

      while (ly-- > 0) {
    pixin  = rowin;
    pixout = rowout;
    for (x = 0; x < lx; x++)
			D 29 *pixout++ = cmap[*pixin++ - cmap_offset];
    E 29
I 29 *pixout++    = cmap[*pixin++];
    E 29 rowin += wrapin;
    rowout += wrapout;
  }
}

I 4
    /*---------------------------------------------------------------------------*/

I 84 static void
rop_copy_cm24_rgbm(RASTER *rin, RASTER *rout, int x1, int y1, int x2, int y2,
                   int newx, int newy) {
  LPIXEL *rowout, *pixout, valout, *penmap, *colmap;
  ULONG *rowin, *pixin, valin;
  I 89 UCHAR *exrow, *expix;
  E 89 int wrapin, wrapout;
  D 89 int x, lx, ly;
  E 89
I 89 int x, lx, ly, lines;
  E 89

      penmap   = rin->cmap.penbuffer;
  colmap       = rin->cmap.colbuffer;
  lx           = x2 - x1 + 1;
  ly           = y2 - y1 + 1;
  I 89 lines  = ly;
  E 89 wrapin = rin->wrap;
  wrapout      = rout->wrap;

  rowin  = (ULONG *)rin->buffer + wrapin * y1 + x1;
  rowout = (LPIXEL *)rout->buffer + wrapout * newy + newx;

  D 89 while (ly-- > 0)
E 89
I 89 if (rin->extra_mask & rout->extra_mask)
E 89 {
    D 89 pixin = rowin;
    pixout      = rowout;
    for (x = 0; x < lx; x++) {
      valin = *pixin++;
      MAP24(valin, penmap, colmap, valout)
      *pixout++ = valout;
    }
    rowin += wrapin;
    rowout += wrapout;
    E 89
I 89 exrow = rout->extra + wrapout * newy + newx;
    while (lines-- > 0) {
      pixin  = rowin;
      pixout = rowout;
      expix  = exrow;
      for (x = 0; x < lx; x++) {
        valin = *pixin++;
        MAP24(valin, penmap, colmap, valout)
        *pixout++ = valout;
        *expix++  = (UCHAR)(valin >> 24);
      }
      rowin += wrapin;
      rowout += wrapout;
      exrow += wrapout;
    }
    E 89
  }
  I 89 else while (lines-- > 0) {
    pixin  = rowin;
    pixout = rowout;
    for (x = 0; x < lx; x++) {
      valin = *pixin++;
      MAP24(valin, penmap, colmap, valout)
      *pixout++ = valout;
    }
    rowin += wrapin;
    rowout += wrapout;
  }
  CLEAR_NEW_EXTRA_BITS_OF_RECT(rout, newx, newy, newx + lx - 1, newy + ly - 1,
                               rin)
  E 89
}

/*---------------------------------------------------------------------------*/

E 84
I 58 static void rop_copy_cm16_rgbm64(RASTER *rin, RASTER *rout, int x1,
                                        int y1, int x2, int y2, int newx,
                                        int newy) {
  SPIXEL *rowout, *pixout;
  LPIXEL *cmap, src_pix;
  USHORT *rowin, *pixin;
  int wrapin, wrapout;
  int x, lx, ly;

  D 84 if (!rin->cmap.buffer) {
    printf("### INTERNAL ERROR - rop_copy_cm16_rgbm; missing color map\n");
    return;
  }
  E 84 cmap = rin->cmap.buffer - rin->cmap.info.offset_mask;
  lx         = x2 - x1 + 1;
  ly         = y2 - y1 + 1;
  wrapin     = rin->wrap;
  wrapout    = rout->wrap;

  rowin  = (USHORT *)rin->buffer + wrapin * y1 + x1;
  rowout = (SPIXEL *)rout->buffer + wrapout * newy + newx;

  while (ly-- > 0) {
    pixin  = rowin;
    pixout = rowout;
    for (x = 0; x < lx; x++) {
      src_pix = cmap[*pixin++];
      D 60 ROP_RGBM_TO_RGBM64(src_pix, *pixout)
E 60
I 60 PIX_RGBM_TO_RGBM64(src_pix, *pixout)
E 60 pixout++;
    }
    rowin += wrapin;
    rowout += wrapout;
  }
}

/*---------------------------------------------------------------------------*/

I 84 static void rop_copy_cm24_rgbm64(RASTER *rin, RASTER *rout, int x1,
                                       int y1, int x2, int y2, int newx,
                                       int newy) {
  SPIXEL *rowout, *pixout, valout;
  LPIXEL *penmap, *colmap;
  ULONG *rowin, *pixin, valin;
  I 89 UCHAR *exrow, *expix;
  E 89 int wrapin, wrapout;
  D 89 int x, lx, ly;
  E 89
I 89 int x, lx, ly, lines;
  E 89

      penmap   = rin->cmap.penbuffer;
  colmap       = rin->cmap.colbuffer;
  lx           = x2 - x1 + 1;
  ly           = y2 - y1 + 1;
  I 89 lines  = ly;
  E 89 wrapin = rin->wrap;
  wrapout      = rout->wrap;

  rowin  = (ULONG *)rin->buffer + wrapin * y1 + x1;
  rowout = (SPIXEL *)rout->buffer + wrapout * newy + newx;

  D 89 while (ly-- > 0)
E 89
I 89 if (rin->extra_mask && rout->extra_mask)
E 89 {
    D 89 pixin = rowin;
    pixout      = rowout;
    for (x = 0; x < lx; x++) {
      valin = *pixin++;
      MAP24_64(valin, penmap, colmap, valout)
      *pixout++ = valout;
    }
    rowin += wrapin;
    rowout += wrapout;
    E 89
I 89 exrow = rout->extra + wrapout * newy + newx;
    while (lines-- > 0) {
      pixin  = rowin;
      pixout = rowout;
      expix  = exrow;
      for (x = 0; x < lx; x++) {
        valin = *pixin++;
        MAP24_64(valin, penmap, colmap, valout)
        *pixout++ = valout;
        *expix++  = (UCHAR)(valin >> 24);
      }
      rowin += wrapin;
      rowout += wrapout;
      exrow += wrapout;
    }
    E 89
  }
  I 89 else while (lines-- > 0) {
    pixin  = rowin;
    pixout = rowout;
    for (x = 0; x < lx; x++) {
      valin = *pixin++;
      MAP24_64(valin, penmap, colmap, valout)
      *pixout++ = valout;
    }
    rowin += wrapin;
    rowout += wrapout;
  }
  CLEAR_NEW_EXTRA_BITS_OF_RECT(rout, newx, newy, newx + lx - 1, newy + ly - 1,
                               rin)
  E 89
}

/*---------------------------------------------------------------------------*/

E 84
static void rop_copy_rgbm_rgbm64(RASTER *rin, RASTER *rout,
D 65
                               int x1, int y1, int x2, int y2,
			       int newx, int newy)
E 65
I 65
                                 int x1, int y1, int x2, int y2,
			         int newx, int newy)
E 65
{
  SPIXEL *rowout, *pixout;
  LPIXEL *rowin, *pixin;
  int wrapin, wrapout;
  int x, lx, ly;

  lx      = x2 - x1 + 1;
  ly      = y2 - y1 + 1;
  wrapin  = rin->wrap;
  wrapout = rout->wrap;

  rowin  = (LPIXEL *)rin->buffer + wrapin * y1 + x1;
  rowout = (SPIXEL *)rout->buffer + wrapout * newy + newx;

  while (ly-- > 0) {
    pixin  = rowin;
    pixout = rowout;
    for (x = 0; x < lx; x++) {
      D 60 ROP_RGBM_TO_RGBM64(*pixin, *pixout)
E 60
I 60 PIX_RGBM_TO_RGBM64(*pixin, *pixout)
E 60 pixin++;
      pixout++;
    }
    rowin += wrapin;
    rowout += wrapout;
  }
}

/*---------------------------------------------------------------------------*/

static void rop_copy_rgbm64_rgbm(RASTER *rin, RASTER *rout,
D 65
                               int x1, int y1, int x2, int y2,
			       int newx, int newy)
E 65
I 65
                                 int x1, int y1, int x2, int y2,
			         int newx, int newy)
E 65
{
  LPIXEL *rowout, *pixout;
  SPIXEL *rowin, *pixin;
  int wrapin, wrapout;
  int x, lx, ly;
  I 65
D 66 UINT random_round;

#define BYTE_FROM_USHORT_CUSTOM_ROUNDED(X, R)                                  \
  ((((X)*BYTE_FROM_USHORT_MAGICFAC) + (R)) >> 24)

#define RGBM64_TO_RGBM_CUSTOM_ROUNDED(S, L, R)                                 \
  {                                                                            \
    (L).r = BYTE_FROM_USHORT_CUSTOM_ROUNDED((S).r, (R));                       \
    (L).g = BYTE_FROM_USHORT_CUSTOM_ROUNDED((S).g, (R));                       \
    (L).b = BYTE_FROM_USHORT_CUSTOM_ROUNDED((S).b, (R));                       \
    (L).m = PIX_BYTE_FROM_USHORT((S).m);                                       \
  }
  E 66

D 66 tnz_random_seed(180461);
  E 66
I 66 PIX_INIT_DITHER_RGBM64_TO_RGBM
E 66
E 65

      lx  = x2 - x1 + 1;
  ly      = y2 - y1 + 1;
  wrapin  = rin->wrap;
  wrapout = rout->wrap;

  rowin  = (SPIXEL *)rin->buffer + wrapin * y1 + x1;
  rowout = (LPIXEL *)rout->buffer + wrapout * newy + newx;

  while (ly-- > 0) {
    pixin  = rowin;
    pixout = rowout;
    for (x = 0; x < lx; x++) {
      D 60 ROP_RGBM64_TO_RGBM(*pixin, *pixout)
E 60
I 60
D 65 PIX_RGBM64_TO_RGBM(*pixin, *pixout)
E 65
I 65
D 66 random_round = tnz_random_uint();
      random_round &= (1U << 24) - 1;
      RGBM64_TO_RGBM_CUSTOM_ROUNDED(*pixin, *pixout, random_round)
      E 66
I 66 PIX_DITHER_RGBM64_TO_RGBM(*pixin, *pixout) 
E 66
E 65
E 60 pixin++;
      pixout++;
    }
    rowin += wrapin;
    rowout += wrapout;
  }
}

/*---------------------------------------------------------------------------*/

I 95 static void rop_copy_rgbx64_rgbx(RASTER *rin, RASTER *rout, int x1,
                                       int y1, int x2, int y2, int newx,
                                       int newy) {
  LPIXEL *rowout, *pixout;
  SPIXEL *rowin, *pixin;
  int wrapin, wrapout;
  int x, lx, ly;
  UINT random_round;

  PIX_INIT_DITHER_RGBM64_TO_RGBM

  lx      = x2 - x1 + 1;
  ly      = y2 - y1 + 1;
  wrapin  = rin->wrap;
  wrapout = rout->wrap;

  rowin  = (SPIXEL *)rin->buffer + wrapin * y1 + x1;
  rowout = (LPIXEL *)rout->buffer + wrapout * newy + newx;

  while (ly-- > 0) {
    pixin  = rowin;
    pixout = rowout;
    for (x = 0; x < lx; x++) {
      random_round = tnz_random_uint() & ((1U << 24) - 1);
      pixout->r    = PIX_DITHER_BYTE_FROM_USHORT(pixin->r, random_round);
      pixout->g    = PIX_DITHER_BYTE_FROM_USHORT(pixin->g, random_round);
      pixout->b    = PIX_DITHER_BYTE_FROM_USHORT(pixin->b, random_round);
      pixout->m    = 0xff;
      pixin++;
      pixout++;
    }
    rowin += wrapin;
    rowout += wrapout;
  }
}

/*---------------------------------------------------------------------------*/

E 95
E 58
I 20
D 84 static void rop_reduce_cm16_rgbm(RASTER *rin, RASTER *rout, int x1,
                                        int y1, int x2, int y2, int newx,
                                        int newy, int factor)
E 84
I 84 static void rop_zoom_out_cm16_rgbm(RASTER *rin, RASTER *rout, int x1,
                                          int y1, int x2, int y2, int newx,
                                          int newy, int abs_zoom_level)
E 84 {
  D 28 printf("rop_reduce_cm16_rgbm not implemented yet\n");
  E 28
I 28
D 84 USHORT *rowin, *pixin;
  E 84
I 84 USHORT *rowin, *pixin, *in;
  E 84 LPIXEL *rowout, *pixout, *cmap, valin, valout;
  int tmp_r, tmp_g, tmp_b, tmp_m;
  D 29 int wrapin, wrapout, cmap_offset;
  E 29
I 29 int wrapin, wrapout;
  E 29 int x, y, lx, ly, xlast, ylast, xrest, yrest, i, j;
  I 84 int factor, fac_fac_2_bits;
  E 84 int fac_fac, yrest_fac, fac_xrest, yrest_xrest;
  int fac_fac_2, yrest_fac_2, fac_xrest_2, yrest_xrest_2;
  I 84 int fac_fac_4;
  E 84

D 29 cmap       = rin->cmap.buffer;
  cmap_offset     = rin->cmap.offset;
  E 29
I 29
D 56 cmap       = rin->cmap.buffer - rin->cmap.offset;
  E 56
I 56 cmap       = rin->cmap.buffer - rin->cmap.info.offset_mask;
  E 56
E 29 lx         = x2 - x1 + 1;
  ly              = y2 - y1 + 1;
  I 84 factor    = 1 << abs_zoom_level;
  E 84
D 61 xrest      = lx % factor;
  yrest           = ly % factor;
  E 61
I 61 xrest      = lx & (factor - 1);
  yrest           = ly & (factor - 1);
  E 61 xlast     = x2 - xrest + 1;
  ylast           = y2 - yrest + 1;
  fac_fac         = factor * factor;
  fac_fac_2       = fac_fac >> 1;
  I 84 fac_fac_4 = fac_fac >> 2;
  fac_fac_2_bits  = 2 * abs_zoom_level - 1;
  E 84 yrest_fac = yrest * factor;
  yrest_fac_2     = yrest_fac >> 1;
  fac_xrest       = factor * xrest;
  fac_xrest_2     = fac_xrest >> 1;
  yrest_xrest     = yrest * xrest;
  yrest_xrest_2   = yrest_xrest >> 1;
  wrapin          = rin->wrap;
  wrapout         = rout->wrap;
  I 84 valout.m  = 0xff;
  E 84

      rowin = (USHORT *)rin->buffer + wrapin * y1 + x1;
  rowout    = (LPIXEL *)rout->buffer + wrapout * newy + newx;
  for (y = y1; y < ylast; y += factor) {
    pixin  = rowin;
    pixout = rowout;
    for (x = x1; x < xlast; x += factor) {
      tmp_r = tmp_g = tmp_b = tmp_m = 0;
      D 84 for (j = 0; j < factor; j++) for (i = 0; i < factor; i++)
E 84
I 84 in = pixin;
      for (j = 0; j < factor; j += 2) {
        for (i = 0; i < factor; i += 2)
					E 84 {
            D 29 valin = cmap[pixin[i + j * wrapin] - cmap_offset];
            E 29
I 29
D 84 valin            = cmap[pixin[i + j * wrapin]];
            E 84
I 84 valin            = cmap[*in];
            tmp_r += valin.r;
            tmp_g += valin.g;
            tmp_b += valin.b;
            tmp_m += valin.m;
            in += wrapin;
            in++;
            valin = cmap[*in];
            E 84
E 29 tmp_r += valin.r;
            tmp_g += valin.g;
            tmp_b += valin.b;
            tmp_m += valin.m;
            I 84 in -= wrapin;
            in++;
            E 84
          }
        D 84 valout.r = (tmp_r + fac_fac_2) / fac_fac;
        valout.g       = (tmp_g + fac_fac_2) / fac_fac;
        valout.b       = (tmp_b + fac_fac_2) / fac_fac;
        valout.m       = (tmp_m + fac_fac_2) / fac_fac;
        E 84
I 84 in += 2 * wrapin - factor;
      }
      valout.r        = (tmp_r + fac_fac_4) >> fac_fac_2_bits;
      valout.g        = (tmp_g + fac_fac_4) >> fac_fac_2_bits;
      valout.b        = (tmp_b + fac_fac_4) >> fac_fac_2_bits;
      valout.m        = (tmp_m + fac_fac_4) >> fac_fac_2_bits;
      E 84 *pixout++ = valout;
      pixin += factor;
    }
    if (xrest) {
      tmp_r = tmp_g = tmp_b = tmp_m = 0;
      for (j = 0; j < factor; j++)
        for (i = 0; i < xrest; i++) {
          D 29 valin = cmap[pixin[i + j * wrapin] - cmap_offset];
          E 29
I 29 valin          = cmap[pixin[i + j * wrapin]];
          E 29 tmp_r += valin.r;
          tmp_g += valin.g;
          tmp_b += valin.b;
          tmp_m += valin.m;
        }
      valout.r = (tmp_r + fac_xrest_2) / fac_xrest;
      valout.g = (tmp_g + fac_xrest_2) / fac_xrest;
      valout.b = (tmp_b + fac_xrest_2) / fac_xrest;
      valout.m = (tmp_m + fac_xrest_2) / fac_xrest;
      *pixout  = valout;
    }
    rowin += wrapin * factor;
    rowout += wrapout;
  }
  if (yrest) {
    pixin  = rowin;
    pixout = rowout;
    for (x = x1; x < xlast; x += factor) {
      tmp_r = tmp_g = tmp_b = tmp_m = 0;
      for (j = 0; j < yrest; j++)
        for (i = 0; i < factor; i++) {
          D 29 valin = cmap[pixin[i + j * wrapin] - cmap_offset];
          E 29
I 29 valin          = cmap[pixin[i + j * wrapin]];
          E 29 tmp_r += valin.r;
          tmp_g += valin.g;
          tmp_b += valin.b;
          tmp_m += valin.m;
        }
      valout.r  = (tmp_r + yrest_fac_2) / yrest_fac;
      valout.g  = (tmp_g + yrest_fac_2) / yrest_fac;
      valout.b  = (tmp_b + yrest_fac_2) / yrest_fac;
      valout.m  = (tmp_m + yrest_fac_2) / yrest_fac;
      *pixout++ = valout;
      pixin += factor;
    }
    if (xrest) {
      tmp_r = tmp_g = tmp_b = tmp_m = 0;
      for (j = 0; j < yrest; j++)
        for (i = 0; i < xrest; i++) {
          D 29 valin = cmap[pixin[i + j * wrapin] - cmap_offset];
          E 29
I 29 valin          = cmap[pixin[i + j * wrapin]];
          E 29 tmp_r += valin.r;
          tmp_g += valin.g;
          tmp_b += valin.b;
          tmp_m += valin.m;
        }
      valout.r = (tmp_r + yrest_xrest_2) / yrest_xrest;
      valout.g = (tmp_g + yrest_xrest_2) / yrest_xrest;
      valout.b = (tmp_b + yrest_xrest_2) / yrest_xrest;
      valout.m = (tmp_m + yrest_xrest_2) / yrest_xrest;
      *pixout  = valout;
    }
  }
}

/*---------------------------------------------------------------------------*/

D 84 static void rop_zoom_out_cm16_rgbm(RASTER *rin, RASTER *rout,
E 84
I 84 static void rop_zoom_out_cm24_rgbm(RASTER *rin, RASTER *rout,
E 84
D 31 int x1, int y1, int x2, int y2, int newx, int newy, int factor)
E 31
I 31 int x1, int y1, int x2, int y2, int newx, int newy, int abs_zoom_level)
E 31 {
  D 31 printf(" rop_zoom_out_cm16_rgbm not implemented yet\n");
  E 31
I 31
D 84 USHORT *rowin, *pixin, *in;
  LPIXEL *rowout, *pixout, *cmap, valin, valout;
  E 84
I 84 ULONG *rowin, *pixin, *in, win;
  LPIXEL *rowout, *pixout, *penmap, *colmap, valin, valout;
  E 84 int tmp_r, tmp_g, tmp_b, tmp_m;
  int wrapin, wrapout;
  int x, y, lx, ly, xlast, ylast, xrest, yrest, i, j;
  int factor, fac_fac_2_bits;
  int fac_fac, yrest_fac, fac_xrest, yrest_xrest;
  int fac_fac_2, yrest_fac_2, fac_xrest_2, yrest_xrest_2;
  int fac_fac_4;

  D 56 cmap     = rin->cmap.buffer - rin->cmap.offset;
  E 56
I 56
D 84 cmap      = rin->cmap.buffer - rin->cmap.info.offset_mask;
  E 84
I 84 penmap    = rin->cmap.penbuffer;
  colmap         = rin->cmap.colbuffer;
  E 84
E 56 lx        = x2 - x1 + 1;
  ly             = y2 - y1 + 1;
  factor         = 1 << abs_zoom_level;
  D 61 xrest    = lx % factor;
  yrest          = ly % factor;
  E 61
I 61 xrest     = lx & (factor - 1);
  yrest          = ly & (factor - 1);
  E 61 xlast    = x2 - xrest + 1;
  ylast          = y2 - yrest + 1;
  fac_fac        = factor * factor;
  fac_fac_2      = fac_fac >> 1;
  fac_fac_4      = fac_fac >> 2;
  fac_fac_2_bits = 2 * abs_zoom_level - 1;
  yrest_fac      = yrest * factor;
  yrest_fac_2    = yrest_fac >> 1;
  fac_xrest      = factor * xrest;
  fac_xrest_2    = fac_xrest >> 1;
  yrest_xrest    = yrest * xrest;
  yrest_xrest_2  = yrest_xrest >> 1;
  wrapin         = rin->wrap;
  wrapout        = rout->wrap;
  valout.m       = 0xff;

  D 84 rowin  = (USHORT *)rin->buffer + wrapin * y1 + x1;
  E 84
I 84 rowin   = (ULONG *)rin->buffer + wrapin * y1 + x1;
  E 84 rowout = (LPIXEL *)rout->buffer + wrapout * newy + newx;
  for (y = y1; y < ylast; y += factor) {
    pixin  = rowin;
    pixout = rowout;
    for (x = x1; x < xlast; x += factor) {
      tmp_r = tmp_g = tmp_b = tmp_m = 0;
      in                            = pixin;
      for (j = 0; j < factor; j += 2) {
        for (i = 0; i < factor; i += 2) {
          D 84 valin = cmap[*in];
          E 84
I 84 win            = *in;
          MAP24(win, penmap, colmap, valin)
          E 84 tmp_r += valin.r;
          tmp_g += valin.g;
          tmp_b += valin.b;
          tmp_m += valin.m;
          D 38 in += factor;
          in++;
          E 38
I 38 in += wrapin;
          in++;
          E 38
D 84 valin = cmap[*in];
          E 84
I 84 win   = *in;
          MAP24(win, penmap, colmap, valin)
          E 84 tmp_r += valin.r;
          tmp_g += valin.g;
          tmp_b += valin.b;
          tmp_m += valin.m;
          D 38 in -= factor;
          E 38
I 38 in -= wrapin;
          E 38 in++;
        }
        in += 2 * wrapin - factor;
      }
      valout.r  = (tmp_r + fac_fac_4) >> fac_fac_2_bits;
      valout.g  = (tmp_g + fac_fac_4) >> fac_fac_2_bits;
      valout.b  = (tmp_b + fac_fac_4) >> fac_fac_2_bits;
      valout.m  = (tmp_m + fac_fac_4) >> fac_fac_2_bits;
      *pixout++ = valout;
      pixin += factor;
    }
    if (xrest) {
      tmp_r = tmp_g = tmp_b = tmp_m = 0;
      for (j = 0; j < factor; j++)
        for (i = 0; i < xrest; i++) {
          D 84 valin = cmap[pixin[i + j * wrapin]];
          E 84
I 84 win            = pixin[i + j * wrapin];
          MAP24(win, penmap, colmap, valin)
          E 84 tmp_r += valin.r;
          tmp_g += valin.g;
          tmp_b += valin.b;
          tmp_m += valin.m;
        }
      valout.r = (tmp_r + fac_xrest_2) / fac_xrest;
      valout.g = (tmp_g + fac_xrest_2) / fac_xrest;
      valout.b = (tmp_b + fac_xrest_2) / fac_xrest;
      valout.m = (tmp_m + fac_xrest_2) / fac_xrest;
      *pixout  = valout;
    }
    rowin += wrapin * factor;
    rowout += wrapout;
  }
  if (yrest) {
    pixin  = rowin;
    pixout = rowout;
    for (x = x1; x < xlast; x += factor) {
      tmp_r = tmp_g = tmp_b = tmp_m = 0;
      for (j = 0; j < yrest; j++)
        for (i = 0; i < factor; i++) {
          D 84 valin = cmap[pixin[i + j * wrapin]];
          E 84
I 84 win            = pixin[i + j * wrapin];
          MAP24(win, penmap, colmap, valin)
          E 84 tmp_r += valin.r;
          tmp_g += valin.g;
          tmp_b += valin.b;
          tmp_m += valin.m;
        }
      valout.r  = (tmp_r + yrest_fac_2) / yrest_fac;
      valout.g  = (tmp_g + yrest_fac_2) / yrest_fac;
      valout.b  = (tmp_b + yrest_fac_2) / yrest_fac;
      valout.m  = (tmp_m + yrest_fac_2) / yrest_fac;
      *pixout++ = valout;
      pixin += factor;
    }
    if (xrest) {
      tmp_r = tmp_g = tmp_b = tmp_m = 0;
      for (j = 0; j < yrest; j++)
        for (i = 0; i < xrest; i++) {
          D 84 valin = cmap[pixin[i + j * wrapin]];
          E 84
I 84 win            = pixin[i + j * wrapin];
          MAP24(win, penmap, colmap, valin)
          E 84 tmp_r += valin.r;
          tmp_g += valin.g;
          tmp_b += valin.b;
          tmp_m += valin.m;
        }
      valout.r = (tmp_r + yrest_xrest_2) / yrest_xrest;
      valout.g = (tmp_g + yrest_xrest_2) / yrest_xrest;
      valout.b = (tmp_b + yrest_xrest_2) / yrest_xrest;
      valout.m = (tmp_m + yrest_xrest_2) / yrest_xrest;
      *pixout  = valout;
    }
  }
  E 31
}

/*---------------------------------------------------------------------------*/

D 46 static void rop_reduce_cm16_rgb_(RASTER *rin, RASTER *rout,
E 46
I 46
D 84 static void rop_reduce_cm16_rgbx(RASTER *rin, RASTER *rout,
E 46 int x1, int y1, int x2, int y2,
													int newx, int newy, int factor)
E 84
I 84 static void rop_zoom_out_cm16_rgbx(RASTER *rin, RASTER *rout,
													int x1, int y1, int x2, int y2,
													int newx, int newy, int abs_zoom_level)
E 84 {
	D 84 USHORT *rowin, *pixin;
	E 84
I 84 USHORT *rowin, *pixin, *in;
	E 84 LPIXEL *rowout, *pixout, *cmap, valin, valout;
	D 84 int tmp_r, tmp_g, tmp_b, tmp_m;
	E 84
I 84 int tmp_r, tmp_g, tmp_b;
	E 84
D 29 int wrapin, wrapout, cmap_offset;
	E 29
I 29 int wrapin, wrapout;
	E 29 int x, y, lx, ly, xlast, ylast, xrest, yrest, i, j;
	I 84 int factor, fac_fac_2_bits;
	E 84 int fac_fac, yrest_fac, fac_xrest, yrest_xrest;
	int fac_fac_2, yrest_fac_2, fac_xrest_2, yrest_xrest_2;
	I 84 int fac_fac_4;
	E 84

D 29 cmap = rin->cmap.buffer;
	cmap_offset = rin->cmap.offset;
	E 29
I 29
D 56 cmap = rin->cmap.buffer - rin->cmap.offset;
	E 56
I 56 cmap = rin->cmap.buffer - rin->cmap.info.offset_mask;
	E 56
E 29 lx = x2 - x1 + 1;
	ly = y2 - y1 + 1;
	I 84 factor = 1 << abs_zoom_level;
	E 84
D 61 xrest = lx % factor;
	yrest = ly % factor;
	E 61
I 61 xrest = lx & (factor - 1);
	yrest = ly & (factor - 1);
	E 61 xlast = x2 - xrest + 1;
	ylast = y2 - yrest + 1;
	fac_fac = factor * factor;
	fac_fac_2 = fac_fac >> 1;
	I 84 fac_fac_4 = fac_fac >> 2;
	fac_fac_2_bits = 2 * abs_zoom_level - 1;
	E 84 yrest_fac = yrest * factor;
	yrest_fac_2 = yrest_fac >> 1;
	fac_xrest = factor * xrest;
	fac_xrest_2 = fac_xrest >> 1;
	yrest_xrest = yrest * xrest;
	yrest_xrest_2 = yrest_xrest >> 1;
	wrapin = rin->wrap;
	wrapout = rout->wrap;
	valout.m = 0xff;

	rowin = (USHORT *)rin->buffer + wrapin * y1 + x1;
	rowout = (LPIXEL *)rout->buffer + wrapout * newy + newx;
	for (y = y1; y < ylast; y += factor) {
		pixin = rowin;
		pixout = rowout;
		for (x = x1; x < xlast; x += factor) {
			D 84 tmp_r = tmp_g = tmp_b = tmp_m = 0;
			for (j = 0; j < factor; j++)
				for (i = 0; i < factor; i++)
					E 84
I 84 tmp_r = tmp_g = tmp_b = 0;
			in = pixin;
			for (j = 0; j < factor; j += 2) {
				for (i = 0; i < factor; i += 2)
					E 84
					{
						D 29 valin = cmap[pixin[i + j * wrapin] - cmap_offset];
						E 29
I 29
D 84 valin = cmap[pixin[i + j * wrapin]];
						E 84
I 84 valin = cmap[*in];
						tmp_r += valin.r;
						tmp_g += valin.g;
						tmp_b += valin.b;
						in += wrapin;
						in++;
						valin = cmap[*in];
						E 84
E 29 tmp_r += valin.r;
						tmp_g += valin.g;
						tmp_b += valin.b;
						I 84 in -= wrapin;
						in++;
						E 84
					}
				D 84 valout.r = (tmp_r + fac_fac_2) / fac_fac;
				valout.g = (tmp_g + fac_fac_2) / fac_fac;
				valout.b = (tmp_b + fac_fac_2) / fac_fac;
				E 84
I 84 in += 2 * wrapin - factor;
			}
			valout.r = (tmp_r + fac_fac_4) >> fac_fac_2_bits;
			valout.g = (tmp_g + fac_fac_4) >> fac_fac_2_bits;
			valout.b = (tmp_b + fac_fac_4) >> fac_fac_2_bits;
			E 84
				*pixout++ = valout;
			pixin += factor;
		}
		if (xrest) {
			D 84 tmp_r = tmp_g = tmp_b = tmp_m = 0;
			E 84
I 84 tmp_r = tmp_g = tmp_b = 0;
			E 84 for (j = 0; j < factor; j++) for (i = 0; i < xrest; i++)
			{
				D 29 valin = cmap[pixin[i + j * wrapin] - cmap_offset];
				E 29
I 29 valin = cmap[pixin[i + j * wrapin]];
				E 29 tmp_r += valin.r;
				tmp_g += valin.g;
				tmp_b += valin.b;
			}
			valout.r = (tmp_r + fac_xrest_2) / fac_xrest;
			valout.g = (tmp_g + fac_xrest_2) / fac_xrest;
			valout.b = (tmp_b + fac_xrest_2) / fac_xrest;
			*pixout = valout;
		}
		rowin += wrapin * factor;
		rowout += wrapout;
	}
	if (yrest) {
		pixin = rowin;
		pixout = rowout;
		for (x = x1; x < xlast; x += factor) {
			D 84 tmp_r = tmp_g = tmp_b = tmp_m = 0;
			E 84
I 84 tmp_r = tmp_g = tmp_b = 0;
			E 84 for (j = 0; j < yrest; j++) for (i = 0; i < factor; i++)
			{
				D 29 valin = cmap[pixin[i + j * wrapin] - cmap_offset];
				E 29
I 29 valin = cmap[pixin[i + j * wrapin]];
				E 29 tmp_r += valin.r;
				tmp_g += valin.g;
				tmp_b += valin.b;
			}
			valout.r = (tmp_r + yrest_fac_2) / yrest_fac;
			valout.g = (tmp_g + yrest_fac_2) / yrest_fac;
			valout.b = (tmp_b + yrest_fac_2) / yrest_fac;
			*pixout++ = valout;
			pixin += factor;
		}
		if (xrest) {
			D 84 tmp_r = tmp_g = tmp_b = tmp_m = 0;
			E 84
I 84 tmp_r = tmp_g = tmp_b = 0;
			E 84 for (j = 0; j < yrest; j++) for (i = 0; i < xrest; i++)
			{
				D 29 valin = cmap[pixin[i + j * wrapin] - cmap_offset];
				E 29
I 29 valin = cmap[pixin[i + j * wrapin]];
				E 29 tmp_r += valin.r;
				tmp_g += valin.g;
				tmp_b += valin.b;
			}
			valout.r = (tmp_r + yrest_xrest_2) / yrest_xrest;
			valout.g = (tmp_g + yrest_xrest_2) / yrest_xrest;
			valout.b = (tmp_b + yrest_xrest_2) / yrest_xrest;
			*pixout = valout;
		}
	}
}

									   /*---------------------------------------------------------------------------*/

D 46 static void rop_zoom_out_cm16_rgb_(RASTER *rin, RASTER *rout,
E 46
I 46
D 84 static void rop_zoom_out_cm16_rgbx(RASTER *rin, RASTER *rout,
E 84
I 84 static void rop_zoom_out_cm24_rgbx(RASTER *rin, RASTER *rout,
E 84
E 46 int x1, int y1, int x2, int y2, int newx, int newy, int abs_zoom_level) {
D 29
USHORT *rowin, *pixin;
E 29
I 29
D 84
USHORT *rowin, *pixin, *in;
E 29
LPIXEL *rowout, *pixout, *cmap, valin, valout;
E 84
I 84
ULONG *rowin, *pixin, *in, win;
LPIXEL *rowout, *pixout, *penmap, *colmap, valin, valout;
E 84
D 46
int tmp_r, tmp_g, tmp_b, tmp_m;
E 46
I 46
int tmp_r, tmp_g, tmp_b;
E 46
D 29
int wrapin, wrapout, cmap_offset;
E 29
I 29
int wrapin, wrapout;
E 29
int x, y, lx, ly, xlast, ylast, xrest, yrest, i, j;
D 29
int factor, fac_fac_bits;
E 29
I 29
int factor, fac_fac_2_bits;
E 29
int fac_fac  , yrest_fac,   fac_xrest,   yrest_xrest;
int fac_fac_2, yrest_fac_2, fac_xrest_2, yrest_xrest_2;
I 29
int fac_fac_4;
E 29

D 29
cmap=rin->cmap.buffer;
cmap_offset=rin->cmap.offset;
E 29
I 29
D 56
cmap=rin->cmap.buffer - rin->cmap.offset;
E 56
I 56
D 84
cmap=rin->cmap.buffer - rin->cmap.info.offset_mask;
E 84
I 84
penmap=rin->cmap.penbuffer;
colmap=rin->cmap.colbuffer;
E 84
E 56
E 29
lx=x2-x1+1;
ly=y2-y1+1;
factor = 1 << abs_zoom_level;
D 61
xrest = lx % factor;
yrest = ly % factor;
E 61
I 61
xrest = lx & (factor - 1);
yrest = ly & (factor - 1);
E 61
xlast = x2 - xrest + 1;
ylast = y2 - yrest + 1;
fac_fac     = factor * factor; fac_fac_2     = fac_fac     >> 1;
D 29
fac_fac_bits= 2 * abs_zoom_level;
E 29
I 29
fac_fac_4   = fac_fac >> 2;    fac_fac_2_bits= 2 * abs_zoom_level - 1;
E 29
yrest_fac   = yrest  * factor; yrest_fac_2   = yrest_fac   >> 1;
fac_xrest   = factor * xrest;  fac_xrest_2   = fac_xrest   >> 1;
yrest_xrest = yrest  * xrest;  yrest_xrest_2 = yrest_xrest >> 1;
wrapin  = rin ->wrap;
wrapout = rout->wrap;
valout.m = 0xff;

D 84
rowin  = (USHORT*)rin->buffer + wrapin*y1 + x1;
E 84
I 84
rowin  = (ULONG *)rin->buffer + wrapin*y1 + x1;
E 84
rowout = (LPIXEL*)rout->buffer + wrapout*newy + newx;
for (y = y1; y < ylast; y += factor)
  {
  pixin  = rowin;
  pixout = rowout;
  for (x = x1; x < xlast; x += factor)
    {
D 46
    tmp_r = tmp_g = tmp_b = tmp_m = 0;
E 46
I 46
    tmp_r = tmp_g = tmp_b = 0;
E 46
D 29
    for (j = 0; j < factor; j++)
      for (i = 0; i < factor; i++)
E 29
I 29
    in = pixin;
    for (j = 0; j < factor; j+=2)
      {
      for (i = 0; i < factor; i+=2)
E 29
        {
D 29
        valin = cmap[pixin[i + j*wrapin] - cmap_offset];
E 29
I 29
D 84
        valin = cmap[*in];
E 84
I 84
	win = *in;
        MAP24 (win, penmap, colmap, valin)
E 84
E 29
	tmp_r += valin.r;
	tmp_g += valin.g;
	tmp_b += valin.b;
I 29
D 38
	in += factor;
E 38
I 38
	in += wrapin;
E 38
        in++;
D 84
        valin = cmap[*in];
E 84
I 84
	win = *in;
        MAP24 (win, penmap, colmap, valin)
E 84
	tmp_r += valin.r;
	tmp_g += valin.g;
	tmp_b += valin.b;
D 38
	in -= factor;
E 38
I 38
	in -= wrapin;
E 38
        in++;
E 29
	}
D 29
    valout.r = (tmp_r + fac_fac_2) >> fac_fac_bits;
    valout.g = (tmp_g + fac_fac_2) >> fac_fac_bits;
    valout.b = (tmp_b + fac_fac_2) >> fac_fac_bits;
E 29
I 29
      in += 2 * wrapin - factor;
      }
    valout.r = (tmp_r + fac_fac_4) >> fac_fac_2_bits;
    valout.g = (tmp_g + fac_fac_4) >> fac_fac_2_bits;
    valout.b = (tmp_b + fac_fac_4) >> fac_fac_2_bits;
E 29
    *pixout++ = valout;
    pixin += factor;
    }
  if (xrest)
    {
D 46
    tmp_r = tmp_g = tmp_b = tmp_m = 0;
E 46
I 46
    tmp_r = tmp_g = tmp_b = 0;
E 46
    for (j = 0; j < factor; j++)
      for (i = 0; i < xrest; i++)
        {
D 29
        valin = cmap[pixin[i + j*wrapin] - cmap_offset];
E 29
I 29
D 84
        valin = cmap[pixin[i + j*wrapin]];
E 84
I 84
	win = pixin[i + j*wrapin];
        MAP24 (win, penmap, colmap, valin)
E 84
E 29
	tmp_r += valin.r;
	tmp_g += valin.g;
	tmp_b += valin.b;
	}
    valout.r = (tmp_r + fac_xrest_2) / fac_xrest;
    valout.g = (tmp_g + fac_xrest_2) / fac_xrest;
    valout.b = (tmp_b + fac_xrest_2) / fac_xrest;
    *pixout = valout;
    }
  rowin  += wrapin * factor;
  rowout += wrapout;
  }
if (yrest)
  {
  pixin  = rowin;
  pixout = rowout;
  for (x = x1; x < xlast; x += factor)
    {
D 46
    tmp_r = tmp_g = tmp_b = tmp_m = 0;
E 46
I 46
    tmp_r = tmp_g = tmp_b = 0;
E 46
    for (j = 0; j < yrest; j++)
      for (i = 0; i < factor; i++)
        {
D 29
        valin = cmap[pixin[i + j*wrapin] - cmap_offset];
E 29
I 29
D 84
        valin = cmap[pixin[i + j*wrapin]];
E 84
I 84
	win = pixin[i + j*wrapin];
        MAP24 (win, penmap, colmap, valin)
E 84
E 29
	tmp_r += valin.r;
	tmp_g += valin.g;
	tmp_b += valin.b;
	}
    valout.r = (tmp_r + yrest_fac_2) / yrest_fac;
    valout.g = (tmp_g + yrest_fac_2) / yrest_fac;
    valout.b = (tmp_b + yrest_fac_2) / yrest_fac;
    *pixout++ = valout;
    pixin += factor;
    }
  if (xrest)
    {
D 46
    tmp_r = tmp_g = tmp_b = tmp_m = 0;
E 46
I 46
    tmp_r = tmp_g = tmp_b = 0;
E 46
    for (j = 0; j < yrest; j++)
      for (i = 0; i < xrest; i++)
        {
D 29
        valin = cmap[pixin[i + j*wrapin] - cmap_offset];
E 29
I 29
D 84
        valin = cmap[pixin[i + j*wrapin]];
E 84
I 84
	win = pixin[i + j*wrapin];
        MAP24 (win, penmap, colmap, valin)
E 84
E 29
	tmp_r += valin.r;
	tmp_g += valin.g;
	tmp_b += valin.b;
	}
    valout.r = (tmp_r + yrest_xrest_2) / yrest_xrest;
    valout.g = (tmp_g + yrest_xrest_2) / yrest_xrest;
    valout.b = (tmp_b + yrest_xrest_2) / yrest_xrest;
    *pixout = valout;
    }
  }
E 28 }

													   /*---------------------------------------------------------------------------*/

I 81 static void rop_copy_rgb_rgb16(RASTER *rin, RASTER *rout, int x1, int y1, int x2, int y2, int newx, int newy) {
USHORT *rowout, *pixout;
UCHAR  *rowin, *pixin;
int bytewrapin, wrapout;
int x, lx, ly;

lx=x2-x1+1;
ly=y2-y1+1;
bytewrapin = rin ->wrap * 3;
wrapout    = rout->wrap;

rowin  = (UCHAR*) rin ->buffer + bytewrapin * y1 + x1 * 3;
rowout = (USHORT*)rout->buffer + wrapout * newy  + newx;

while(ly-->0)
  {
   pixin=rowin;
   pixout=rowout;
   for(x=0;x<lx;x++)
     {
     *pixout++ = PIX_RGB16_FROM_BYTES(pixin[0], pixin[1], pixin[2]);
     pixin += 3;
     }
   rowin+=bytewrapin;
   rowout+=wrapout;
  } }

													   /*---------------------------------------------------------------------------*/

E 81
E 20
I 18 static void rop_copy_rgb_rgbm(RASTER *rin, RASTER *rout, int x1, int y1, int x2, int y2, int newx, int newy) {
LPIXEL *rowout, *pixout;
UCHAR  *rowin, *pixin;
D 81
int wrapin, wrapout;
E 81
I 81
int bytewrapin, wrapout;
E 81
int x, lx, ly;
LPIXEL tmp;

lx=x2-x1+1;
ly=y2-y1+1;
D 81
wrapin  = rin ->wrap * 3;
wrapout = rout->wrap;
E 81
I 81
bytewrapin = rin ->wrap * 3;
wrapout    = rout->wrap;
E 81

D 24
rowin  = (UCHAR*) rin ->buffer + wrapin*y1    + x1;
E 24
I 24
D 81
rowin  = (UCHAR*) rin ->buffer + wrapin*y1    + x1 * 3;
E 24
rowout = (LPIXEL*)rout->buffer + wrapout*newy + newx;
E 81
I 81
rowin  = (UCHAR*) rin ->buffer + bytewrapin * y1 + x1 * 3;
rowout = (LPIXEL*)rout->buffer + wrapout * newy  + newx;
E 81

I 45
tmp.m = 0xff;
E 45
while(ly-->0)
  {
   pixin=rowin;
   pixout=rowout;
   for(x=0;x<lx;x++)
     {
     tmp.r = *pixin++;
     tmp.g = *pixin++;
     tmp.b = *pixin++;
D 45
     tmp.m = 0xff;
E 45
     *pixout++ = tmp;
     }
D 81
   rowin+=wrapin;
E 81
I 81
   rowin+=bytewrapin;
E 81
   rowout+=wrapout;
  } }

													   /*---------------------------------------------------------------------------*/

I 67 static void rop_copy_rgbx_rgb(RASTER *rin, RASTER *rout, int x1, int y1, int x2, int y2, int newx, int newy) {
UCHAR *rowout, *pixout;
LPIXEL  *rowin, *pixin;
int wrapin, wrapout;
int x, lx, ly;

lx=x2-x1+1;
ly=y2-y1+1;
wrapin  = rin ->wrap;
wrapout = rout->wrap * 3;

rowin  = (LPIXEL*) rin ->buffer + wrapin*y1    + x1;
rowout = (UCHAR*)rout->buffer + wrapout*newy + newx * 3;

while(ly-->0)
  {
   pixin=rowin;
   pixout=rowout;
   for(x=0;x<lx;x++, pixin++)
     {
     *pixout++ = pixin->b;
     *pixout++ = pixin->g;
     *pixout++ = pixin->r;
     }
   rowin+=wrapin;
   rowout+=wrapout;
  } }

													   /*---------------------------------------------------------------------------*/

I 81
D 84 static void rop_reduce_rgb_rgb16(RASTER *rin, RASTER *rout, int x1, int y1, int x2, int y2, int newx, int newy, int factor) { printf("rop_reduce_rgb_rgb16 not implemented yet\n"); }

													   /*---------------------------------------------------------------------------*/

E 81
E 67
I 20 static void rop_reduce_rgb_rgbm(RASTER *rin, RASTER *rout, int x1, int y1, int x2, int y2, int newx, int newy, int factor) { printf("rop_reduce_rgb_rgbm not implemented yet\n"); }

													   /*---------------------------------------------------------------------------*/

E 84
I 81 static void rop_zoom_out_rgb_rgb16(RASTER *rin, RASTER *rout, int x1, int y1, int x2, int y2, int newx, int newy, int abs_zoom_level) {
UCHAR  *rowin, *pixin, *in;
USHORT *rowout, *pixout;
int tmp_r, tmp_g, tmp_b;
int wrapin_pixels, wrapin_bytes, wrapout;
int x, y, lx, ly, xlast, ylast, xrest, yrest, i, j;
int factor, fac_fac_2_bits;
int fac_fac  , yrest_fac,   fac_xrest,   yrest_xrest;
int fac_fac_2, yrest_fac_2, fac_xrest_2, yrest_xrest_2;
int fac_fac_4;

lx=x2-x1+1;
ly=y2-y1+1;
factor = 1 << abs_zoom_level;
xrest = lx & (factor - 1);
yrest = ly & (factor - 1);
xlast = x2 - xrest + 1;
ylast = y2 - yrest + 1;
fac_fac     = factor * factor; fac_fac_2     = fac_fac     >> 1;
fac_fac_4   = fac_fac >> 2;    fac_fac_2_bits= 2 * abs_zoom_level - 1;
yrest_fac   = yrest  * factor; yrest_fac_2   = yrest_fac   >> 1;
fac_xrest   = factor * xrest;  fac_xrest_2   = fac_xrest   >> 1;
yrest_xrest = yrest  * xrest;  yrest_xrest_2 = yrest_xrest >> 1;
wrapin_pixels = rin->wrap;
wrapin_bytes  = wrapin_pixels * 3;
wrapout = rout->wrap;

rowin  = (UCHAR *)rin->buffer + wrapin_bytes * y1 + 3 * x1;
rowout = (USHORT*)rout->buffer + wrapout * newy + newx;
for (y = y1; y < ylast; y += factor)
  {
  pixin  = rowin;
  pixout = rowout;
  for (x = x1; x < xlast; x += factor)
    {
    tmp_r = tmp_g = tmp_b = 0;
    in = pixin;
    for (j = 0; j < factor; j+=2)
      {
      for (i = 0; i < factor; i+=2)
        {
        tmp_r += *in++;
        tmp_g += *in++;
        tmp_b += *in++;
	in += wrapin_bytes;
        tmp_r += *in++;
        tmp_g += *in++;
        tmp_b += *in++;
	in -= wrapin_bytes;
	}
      in += 6 * wrapin_pixels - 3 * factor;
      }
    tmp_r = (tmp_r + fac_fac_4) >> fac_fac_2_bits;
    tmp_g = (tmp_g + fac_fac_4) >> fac_fac_2_bits;
    tmp_b = (tmp_b + fac_fac_4) >> fac_fac_2_bits;
    *pixout++ = PIX_RGB16_FROM_BYTES (tmp_r, tmp_g, tmp_b);
    pixin += 3 * factor;
    }
  if (xrest)
    {
    tmp_r = tmp_g = tmp_b = 0;
    for (j = 0; j < factor; j++)
      for (i = 0; i < xrest; i++)
        {
	tmp_r += pixin[3*i + j*wrapin_bytes];
	tmp_g += pixin[3*i + j*wrapin_bytes + 1];
	tmp_b += pixin[3*i + j*wrapin_bytes + 2];
	}
    tmp_r = (tmp_r + fac_xrest_2) / fac_xrest;
    tmp_g = (tmp_g + fac_xrest_2) / fac_xrest;
    tmp_b = (tmp_b + fac_xrest_2) / fac_xrest;
    *pixout = PIX_RGB16_FROM_BYTES (tmp_r, tmp_g, tmp_b);
    }
  rowin  += wrapin_bytes * factor;
  rowout += wrapout;
  }
if (yrest)
  {
  pixin  = rowin;
  pixout = rowout;
  for (x = x1; x < xlast; x += factor)
    {
    tmp_r = tmp_g = tmp_b = 0;
    for (j = 0; j < yrest; j++)
      for (i = 0; i < factor; i++)
        {
	tmp_r += pixin[3*i + j*wrapin_bytes];
	tmp_g += pixin[3*i + j*wrapin_bytes + 1];
	tmp_b += pixin[3*i + j*wrapin_bytes + 2];
	}
    tmp_r = (tmp_r + yrest_fac_2) / yrest_fac;
    tmp_g = (tmp_g + yrest_fac_2) / yrest_fac;
    tmp_b = (tmp_b + yrest_fac_2) / yrest_fac;
    *pixout++ = PIX_RGB16_FROM_BYTES (tmp_r, tmp_g, tmp_b);
    pixin += 3 * factor;
    }
  if (xrest)
    {
    tmp_r = tmp_g = tmp_b = 0;
    for (j = 0; j < yrest; j++)
      for (i = 0; i < xrest; i++)
        {
	tmp_r += pixin[3*i + j*wrapin_bytes];
	tmp_g += pixin[3*i + j*wrapin_bytes + 1];
	tmp_b += pixin[3*i + j*wrapin_bytes + 2];
	}
    tmp_r = (tmp_r + yrest_xrest_2) / yrest_xrest;
    tmp_g = (tmp_g + yrest_xrest_2) / yrest_xrest;
    tmp_b = (tmp_b + yrest_xrest_2) / yrest_xrest;
    *pixout = PIX_RGB16_FROM_BYTES (tmp_r, tmp_g, tmp_b);
    }
  } }

													   /*---------------------------------------------------------------------------*/

E 81
I 28 static void rop_zoom_out_rgb_rgbm(RASTER *rin, RASTER *rout,
D 31 int x1, int y1, int x2, int y2, int newx, int newy, int factor)
E 31
I 31 int x1,
													   int y1, int x2, int y2, int newx, int newy, int abs_zoom_level)
E 31 {
D 48
printf ("rop_zoom_out_rgb_rgbm not implemented yet\n");
E 48
I 48
UCHAR  *rowin, *pixin, *in;
LPIXEL *rowout, *pixout, valout;
int tmp_r, tmp_g, tmp_b;
int wrapin_pixels, wrapin_bytes, wrapout;
int x, y, lx, ly, xlast, ylast, xrest, yrest, i, j;
int factor, fac_fac_2_bits;
int fac_fac  , yrest_fac,   fac_xrest,   yrest_xrest;
int fac_fac_2, yrest_fac_2, fac_xrest_2, yrest_xrest_2;
int fac_fac_4;

lx=x2-x1+1;
ly=y2-y1+1;
factor = 1 << abs_zoom_level;
D 61
xrest = lx % factor;
yrest = ly % factor;
E 61
I 61
xrest = lx & (factor - 1);
yrest = ly & (factor - 1);
E 61
xlast = x2 - xrest + 1;
ylast = y2 - yrest + 1;
fac_fac     = factor * factor; fac_fac_2     = fac_fac     >> 1;
fac_fac_4   = fac_fac >> 2;    fac_fac_2_bits= 2 * abs_zoom_level - 1;
yrest_fac   = yrest  * factor; yrest_fac_2   = yrest_fac   >> 1;
fac_xrest   = factor * xrest;  fac_xrest_2   = fac_xrest   >> 1;
yrest_xrest = yrest  * xrest;  yrest_xrest_2 = yrest_xrest >> 1;
wrapin_pixels = rin->wrap;
wrapin_bytes  = wrapin_pixels * 3;
wrapout = rout->wrap;
valout.m = 0xff;

rowin  = (UCHAR *)rin->buffer + wrapin_bytes*y1 + 3*x1;
rowout = (LPIXEL*)rout->buffer + wrapout*newy + newx;
for (y = y1; y < ylast; y += factor)
  {
  pixin  = rowin;
  pixout = rowout;
  for (x = x1; x < xlast; x += factor)
    {
    tmp_r = tmp_g = tmp_b = 0;
    in = pixin;
    for (j = 0; j < factor; j+=2)
      {
      for (i = 0; i < factor; i+=2)
        {
        tmp_r += *in++;
        tmp_g += *in++;
        tmp_b += *in++;
	in += wrapin_bytes;
        tmp_r += *in++;
        tmp_g += *in++;
        tmp_b += *in++;
	in -= wrapin_bytes;
	}
      in += 6 * wrapin_pixels - 3 * factor;
      }
    valout.r = (tmp_r + fac_fac_4) >> fac_fac_2_bits;
    valout.g = (tmp_g + fac_fac_4) >> fac_fac_2_bits;
    valout.b = (tmp_b + fac_fac_4) >> fac_fac_2_bits;
    *pixout++ = valout;
    pixin += 3 * factor;
    }
  if (xrest)
    {
    tmp_r = tmp_g = tmp_b = 0;
    for (j = 0; j < factor; j++)
      for (i = 0; i < xrest; i++)
        {
	tmp_r += pixin[3*i + j*wrapin_bytes];
	tmp_g += pixin[3*i + j*wrapin_bytes + 1];
	tmp_b += pixin[3*i + j*wrapin_bytes + 2];
	}
    valout.r = (tmp_r + fac_xrest_2) / fac_xrest;
    valout.g = (tmp_g + fac_xrest_2) / fac_xrest;
    valout.b = (tmp_b + fac_xrest_2) / fac_xrest;
    *pixout = valout;
    }
  rowin  += wrapin_bytes * factor;
  rowout += wrapout;
  }
if (yrest)
  {
  pixin  = rowin;
  pixout = rowout;
  for (x = x1; x < xlast; x += factor)
    {
    tmp_r = tmp_g = tmp_b = 0;
    for (j = 0; j < yrest; j++)
      for (i = 0; i < factor; i++)
        {
	tmp_r += pixin[3*i + j*wrapin_bytes];
	tmp_g += pixin[3*i + j*wrapin_bytes + 1];
	tmp_b += pixin[3*i + j*wrapin_bytes + 2];
	}
    valout.r = (tmp_r + yrest_fac_2) / yrest_fac;
    valout.g = (tmp_g + yrest_fac_2) / yrest_fac;
    valout.b = (tmp_b + yrest_fac_2) / yrest_fac;
    *pixout++ = valout;
D 49
    pixin += factor;
E 49
I 49
    pixin += 3 * factor;
E 49
    }
  if (xrest)
    {
    tmp_r = tmp_g = tmp_b = 0;
    for (j = 0; j < yrest; j++)
      for (i = 0; i < xrest; i++)
        {
	tmp_r += pixin[3*i + j*wrapin_bytes];
	tmp_g += pixin[3*i + j*wrapin_bytes + 1];
	tmp_b += pixin[3*i + j*wrapin_bytes + 2];
	}
    valout.r = (tmp_r + yrest_xrest_2) / yrest_xrest;
    valout.g = (tmp_g + yrest_xrest_2) / yrest_xrest;
    valout.b = (tmp_b + yrest_xrest_2) / yrest_xrest;
    *pixout = valout;
    }
  }
E 48 }

													/*---------------------------------------------------------------------------*/

I 68
D 69 static void rop_copy_bw_bw(RASTER *rin, RASTER *rout, int x1, int y1, int x2, int y2, int newx, int newy)
E 69
I 69 static void rop_copy_bw(RASTER *rin, RASTER *rout, int x1, int y1, int x2, int y2, int newx, int newy)
E 69 {
UINT mask;
D 69
int yin, yout, byte, byte1, byte2, newbyte, bytewrapin, bytewrapout;
E 69
I 69
int yin, yout, byte, byte1, byte2, newbyte, bytewrapin, bytewrapout, rest;
E 69
UCHAR *bytein, *byteout;

D 69
if ((x1 & 7) || ((x2 + 1) & 7))
E 69
I 69
if ((x1 & 7) || (newx & 7))
E 69
  {
D 69
  msg (MSG_IE, "rop_copy_bw_bw: x coordinates must be multiples of 8");
E 69
I 69
  rop_copy_90_bw (rin, rout, x1, y1, x2, y2, newx, newy, FALSE, 0);
E 69
  return;
  }
mask = rin->type == rout->type ? 0x0 : 0xff;
byte1 = x1 >> 3;
byte2 = ((x2 + 1) >> 3) - 1;
I 69
rest  = (x2 + 1) & 7;
E 69
newbyte = newx >> 3;
bytewrapin  = (rin->wrap  + 7) >> 3;
bytewrapout = (rout->wrap + 7) >> 3;
for (yin = y1, yout = newy; yin <= y2; yin++, yout++)
  {
  bytein  = (UCHAR *)rin->buffer  + yin  * bytewrapin  + byte1;
  byteout = (UCHAR *)rout->buffer + yout * bytewrapout + newbyte;
  if (mask)
    for (byte = byte1; byte <= byte2; byte++)
      *byteout++ = *bytein++ ^ mask;
  else
    for (byte = byte1; byte <= byte2; byte++)
      *byteout++ = *bytein++;
I 69
  if (rest)
    *byteout |= (*bytein ^ mask) & (~0U << (7 - rest));
E 69
  } }

													/*---------------------------------------------------------------------------*/

E 68
E 28
E 20
E 18 static void rop_copy_bw_cm16(RASTER *rin, RASTER *rout, int x1, int y1, int x2, int y2, int newx, int newy) {
UCHAR  *rowin, *bytein;
USHORT *rowout, *pixout;
LPIXEL *cmap;
I 5
D 21
int i_0, i_1, i_t;
E 21
I 21
USHORT i_0, i_1, i_t;
E 21
E 5
D 18
int wrapin, wrapout, tmp, cmap_offset, bit, bit_offs, startbit;
E 18
I 18
int wrapin, wrapout, cmap_offset, bit, bit_offs, startbit;
E 18
int x, lx, ly;
D 5
int reversed;
E 5

D 68
if(!rout->cmap.buffer)
E 68
I 68
D 84
if ( !rout->cmap.buffer)
E 68
  {
D 68
   msg (MSG_IE, "rop_copy_bw_cm16: missing color map");
   return;
E 68
I 68
  msg (MSG_IE, "rop_copy_bw_cm16: missing color map");
  return;
E 68
  }
E 84
cmap=rout->cmap.buffer;
D 56
cmap_offset=rout->cmap.offset;
E 56
I 56
cmap_offset=rout->cmap.info.offset_mask;
E 56
D 5
switch (rin->type)
E 5
I 5
if (cmap[0].r==0)
E 5
  {
D 5
  CASE RAS_BW: reversed = cmap[0].r!=0;
  CASE RAS_WB: reversed = cmap[0].r==0;
  DEFAULT: msg (MSG_IE, "rop_copy_bw_cm16: invalid rastertype");
E 5
I 5
  i_0 = cmap_offset;
  i_1 = cmap_offset + 255;
E 5
  }
I 5
else
  {
  i_0 = cmap_offset + 255;
  i_1 = cmap_offset;
  }
if (rin->type==RAS_WB)
  {
  i_t = i_0;
  i_0 = i_1;
  i_1 = i_t;
  }
E 5
lx=x2-x1+1;
ly=y2-y1+1;
D 8
wrapin  = rin->wrap;
wrapout = rout->wrap;
E 8
I 8
D 10
if ((rin->wrap % sizeof(UCHAR)) || (rout->wrap % sizeof(USHORT)))
  {
   printf("### INTERNAL ERROR - rop_copy_bw_cm16; bad wrap\n");
   return;
  }
wrapin  = rin->wrap  / sizeof(UCHAR);
wrapout = rout->wrap / sizeof(USHORT);
E 8
bit_offs = rin->bit_offs;
E 10
I 10
D 16
wrapin   = rin ->wrap / 8;
E 16
I 16
D 47
wrapin   = (rin->wrap + 7) / 8;
E 47
I 47
wrapin   = (rin->wrap + 7) >> 3;
E 47
E 16
bit_offs = rin ->bit_offs;
wrapout  = rout->wrap;
E 10

rowin = (UCHAR*)rin->buffer + wrapin*y1 + ((x1 + bit_offs)>>3);
startbit = 7 - ((x1 + bit_offs) & 7);
rowout = (USHORT*)rout->buffer + wrapout*newy + newx;
while (ly-- > 0)
  {
  bytein = rowin;
  bit    = startbit;
  pixout = rowout;
D 5
  if (reversed)
    for (x = lx; x > 0; x--)
      {
      *pixout++ = cmap_offset + 1 - ((*bytein >> bit) & 1);
      if (bit==0)
        {
        bytein++;
        bit = 7;
        }
      else
        bit--;
      }
  else
    for (x = lx; x > 0; x--)
E 5
I 5
  for (x = lx; x > 0; x--)
    {
    *pixout++ = ((*bytein >> bit) & 1) ? i_1 : i_0;
    if (bit==0)
E 5
      {
D 5
      *pixout++ = cmap_offset + ((*bytein >> bit) & 1);
      if (bit==0)
        {
        bytein++;
        bit = 7;
        }
      else
        bit--;
E 5
I 5
      bytein++;
      bit = 7;
E 5
      }
I 5
    else
      bit--;
    }
E 5
  rowin  += wrapin;
  rowout += wrapout;
  } }

													/*---------------------------------------------------------------------------*/

I 20
D 84 static void rop_reduce_bw_cm16(RASTER *rin, RASTER *rout, int x1, int y1, int x2, int y2, int newx, int newy, int factor)
E 84
I 84 static void rop_copy_bw_cm24(RASTER *rin, RASTER *rout, int x1, int y1, int x2, int y2, int newx, int newy)
E 84 {
D 84
printf ("rop_reduce_bw_cm16 not implemented yet\n");
E 84
I 84
UCHAR  *rowin, *bytein;
ULONG  *rowout, *pixout;
LPIXEL *penmap, *colmap;
ULONG  i_0, i_1, i_t;
int wrapin, wrapout, bit, bit_offs, startbit;
int x, lx, ly;

penmap=rout->cmap.penbuffer;
colmap=rout->cmap.colbuffer;
if (penmap[0].r + colmap[0].r==0)
  {
  i_0 = 0;
  i_1 = 255;
  }
else
  {
  i_0 = 255;
  i_1 = 0;
  }
if (rin->type==RAS_WB)
  {
  i_t = i_0;
  i_0 = i_1;
  i_1 = i_t;
  }
lx=x2-x1+1;
ly=y2-y1+1;
wrapin   = (rin->wrap + 7) >> 3;
bit_offs = rin ->bit_offs;
wrapout  = rout->wrap;

rowin = (UCHAR*)rin->buffer + wrapin*y1 + ((x1 + bit_offs)>>3);
startbit = 7 - ((x1 + bit_offs) & 7);
rowout = (ULONG*)rout->buffer + wrapout*newy + newx;
while (ly-- > 0)
  {
  bytein = rowin;
  bit    = startbit;
  pixout = rowout;
  for (x = lx; x > 0; x--)
    {
    *pixout++ = ((*bytein >> bit) & 1) ? i_1 : i_0;
    if (bit==0)
      {
      bytein++;
      bit = 7;
      }
    else
      bit--;
    }
  rowin  += wrapin;
  rowout += wrapout;
  }
I 89
if (rout->extra_mask)
  rop_copy_extra (rin, rout, x1, y1, x2, y2, newx, newy);
E 89
E 84 }

													/*---------------------------------------------------------------------------*/

I 28 static void rop_zoom_out_bw_cm16(RASTER *rin, RASTER *rout,
D 31 int x1, int y1, int x2, int y2, int newx, int newy, int factor)
E 31
I 31 int x1,
													int y1, int x2, int y2, int newx, int newy, int abs_zoom_level)
E 31 { printf("rop_zoom_out_bw_cm16 not implemented yet\n"); }

									   /*---------------------------------------------------------------------------*/

I 84 static void rop_zoom_out_bw_cm24(RASTER *rin, RASTER *rout, int x1, int y1, int x2, int y2, int newx, int newy, int abs_zoom_level) { printf("rop_zoom_out_bw_cm24 not implemented yet\n"); }

									   /*---------------------------------------------------------------------------*/

E 84
E 28
E 20
I 19 static void rop_copy_bw_rgbm(RASTER *rin, RASTER *rout, int x1, int y1, int x2, int y2, int newx, int newy) {
UCHAR  *rowin, *bytein;
LPIXEL *rowout, *pixout;
LPIXEL rgb_0, rgb_1;
int wrapin, wrapout, bit, bit_offs, startbit;
int x, lx, ly;

if (rin->type==RAS_WB)
  {
  rgb_0.r = 0xff;  rgb_0.g = 0xff;  rgb_0.b = 0xff;  rgb_0.m = 0xff;
  rgb_1.r = 0x00;  rgb_1.g = 0x00;  rgb_1.b = 0x00;  rgb_1.m = 0xff;
  }
else
  {
  rgb_0.r = 0x00;  rgb_0.g = 0x00;  rgb_0.b = 0x00;  rgb_0.m = 0xff;
  rgb_1.r = 0xff;  rgb_1.g = 0xff;  rgb_1.b = 0xff;  rgb_1.m = 0xff;
  }
lx=x2-x1+1;
ly=y2-y1+1;
D 47
wrapin   = (rin->wrap + 7) / 8;
E 47
I 47
wrapin   = (rin->wrap + 7) >> 3;
E 47
bit_offs = rin ->bit_offs;
wrapout  = rout->wrap;

rowin = (UCHAR*)rin->buffer + wrapin*y1 + ((x1 + bit_offs)>>3);
startbit = 7 - ((x1 + bit_offs) & 7);
rowout = (LPIXEL*)rout->buffer + wrapout*newy + newx;
while (ly-- > 0)
  {
  bytein = rowin;
  bit    = startbit;
  pixout = rowout;
  for (x = lx; x > 0; x--)
    {
    *pixout++ = ((*bytein >> bit) & 1) ? rgb_1 : rgb_0;
    if (bit==0)
      {
      bytein++;
      bit = 7;
      }
    else
      bit--;
    }
  rowin  += wrapin;
  rowout += wrapout;
  } }

									   /*---------------------------------------------------------------------------*/

I 20
D 84 static void rop_reduce_bw_rgbm(RASTER *rin, RASTER *rout, int x1, int y1, int x2, int y2, int newx, int newy, int factor) { printf("rop_reduce_bw_rgbm not implemented yet\n"); }

									   /*---------------------------------------------------------------------------*/

E 84
I 28 static void rop_zoom_out_bw_rgbm(RASTER *rin, RASTER *rout,
D 31 int x1, int y1, int x2, int y2, int newx, int newy, int factor)
E 31
I 31 int x1,
									   int y1, int x2, int y2, int newx, int newy, int abs_zoom_level)
E 31
{
  D 61 printf("rop_zoom_out_bw_rgbm not implemented yet\n");
  E 61
I 61 UCHAR *rowin, *bytein, *in;
  LPIXEL *rowout, *pixout, valout;
  int val_0, val_1, tmp;
  int wrapin, wrapout, bitin, bit, bit_offs, startbit;
  int x, y, lx, ly, xlast, ylast, xrest, yrest, i, j;
  int factor, fac_fac_2_bits;
  int fac_fac, yrest_fac, fac_xrest, yrest_xrest;
  int fac_fac_2, yrest_fac_2, fac_xrest_2, yrest_xrest_2;
  int fac_fac_4;

  if (rin->type == RAS_WB) {
    val_0 = 0xff;
    val_1 = 0x00;
  } else {
    val_0 = 0x00;
    val_1 = 0xff;
  }
  lx             = x2 - x1 + 1;
  ly             = y2 - y1 + 1;
  factor         = 1 << abs_zoom_level;
  xrest          = lx & (factor - 1);
  yrest          = ly & (factor - 1);
  xlast          = x2 - xrest + 1;
  ylast          = y2 - yrest + 1;
  fac_fac        = factor * factor;
  fac_fac_2      = fac_fac >> 1;
  fac_fac_4      = fac_fac >> 2;
  fac_fac_2_bits = 2 * abs_zoom_level - 1;
  yrest_fac      = yrest * factor;
  yrest_fac_2    = yrest_fac >> 1;
  fac_xrest      = factor * xrest;
  fac_xrest_2    = fac_xrest >> 1;
  yrest_xrest    = yrest * xrest;
  yrest_xrest_2  = yrest_xrest >> 1;
  wrapin         = (rin->wrap + 7) >> 3;
  bit_offs       = rin->bit_offs;
  wrapout        = rout->wrap;
  valout.m       = 0xff;

  rowin    = (UCHAR *)rin->buffer + wrapin * y1 + ((x1 + bit_offs) >> 3);
  startbit = 7 - ((x1 + bit_offs) & 7);
  rowout   = (LPIXEL *)rout->buffer + wrapout * newy + newx;
  for (y = y1; y < ylast; y += factor) {
    bytein = rowin;
    bitin  = startbit;
    pixout = rowout;
    for (x = x1; x < xlast; x += factor) {
      tmp = 0;
      in  = bytein;
      bit = bitin;
      for (j = 0; j < factor; j += 2) {
        for (i = 0; i < factor; i += 2) {
          tmp += ((*in >> bit) & 1);
          in += wrapin;
          if (bit == 0) {
            in++;
            bit = 7;
          } else
            bit--;
          tmp += ((*in >> bit) & 1);
          in -= wrapin;
          if (bit == 0) {
            in++;
            bit = 7;
          } else
            bit--;
        }
        in += 2 * wrapin;
        bit += factor;
        while (bit > 7) {
          in--;
          bit -= 8;
        }
      }
      tmp      = tmp * val_1 + (fac_fac_2 - tmp) * val_0;
      valout.r = valout.g = valout.b = (tmp + fac_fac_4) >> fac_fac_2_bits;
      *pixout++                      = valout;
      bitin -= factor;
      while (bitin < 0) {
        bytein++;
        bitin += 8;
      }
    }
    if (xrest) {
      tmp = 0;
      for (j = 0; j < factor; j++)
        for (i = 0; i < xrest; i++) {
          tmp += GET_BIT(i, j, bytein, wrapin, 7 - bitin);
        }
      tmp      = tmp * val_1 + (fac_xrest - tmp) * val_0;
      valout.r = valout.g = valout.b = (tmp + fac_xrest_2) / fac_xrest;
      *pixout                        = valout;
    }
    rowin += wrapin * factor;
    rowout += wrapout;
  }
  if (yrest) {
    bytein = rowin;
    bitin  = startbit;
    pixout = rowout;
    for (x = x1; x < xlast; x += factor) {
      tmp = 0;
      for (j = 0; j < yrest; j++)
        for (i = 0; i < factor; i++) {
          tmp += GET_BIT(i, j, bytein, wrapin, 7 - bitin);
        }
      tmp      = tmp * val_1 + (yrest_fac - tmp) * val_0;
      valout.r = valout.g = valout.b = (tmp + yrest_fac_2) / yrest_fac;
      *pixout++                      = valout;
      bitin -= factor;
      while (bitin < 0) {
        bytein++;
        bitin += 8;
      }
    }
    if (xrest) {
      tmp = 0;
      for (j = 0; j < yrest; j++)
        for (i = 0; i < xrest; i++) {
          tmp += GET_BIT(i, j, bytein, wrapin, 7 - bitin);
        }
      tmp      = tmp * val_1 + (yrest_xrest - tmp) * val_0;
      valout.r = valout.g = valout.b = (tmp + yrest_xrest_2) / yrest_xrest;
      *pixout                        = valout;
    }
  }
  E 61
}

/*---------------------------------------------------------------------------*/

E 28
E 20
E 19 static void rop_copy_gr8_cm16(RASTER *rin, RASTER *rout, int x1, int y1,
                                     int x2, int y2, int newx, int newy) {
  UCHAR *rowin, *bytein;
  USHORT *rowout, *pixout;
  LPIXEL *cmap;
  D 18 int wrapin, wrapout, tmp, cmap_offset;
  E 18
I 18 int wrapin, wrapout, cmap_offset;
  E 18 int x, lx, ly;
  int i, reversed;
  E 4

I 4
D 84 if (!rout->cmap.buffer) {
    msg(MSG_IE, "rop_copy_gr8_cm16: missing color map");
    return;
  }
  E 84 cmap        = rout->cmap.buffer;
  D 56 cmap_offset = rout->cmap.offset;
  E 56
I 56 cmap_offset  = rout->cmap.info.offset_mask;
  E 56 reversed    = cmap[0].r != 0;
  lx                = x2 - x1 + 1;
  ly                = y2 - y1 + 1;
  D 8 wrapin       = rin->wrap;
  wrapout           = rout->wrap;
  E 8
I 8
D 10 if ((rin->wrap % sizeof(UCHAR)) || (rout->wrap % sizeof(USHORT))) {
    printf("### INTERNAL ERROR - rop_copy_gr8_cm16; bad wrap\n");
    return;
  }
  wrapin      = rin->wrap / sizeof(UCHAR);
  wrapout     = rout->wrap / sizeof(USHORT);
  E 10
I 10 wrapin = rin->wrap;
  wrapout     = rout->wrap;
  E 10
E 8

      rowin = (UCHAR *)rin->buffer + wrapin * y1 + x1;
  rowout    = (USHORT *)rout->buffer + wrapout * newy + newx;
  while (ly-- > 0) {
    bytein = rowin;
    pixout = rowout;
    if (reversed)
      for (x = lx; x > 0; x--) *pixout++ = cmap_offset + (255 - *bytein++);
    else
      for (x = lx; x > 0; x--) *pixout++ = cmap_offset + *bytein++;
    rowin += wrapin;
    rowout += wrapout;
  }
}

E 4
    /*---------------------------------------------------------------------------*/

I 20
D 84 static void
rop_reduce_gr8_cm16(RASTER *rin, RASTER *rout, int x1, int y1, int x2, int y2,
                    int newx, int newy, int factor)
E 84
I 84 static void rop_copy_gr8_cm24(RASTER *rin, RASTER *rout, int x1, int y1,
                                     int x2, int y2, int newx, int newy)
E 84 {
  D 84 printf("rop_reduce_gr8_cm16 not implemented yet\n");
  E 84
I 84 UCHAR *rowin, *bytein;
  ULONG *rowout, *pixout;
  LPIXEL *penmap, *colmap;
  int wrapin, wrapout;
  int x, lx, ly;
  int i, reversed;

  penmap   = rout->cmap.penbuffer;
  colmap   = rout->cmap.colbuffer;
  reversed = penmap[0].r + colmap[0].r != 0;
  lx       = x2 - x1 + 1;
  ly       = y2 - y1 + 1;
  wrapin   = rin->wrap;
  wrapout  = rout->wrap;

  rowin  = (UCHAR *)rin->buffer + wrapin * y1 + x1;
  rowout = (ULONG *)rout->buffer + wrapout * newy + newx;
  while (ly-- > 0) {
    bytein = rowin;
    pixout = rowout;
    if (reversed)
      for (x = lx; x > 0; x--) *pixout++ = 255 - *bytein++;
    else
      for (x = lx; x > 0; x--) *pixout++ = *bytein++;
    rowin += wrapin;
    rowout += wrapout;
  }
  I 89 if (rout->extra_mask)
      rop_copy_extra(rin, rout, x1, y1, x2, y2, newx, newy);
  E 89
E 84
}

/*---------------------------------------------------------------------------*/

I 28
static void rop_zoom_out_gr8_cm16(RASTER *rin, RASTER *rout,
D 31
                                int x1, int y1, int x2, int y2,
			        int newx, int newy, int factor)
E 31
I 31
                                  int x1, int y1, int x2, int y2,
			          int newx, int newy, int abs_zoom_level)
E 31
{
  printf("rop_zoom_out_gr8_cm16 not implemented yet\n");
}

/*---------------------------------------------------------------------------*/

I 84 static void rop_zoom_out_gr8_cm24(RASTER *rin, RASTER *rout, int x1,
                                        int y1, int x2, int y2, int newx,
                                        int newy, int abs_zoom_level) {
  printf("rop_zoom_out_gr8_cm24 not implemented yet\n");
}

/*---------------------------------------------------------------------------*/

E 84
E 28
I 23 static void rop_copy_cm8_cm16(RASTER *rin, RASTER *rout, int x1, int y1,
                                     int x2, int y2, int newx, int newy) {
  UCHAR *rowin, *pixin;
  USHORT *rowout, *pixout;
  LPIXEL *cmapin, *cmapout;
  int wrapin, wrapout, cmapin_offset, cmapout_offset, cmap_offset_delta;
  int x, lx, ly;

  D 84 if (!rin->cmap.buffer || !rout->cmap.buffer) {
    msg(MSG_IE, "rop_copy_cm8_cm16: missing color map");
    return;
  }
  E 84 cmapin            = rin->cmap.buffer;
  cmapout                 = rout->cmap.buffer;
  D 56 cmapin_offset     = rin->cmap.offset;
  cmapout_offset          = rout->cmap.offset;
  E 56
I 56 cmapin_offset      = rin->cmap.info.offset_mask;
  cmapout_offset          = rout->cmap.info.offset_mask;
  E 56 cmap_offset_delta = cmapout_offset - cmapin_offset;
  lx                      = x2 - x1 + 1;
  ly                      = y2 - y1 + 1;
  wrapin                  = rin->wrap;
  wrapout                 = rout->wrap;

  rowin  = (UCHAR *)rin->buffer + wrapin * y1 + x1;
  rowout = (USHORT *)rout->buffer + wrapout * newy + newx;
  while (ly-- > 0) {
    pixin  = rowin;
    pixout = rowout;
    for (x = lx; x > 0; x--) *pixout++ = *pixin++ + cmap_offset_delta;
    rowin += wrapin;
    rowout += wrapout;
  }
}

/*---------------------------------------------------------------------------*/

E 23
D 84 static void rop_reduce_bw_gr8(
    RASTER *rin,
    RASTER *rout,
E 84
I 84 static void rop_copy_cm8_cm24(
        RASTER *rin, RASTER *rout,
E 84 int x1, int y1, int x2, int y2,
D 84 int newx, int newy,
        int factor) { printf("rop_reduce_bw_gr8 not implemented yet\n"); }

    /*---------------------------------------------------------------------------*/

I 28 static void rop_zoom_out_bw_gr8(RASTER *rin, RASTER *rout,
D 31 int x1, int y1, int x2, int y2, int newx, int newy, int factor)
E 31
I 31 int x1,
    int y1, int x2, int y2, int newx, int newy, int abs_zoom_level)
E 31 {
  printf("rop_zoom_out_bw_gr8 not implemented yet\n");
}

/*---------------------------------------------------------------------------*/

E 28
static void rop_reduce_gr8(RASTER *rin, RASTER *rout,
                           int x1, int y1, int x2, int y2,
			   int newx, int newy, int factor)
E 84
I 84
			      int newx, int newy)
E 84
{
  D 84 UCHAR *rowin, *bytein;
  UCHAR *rowout, *byteout;
  int tmp;
  int wrapin, wrapout;
  int x, y, lx, ly, xlast, ylast, xrest, yrest, i, j;
  E 84
I 84 UCHAR *rowin, *pixin;
  ULONG *rowout, *pixout;
  int wrapin, wrapout, cmapin_offset, cmap_offset_delta;
  int x, lx, ly;
  E 84

I 84 cmapin_offset = rin->cmap.info.offset_mask;
  cmap_offset_delta  = -cmapin_offset;
  E 84 lx           = x2 - x1 + 1;
  ly                 = y2 - y1 + 1;
  D 61 xrest        = lx % factor;
  yrest              = ly % factor;
  E 61
I 61
D 84 xrest         = lx & (factor - 1);
  yrest              = ly & (factor - 1);
  E 61
D 28 xlast         = x2 - xrest - (factor - 1);
  ylast              = y2 - yrest - (factor - 1);
  E 28
I 28 xlast         = x2 - xrest + 1;
  ylast              = y2 - yrest + 1;
  E 84
E 28 wrapin        = rin->wrap;
  wrapout            = rout->wrap;

  D 84 rowin = (UCHAR *)rin->buffer + wrapin * y1 + x1;
  rowout      = (UCHAR *)rout->buffer + wrapout * newy + newx;
  D 28 for (y = y1; y <= ylast; y += factor)
E 28
I 28 for (y = y1; y < ylast; y += factor)
E 84
I 84 rowin = (UCHAR *)rin->buffer + wrapin * y1 + x1;
  rowout     = (ULONG *)rout->buffer + wrapout * newy + newx;
  while (ly-- > 0)
		E 84
E 28 {
      D 84 bytein = rowin;
      byteout      = rowout;
      for (x = x1; x < xlast; x += factor) {
        tmp = 0;
        for (j = 0; j < factor; j++)
          for (i   = 0; i < factor; i++) tmp += bytein[i + j * wrapin];
        *byteout++ = tmp / (factor * factor);
        bytein += factor;
      }
      if (xrest) {
        tmp = 0;
        for (j = 0; j < factor; j++)
          for (i = 0; i < xrest; i++) tmp += bytein[i + j * wrapin];
        *byteout = tmp / (factor * xrest);
      }
      rowin += wrapin * factor;
      E 84
I 84 pixin = rowin;
      pixout = rowout;
      for (x = lx; x > 0; x--) *pixout++ = *pixin++ + cmap_offset_delta;
      rowin += wrapin;
      E 84 rowout += wrapout;
    }
  I 89 if (rout->extra_mask)
      rop_copy_extra(rin, rout, x1, y1, x2, y2, newx, newy);
  E 89
D 84 if (yrest) {
    bytein  = rowin;
    byteout = rowout;
    for (x = x1; x < xlast; x += factor) {
      tmp = 0;
      for (j = 0; j < yrest; j++)
        for (i   = 0; i < factor; i++) tmp += bytein[i + j * wrapin];
      *byteout++ = tmp / (yrest * factor);
      bytein += factor;
    }
    if (xrest) {
      tmp = 0;
      for (j = 0; j < yrest; j++)
        for (i = 0; i < xrest; i++) tmp += bytein[i + j * wrapin];
      *byteout = tmp / (yrest * xrest);
    }
  }
  E 84
}

/*---------------------------------------------------------------------------*/

I 28
D 84
static void rop_zoom_out_gr8(RASTER *rin, RASTER *rout,
D 31
                              int x1, int y1, int x2, int y2,
			      int newx, int newy, int factor)
E 31
I 31
                             int x1, int y1, int x2, int y2,
			     int newx, int newy, int abs_zoom_level)
E 84
I 84
static void rop_zoom_out_bw_gr8(RASTER *rin, RASTER *rout,
                                int x1, int y1, int x2, int y2,
			        int newx, int newy, int abs_zoom_level)
E 84
E 31
{
  D 84 printf("rop_zoom_out_gr8 not implemented yet\n");
  E 84
I 84 printf("rop_zoom_out_bw_gr8 not implemented yet\n");
  E 84
}

/*---------------------------------------------------------------------------*/

E 28
D 84 static void rop_reduce_cm16(RASTER *rin, RASTER *rout, int x1, int y1,
                                   int x2, int y2, int newx, int newy,
                                   int factor)
E 84
I 84 static void rop_zoom_out_gr8(RASTER *rin, RASTER *rout, int x1, int y1,
                                    int x2, int y2, int newx, int newy,
                                    int abs_zoom_level)
E 84 {
  D 84 printf("rop_reduce_cm16 not implemented yet\n");
  E 84
I 84 printf("rop_zoom_out_gr8 not implemented yet\n");
  E 84
}

/*---------------------------------------------------------------------------*/

I 28
static void rop_zoom_out_cm16(RASTER *rin, RASTER *rout,
D 31
                            int x1, int y1, int x2, int y2,
			    int newx, int newy, int factor)
E 31
I 31
                              int x1, int y1, int x2, int y2,
			      int newx, int newy, int abs_zoom_level)
E 31
{
  printf("rop_zoom_out_cm16 not implemented yet\n");
}

/*---------------------------------------------------------------------------*/

E 28
D 84 static void rop_reduce_rgbm(RASTER *rin, RASTER *rout, int x1, int y1,
                                   int x2, int y2, int newx, int newy,
                                   int factor)
E 84
I 84 static void rop_zoom_out_cm24(RASTER *rin, RASTER *rout, int x1, int y1,
                                     int x2, int y2, int newx, int newy,
                                     int abs_zoom_level)
E 84 {
  D 84 printf("rop_reduce_rgbm not implemented yet\n");
  E 84
I 84 printf("rop_zoom_out_cm24 not implemented yet\n");
  E 84
}

/*---------------------------------------------------------------------------*/

I 28
static void rop_zoom_out_rgbm(RASTER *rin, RASTER *rout,
D 31
                            int x1, int y1, int x2, int y2,
			    int newx, int newy, int factor)
E 31
I 31
                              int x1, int y1, int x2, int y2,
			      int newx, int newy, int abs_zoom_level)
E 31
{
  D 31 printf("rop_zoom_out_rgbm not implemented yet\n");
  E 31
I 31 LPIXEL *rowin, *pixin, *in, valin;
  LPIXEL *rowout, *pixout, valout;
  int tmp_r, tmp_g, tmp_b, tmp_m;
  int wrapin, wrapout;
  int x, y, lx, ly, xlast, ylast, xrest, yrest, i, j;
  int factor, fac_fac_2_bits;
  int fac_fac, yrest_fac, fac_xrest, yrest_xrest;
  int fac_fac_2, yrest_fac_2, fac_xrest_2, yrest_xrest_2;
  int fac_fac_4;

  lx             = x2 - x1 + 1;
  ly             = y2 - y1 + 1;
  factor         = 1 << abs_zoom_level;
  D 61 xrest    = lx % factor;
  yrest          = ly % factor;
  E 61
I 61 xrest     = lx & (factor - 1);
  yrest          = ly & (factor - 1);
  E 61 xlast    = x2 - xrest + 1;
  ylast          = y2 - yrest + 1;
  fac_fac        = factor * factor;
  fac_fac_2      = fac_fac >> 1;
  fac_fac_4      = fac_fac >> 2;
  fac_fac_2_bits = 2 * abs_zoom_level - 1;
  yrest_fac      = yrest * factor;
  yrest_fac_2    = yrest_fac >> 1;
  fac_xrest      = factor * xrest;
  fac_xrest_2    = fac_xrest >> 1;
  yrest_xrest    = yrest * xrest;
  yrest_xrest_2  = yrest_xrest >> 1;
  wrapin         = rin->wrap;
  wrapout        = rout->wrap;
  valout.m       = 0xff;

  rowin  = (LPIXEL *)rin->buffer + wrapin * y1 + x1;
  rowout = (LPIXEL *)rout->buffer + wrapout * newy + newx;
  for (y = y1; y < ylast; y += factor) {
    pixin  = rowin;
    pixout = rowout;
    for (x = x1; x < xlast; x += factor) {
      tmp_r = tmp_g = tmp_b = tmp_m = 0;
      in                            = pixin;
      for (j = 0; j < factor; j += 2) {
        for (i = 0; i < factor; i += 2) {
          valin = *in;
          tmp_r += valin.r;
          tmp_g += valin.g;
          tmp_b += valin.b;
          tmp_m += valin.m;
          D 38 in += factor;
          E 38
I 38 in += wrapin;
          E 38 in++;
          valin = *in;
          tmp_r += valin.r;
          tmp_g += valin.g;
          tmp_b += valin.b;
          tmp_m += valin.m;
          D 38 in -= factor;
          E 38
I 38 in -= wrapin;
          E 38 in++;
        }
        in += 2 * wrapin - factor;
      }
      valout.r  = (tmp_r + fac_fac_4) >> fac_fac_2_bits;
      valout.g  = (tmp_g + fac_fac_4) >> fac_fac_2_bits;
      valout.b  = (tmp_b + fac_fac_4) >> fac_fac_2_bits;
      valout.m  = (tmp_m + fac_fac_4) >> fac_fac_2_bits;
      *pixout++ = valout;
      pixin += factor;
    }
    if (xrest) {
      tmp_r = tmp_g = tmp_b = tmp_m = 0;
      for (j = 0; j < factor; j++)
        for (i = 0; i < xrest; i++) {
          valin = pixin[i + j * wrapin];
          tmp_r += valin.r;
          tmp_g += valin.g;
          tmp_b += valin.b;
          tmp_m += valin.m;
        }
      valout.r = (tmp_r + fac_xrest_2) / fac_xrest;
      valout.g = (tmp_g + fac_xrest_2) / fac_xrest;
      valout.b = (tmp_b + fac_xrest_2) / fac_xrest;
      valout.m = (tmp_m + fac_xrest_2) / fac_xrest;
      *pixout  = valout;
    }
    rowin += wrapin * factor;
    rowout += wrapout;
  }
  if (yrest) {
    pixin  = rowin;
    pixout = rowout;
    for (x = x1; x < xlast; x += factor) {
      tmp_r = tmp_g = tmp_b = tmp_m = 0;
      for (j = 0; j < yrest; j++)
        for (i = 0; i < factor; i++) {
          valin = pixin[i + j * wrapin];
          tmp_r += valin.r;
          tmp_g += valin.g;
          tmp_b += valin.b;
          tmp_m += valin.m;
        }
      valout.r  = (tmp_r + yrest_fac_2) / yrest_fac;
      valout.g  = (tmp_g + yrest_fac_2) / yrest_fac;
      valout.b  = (tmp_b + yrest_fac_2) / yrest_fac;
      valout.m  = (tmp_m + yrest_fac_2) / yrest_fac;
      *pixout++ = valout;
      pixin += factor;
    }
    if (xrest) {
      tmp_r = tmp_g = tmp_b = tmp_m = 0;
      for (j = 0; j < yrest; j++)
        for (i = 0; i < xrest; i++) {
          valin = pixin[i + j * wrapin];
          tmp_r += valin.r;
          tmp_g += valin.g;
          tmp_b += valin.b;
          tmp_m += valin.m;
        }
      valout.r = (tmp_r + yrest_xrest_2) / yrest_xrest;
      valout.g = (tmp_g + yrest_xrest_2) / yrest_xrest;
      valout.b = (tmp_b + yrest_xrest_2) / yrest_xrest;
      valout.m = (tmp_m + yrest_xrest_2) / yrest_xrest;
      *pixout  = valout;
    }
  }
  E 31
}

/*---------------------------------------------------------------------------*/

I 46 static void rop_zoom_out_rgbx(RASTER *rin, RASTER *rout, int x1, int y1,
                                    int x2, int y2, int newx, int newy,
                                    int abs_zoom_level) {
  LPIXEL *rowin, *pixin, *in, valin;
  LPIXEL *rowout, *pixout, valout;
  int tmp_r, tmp_g, tmp_b;
  int wrapin, wrapout;
  int x, y, lx, ly, xlast, ylast, xrest, yrest, i, j;
  int factor, fac_fac_2_bits;
  int fac_fac, yrest_fac, fac_xrest, yrest_xrest;
  int fac_fac_2, yrest_fac_2, fac_xrest_2, yrest_xrest_2;
  int fac_fac_4;

  lx             = x2 - x1 + 1;
  ly             = y2 - y1 + 1;
  factor         = 1 << abs_zoom_level;
  D 61 xrest    = lx % factor;
  yrest          = ly % factor;
  E 61
I 61 xrest     = lx & (factor - 1);
  yrest          = ly & (factor - 1);
  E 61 xlast    = x2 - xrest + 1;
  ylast          = y2 - yrest + 1;
  fac_fac        = factor * factor;
  fac_fac_2      = fac_fac >> 1;
  fac_fac_4      = fac_fac >> 2;
  fac_fac_2_bits = 2 * abs_zoom_level - 1;
  yrest_fac      = yrest * factor;
  yrest_fac_2    = yrest_fac >> 1;
  fac_xrest      = factor * xrest;
  fac_xrest_2    = fac_xrest >> 1;
  yrest_xrest    = yrest * xrest;
  yrest_xrest_2  = yrest_xrest >> 1;
  wrapin         = rin->wrap;
  wrapout        = rout->wrap;
  valout.m       = 0xff;

  rowin  = (LPIXEL *)rin->buffer + wrapin * y1 + x1;
  rowout = (LPIXEL *)rout->buffer + wrapout * newy + newx;
  for (y = y1; y < ylast; y += factor) {
    pixin  = rowin;
    pixout = rowout;
    for (x = x1; x < xlast; x += factor) {
      tmp_r = tmp_g = tmp_b = 0;
      in                    = pixin;
      for (j = 0; j < factor; j += 2) {
        for (i = 0; i < factor; i += 2) {
          valin = *in;
          tmp_r += valin.r;
          tmp_g += valin.g;
          tmp_b += valin.b;
          in += wrapin;
          in++;
          valin = *in;
          tmp_r += valin.r;
          tmp_g += valin.g;
          tmp_b += valin.b;
          in -= wrapin;
          in++;
        }
        in += 2 * wrapin - factor;
      }
      valout.r  = (tmp_r + fac_fac_4) >> fac_fac_2_bits;
      valout.g  = (tmp_g + fac_fac_4) >> fac_fac_2_bits;
      valout.b  = (tmp_b + fac_fac_4) >> fac_fac_2_bits;
      *pixout++ = valout;
      pixin += factor;
    }
    if (xrest) {
      tmp_r = tmp_g = tmp_b = 0;
      for (j = 0; j < factor; j++)
        for (i = 0; i < xrest; i++) {
          valin = pixin[i + j * wrapin];
          tmp_r += valin.r;
          tmp_g += valin.g;
          tmp_b += valin.b;
        }
      valout.r = (tmp_r + fac_xrest_2) / fac_xrest;
      valout.g = (tmp_g + fac_xrest_2) / fac_xrest;
      valout.b = (tmp_b + fac_xrest_2) / fac_xrest;
      *pixout  = valout;
    }
    rowin += wrapin * factor;
    rowout += wrapout;
  }
  if (yrest) {
    pixin  = rowin;
    pixout = rowout;
    for (x = x1; x < xlast; x += factor) {
      tmp_r = tmp_g = tmp_b = 0;
      for (j = 0; j < yrest; j++)
        for (i = 0; i < factor; i++) {
          valin = pixin[i + j * wrapin];
          tmp_r += valin.r;
          tmp_g += valin.g;
          tmp_b += valin.b;
        }
      valout.r  = (tmp_r + yrest_fac_2) / yrest_fac;
      valout.g  = (tmp_g + yrest_fac_2) / yrest_fac;
      valout.b  = (tmp_b + yrest_fac_2) / yrest_fac;
      *pixout++ = valout;
      pixin += factor;
    }
    if (xrest) {
      tmp_r = tmp_g = tmp_b = 0;
      for (j = 0; j < yrest; j++)
        for (i = 0; i < xrest; i++) {
          valin = pixin[i + j * wrapin];
          tmp_r += valin.r;
          tmp_g += valin.g;
          tmp_b += valin.b;
        }
      valout.r = (tmp_r + yrest_xrest_2) / yrest_xrest;
      valout.g = (tmp_g + yrest_xrest_2) / yrest_xrest;
      valout.b = (tmp_b + yrest_xrest_2) / yrest_xrest;
      *pixout  = valout;
    }
  }
}
I 95
    /*---------------------------------------------------------------------------*/

    static void
    rop_zoom_out_rgbx64_rgbx(RASTER *rin, RASTER *rout, int x1, int y1, int x2,
                             int y2, int newx, int newy, int abs_zoom_level) {
  SPIXEL *rowin, *pixin, *in, valin;
  LPIXEL *rowout, *pixout, valout;
  int tmp_r, tmp_g, tmp_b;
  int wrapin, wrapout;
  int x, y, lx, ly, xlast, ylast, xrest, yrest, i, j;
  int factor, fac_fac_2_bits;
  int fac_fac, yrest_fac, fac_xrest, yrest_xrest;
  int fac_fac_2, yrest_fac_2, fac_xrest_2, yrest_xrest_2;
  int fac_fac_4;

  lx             = x2 - x1 + 1;
  ly             = y2 - y1 + 1;
  factor         = 1 << abs_zoom_level;
  xrest          = lx & (factor - 1);
  yrest          = ly & (factor - 1);
  xlast          = x2 - xrest + 1;
  ylast          = y2 - yrest + 1;
  fac_fac        = factor * factor;
  fac_fac_2      = fac_fac >> 1;
  fac_fac_4      = fac_fac >> 2;
  fac_fac_2_bits = 2 * abs_zoom_level - 1;
  yrest_fac      = yrest * factor;
  yrest_fac_2    = yrest_fac >> 1;
  fac_xrest      = factor * xrest;
  fac_xrest_2    = fac_xrest >> 1;
  yrest_xrest    = yrest * xrest;
  yrest_xrest_2  = yrest_xrest >> 1;

  wrapin   = rin->wrap;
  wrapout  = rout->wrap;
  valout.m = 0xff;

  rowin  = (SPIXEL *)rin->buffer + wrapin * y1 + x1;
  rowout = (LPIXEL *)rout->buffer + wrapout * newy + newx;
  for (y = y1; y < ylast; y += factor) {
    pixin  = rowin;
    pixout = rowout;
    for (x = x1; x < xlast; x += factor) {
      tmp_r = tmp_g = tmp_b = 0;
      in                    = pixin;
      for (j = 0; j < factor; j += 2) {
        for (i = 0; i < factor; i += 2) {
          valin = *in;
          tmp_r += valin.r;
          tmp_g += valin.g;
          tmp_b += valin.b;
          in += wrapin;
          in++;
          valin = *in;
          tmp_r += valin.r;
          tmp_g += valin.g;
          tmp_b += valin.b;
          in -= wrapin;
          in++;
        }
        in += 2 * wrapin - factor;
      }
      valout.r  = PIX_BYTE_FROM_USHORT((tmp_r + fac_fac_4) >> fac_fac_2_bits);
      valout.g  = PIX_BYTE_FROM_USHORT((tmp_g + fac_fac_4) >> fac_fac_2_bits);
      valout.b  = PIX_BYTE_FROM_USHORT((tmp_b + fac_fac_4) >> fac_fac_2_bits);
      *pixout++ = valout;
      pixin += factor;
    }
    if (xrest) {
      tmp_r = tmp_g = tmp_b = 0;
      for (j = 0; j < factor; j++)
        for (i = 0; i < xrest; i++) {
          valin = pixin[i + j * wrapin];
          tmp_r += valin.r;
          tmp_g += valin.g;
          tmp_b += valin.b;
        }
      valout.r = PIX_BYTE_FROM_USHORT((tmp_r + fac_xrest_2) / fac_xrest);
      valout.g = PIX_BYTE_FROM_USHORT((tmp_g + fac_xrest_2) / fac_xrest);
      valout.b = PIX_BYTE_FROM_USHORT((tmp_b + fac_xrest_2) / fac_xrest);
      *pixout  = valout;
    }
    rowin += wrapin * factor;
    rowout += wrapout;
  }
  if (yrest) {
    pixin  = rowin;
    pixout = rowout;
    for (x = x1; x < xlast; x += factor) {
      tmp_r = tmp_g = tmp_b = 0;
      for (j = 0; j < yrest; j++)
        for (i = 0; i < factor; i++) {
          valin = pixin[i + j * wrapin];
          tmp_r += valin.r;
          tmp_g += valin.g;
          tmp_b += valin.b;
        }
      valout.r  = PIX_BYTE_FROM_USHORT((tmp_r + yrest_fac_2) / yrest_fac);
      valout.g  = PIX_BYTE_FROM_USHORT((tmp_g + yrest_fac_2) / yrest_fac);
      valout.b  = PIX_BYTE_FROM_USHORT((tmp_b + yrest_fac_2) / yrest_fac);
      *pixout++ = valout;
      pixin += factor;
    }
    if (xrest) {
      tmp_r = tmp_g = tmp_b = 0;
      for (j = 0; j < yrest; j++)
        for (i = 0; i < xrest; i++) {
          valin = pixin[i + j * wrapin];
          tmp_r += valin.r;
          tmp_g += valin.g;
          tmp_b += valin.b;
        }
      valout.r = PIX_BYTE_FROM_USHORT((tmp_r + yrest_xrest_2) / yrest_xrest);
      valout.g = PIX_BYTE_FROM_USHORT((tmp_g + yrest_xrest_2) / yrest_xrest);
      valout.b = PIX_BYTE_FROM_USHORT((tmp_b + yrest_xrest_2) / yrest_xrest);
      *pixout  = valout;
    }
  }
}
/*---------------------------------------------------------------------------*/

static void rop_zoom_out_rgbm64_rgbm(RASTER *rin, RASTER *rout, int x1, int y1,
                                     int x2, int y2, int newx, int newy,
                                     int abs_zoom_level) {
  SPIXEL *rowin, *pixin, *in, valin;
  LPIXEL *rowout, *pixout, valout;
  int tmp_r, tmp_g, tmp_b, tmp_m;
  int wrapin, wrapout;
  int x, y, lx, ly, xlast, ylast, xrest, yrest, i, j;
  int factor, fac_fac_2_bits;
  int fac_fac, yrest_fac, fac_xrest, yrest_xrest;
  int fac_fac_2, yrest_fac_2, fac_xrest_2, yrest_xrest_2;
  int fac_fac_4;

  lx             = x2 - x1 + 1;
  ly             = y2 - y1 + 1;
  factor         = 1 << abs_zoom_level;
  xrest          = lx & (factor - 1);
  yrest          = ly & (factor - 1);
  xlast          = x2 - xrest + 1;
  ylast          = y2 - yrest + 1;
  fac_fac        = factor * factor;
  fac_fac_2      = fac_fac >> 1;
  fac_fac_4      = fac_fac >> 2;
  fac_fac_2_bits = 2 * abs_zoom_level - 1;
  yrest_fac      = yrest * factor;
  yrest_fac_2    = yrest_fac >> 1;
  fac_xrest      = factor * xrest;
  fac_xrest_2    = fac_xrest >> 1;
  yrest_xrest    = yrest * xrest;
  yrest_xrest_2  = yrest_xrest >> 1;

  wrapin     = rin->wrap;
  wrapout    = rout->wrap;
  E 95

I 95 rowin = (SPIXEL *)rin->buffer + wrapin * y1 + x1;
  rowout     = (LPIXEL *)rout->buffer + wrapout * newy + newx;
  for (y = y1; y < ylast; y += factor) {
    pixin  = rowin;
    pixout = rowout;
    for (x = x1; x < xlast; x += factor) {
      tmp_r = tmp_g = tmp_b = tmp_m = 0;
      in                            = pixin;
      for (j = 0; j < factor; j += 2) {
        for (i = 0; i < factor; i += 2) {
          valin = *in;
          tmp_r += valin.r;
          tmp_g += valin.g;
          tmp_b += valin.b;
          tmp_m += valin.m;
          in += wrapin;
          in++;
          valin = *in;
          tmp_r += valin.r;
          tmp_g += valin.g;
          tmp_b += valin.b;
          tmp_m += valin.m;
          in -= wrapin;
          in++;
        }
        in += 2 * wrapin - factor;
      }
      valout.r  = PIX_BYTE_FROM_USHORT((tmp_r + fac_fac_4) >> fac_fac_2_bits);
      valout.g  = PIX_BYTE_FROM_USHORT((tmp_g + fac_fac_4) >> fac_fac_2_bits);
      valout.b  = PIX_BYTE_FROM_USHORT((tmp_b + fac_fac_4) >> fac_fac_2_bits);
      valout.m  = PIX_BYTE_FROM_USHORT((tmp_m + fac_fac_4) >> fac_fac_2_bits);
      *pixout++ = valout;
      pixin += factor;
    }
    if (xrest) {
      tmp_r = tmp_g = tmp_b = tmp_m = 0;
      for (j = 0; j < factor; j++)
        for (i = 0; i < xrest; i++) {
          valin = pixin[i + j * wrapin];
          tmp_r += valin.r;
          tmp_g += valin.g;
          tmp_b += valin.b;
          tmp_m += valin.m;
        }
      valout.r = PIX_BYTE_FROM_USHORT((tmp_r + fac_xrest_2) / fac_xrest);
      valout.g = PIX_BYTE_FROM_USHORT((tmp_g + fac_xrest_2) / fac_xrest);
      valout.b = PIX_BYTE_FROM_USHORT((tmp_b + fac_xrest_2) / fac_xrest);
      valout.m = PIX_BYTE_FROM_USHORT((tmp_m + fac_xrest_2) / fac_xrest);
      *pixout  = valout;
    }
    rowin += wrapin * factor;
    rowout += wrapout;
  }
  if (yrest) {
    pixin  = rowin;
    pixout = rowout;
    for (x = x1; x < xlast; x += factor) {
      tmp_r = tmp_g = tmp_b = tmp_m = 0;
      for (j = 0; j < yrest; j++)
        for (i = 0; i < factor; i++) {
          valin = pixin[i + j * wrapin];
          tmp_r += valin.r;
          tmp_g += valin.g;
          tmp_b += valin.b;
          tmp_m += valin.m;
        }
      valout.r  = PIX_BYTE_FROM_USHORT((tmp_r + yrest_fac_2) / yrest_fac);
      valout.g  = PIX_BYTE_FROM_USHORT((tmp_g + yrest_fac_2) / yrest_fac);
      valout.b  = PIX_BYTE_FROM_USHORT((tmp_b + yrest_fac_2) / yrest_fac);
      valout.m  = PIX_BYTE_FROM_USHORT((tmp_m + yrest_fac_2) / yrest_fac);
      *pixout++ = valout;
      pixin += factor;
    }
    if (xrest) {
      tmp_r = tmp_g = tmp_b = tmp_m = 0;
      for (j = 0; j < yrest; j++)
        for (i = 0; i < xrest; i++) {
          valin = pixin[i + j * wrapin];
          tmp_r += valin.r;
          tmp_g += valin.g;
          tmp_b += valin.b;
          tmp_m += valin.m;
        }
      valout.r = PIX_BYTE_FROM_USHORT((tmp_r + yrest_xrest_2) / yrest_xrest);
      valout.g = PIX_BYTE_FROM_USHORT((tmp_g + yrest_xrest_2) / yrest_xrest);
      valout.b = PIX_BYTE_FROM_USHORT((tmp_b + yrest_xrest_2) / yrest_xrest);
      valout.m = PIX_BYTE_FROM_USHORT((tmp_m + yrest_xrest_2) / yrest_xrest);
      *pixout  = valout;
    }
  }
}
E 95
    /*---------------------------------------------------------------------------*/

I 54 static void
rop_copy_bw_rgb16(RASTER *rin, RASTER *rout, int x1, int y1, int x2, int y2,
                  int newx, int newy) {
  UCHAR *rowin, *bytein;
  USHORT *rowout, *pixout;
  USHORT rgb_0, rgb_1;
  int wrapin, wrapout, bit, bit_offs, startbit;
  int x, lx, ly;

  if (rin->type == RAS_WB) {
    rgb_0 = 0xffff;
    rgb_1 = 0x0000;
  } else {
    rgb_0 = 0x0000;
    rgb_1 = 0xffff;
  }
  lx       = x2 - x1 + 1;
  ly       = y2 - y1 + 1;
  wrapin   = (rin->wrap + 7) >> 3;
  bit_offs = rin->bit_offs;
  wrapout  = rout->wrap;

  rowin    = (UCHAR *)rin->buffer + wrapin * y1 + ((x1 + bit_offs) >> 3);
  startbit = 7 - ((x1 + bit_offs) & 7);
  rowout   = (USHORT *)rout->buffer + wrapout * newy + newx;
  while (ly-- > 0) {
    bytein = rowin;
    bit    = startbit;
    pixout = rowout;
    for (x = lx; x > 0; x--) {
      *pixout++ = ((*bytein >> bit) & 1) ? rgb_1 : rgb_0;
      if (bit == 0) {
        bytein++;
        bit = 7;
      } else
        bit--;
    }
    rowin += wrapin;
    rowout += wrapout;
  }
}

/*---------------------------------------------------------------------------*/

I 61 static void rop_zoom_out_bw_rgb16(RASTER *rin, RASTER *rout, int x1,
                                        int y1, int x2, int y2, int newx,
                                        int newy, int abs_zoom_level) {
  UCHAR *rowin, *bytein, *in;
  USHORT *rowout, *pixout;
  int val_0, val_1, tmp;
  int wrapin, wrapout, bitin, bit, bit_offs, startbit;
  int x, y, lx, ly, xlast, ylast, xrest, yrest, i, j;
  int factor, fac_fac_2_bits;
  int fac_fac, yrest_fac, fac_xrest, yrest_xrest;
  int fac_fac_2, yrest_fac_2, fac_xrest_2, yrest_xrest_2;
  int fac_fac_4;

  if (rin->type == RAS_WB) {
    val_0 = 0xff;
    val_1 = 0x00;
  } else {
    val_0 = 0x00;
    val_1 = 0xff;
  }
  lx             = x2 - x1 + 1;
  ly             = y2 - y1 + 1;
  factor         = 1 << abs_zoom_level;
  xrest          = lx & (factor - 1);
  yrest          = ly & (factor - 1);
  xlast          = x2 - xrest + 1;
  ylast          = y2 - yrest + 1;
  fac_fac        = factor * factor;
  fac_fac_2      = fac_fac >> 1;
  fac_fac_4      = fac_fac >> 2;
  fac_fac_2_bits = 2 * abs_zoom_level - 1;
  yrest_fac      = yrest * factor;
  yrest_fac_2    = yrest_fac >> 1;
  fac_xrest      = factor * xrest;
  fac_xrest_2    = fac_xrest >> 1;
  yrest_xrest    = yrest * xrest;
  yrest_xrest_2  = yrest_xrest >> 1;
  wrapin         = (rin->wrap + 7) >> 3;
  bit_offs       = rin->bit_offs;
  wrapout        = rout->wrap;

  rowin    = (UCHAR *)rin->buffer + wrapin * y1 + ((x1 + bit_offs) >> 3);
  startbit = 7 - ((x1 + bit_offs) & 7);
  rowout   = (USHORT *)rout->buffer + wrapout * newy + newx;
  for (y = y1; y < ylast; y += factor) {
    bytein = rowin;
    bitin  = startbit;
    pixout = rowout;
    for (x = x1; x < xlast; x += factor) {
      tmp = 0;
      in  = bytein;
      bit = bitin;
      for (j = 0; j < factor; j += 2) {
        for (i = 0; i < factor; i += 2) {
          tmp += ((*in >> bit) & 1);
          in += wrapin;
          if (bit == 0) {
            in++;
            bit = 7;
          } else
            bit--;
          tmp += ((*in >> bit) & 1);
          in -= wrapin;
          if (bit == 0) {
            in++;
            bit = 7;
          } else
            bit--;
        }
        in += 2 * wrapin;
        bit += factor;
        while (bit > 7) {
          in--;
          bit -= 8;
        }
      }
      tmp       = tmp * val_1 + (fac_fac_2 - tmp) * val_0;
      tmp       = (tmp + fac_fac_4) >> fac_fac_2_bits;
      *pixout++ = PIX_RGB16_FROM_BYTES(tmp, tmp, tmp);
      bitin -= factor;
      while (bitin < 0) {
        bytein++;
        bitin += 8;
      }
    }
    if (xrest) {
      tmp = 0;
      for (j = 0; j < factor; j++)
        for (i = 0; i < xrest; i++) {
          tmp += GET_BIT(i, j, bytein, wrapin, 7 - bitin);
        }
      tmp     = tmp * val_1 + (fac_xrest - tmp) * val_0;
      tmp     = (tmp + fac_xrest_2) / fac_xrest;
      *pixout = PIX_RGB16_FROM_BYTES(tmp, tmp, tmp);
    }
    rowin += wrapin * factor;
    rowout += wrapout;
  }
  if (yrest) {
    bytein = rowin;
    bitin  = startbit;
    pixout = rowout;
    for (x = x1; x < xlast; x += factor) {
      tmp = 0;
      for (j = 0; j < yrest; j++)
        for (i = 0; i < factor; i++) {
          tmp += GET_BIT(i, j, bytein, wrapin, 7 - bitin);
        }
      tmp       = tmp * val_1 + (yrest_fac - tmp) * val_0;
      tmp       = (tmp + yrest_fac_2) / yrest_fac;
      *pixout++ = PIX_RGB16_FROM_BYTES(tmp, tmp, tmp);
      bitin -= factor;
      while (bitin < 0) {
        bytein++;
        bitin += 8;
      }
    }
    if (xrest) {
      tmp = 0;
      for (j = 0; j < yrest; j++)
        for (i = 0; i < xrest; i++) {
          tmp += GET_BIT(i, j, bytein, wrapin, 7 - bitin);
        }
      tmp     = tmp * val_1 + (yrest_xrest - tmp) * val_0;
      tmp     = (tmp + yrest_xrest_2) / yrest_xrest;
      *pixout = PIX_RGB16_FROM_BYTES(tmp, tmp, tmp);
    }
  }
}

/*---------------------------------------------------------------------------*/

E 61 static void rop_copy_gr8_rgb16(RASTER *rin, RASTER *rout, int x1, int y1,
                                     int x2, int y2, int newx, int newy) {
  UCHAR *rowin, *pixin, val;
  USHORT *rowout, *pixout;
  int wrapin, wrapout;
  int x, y, lx, ly;

  lx      = x2 - x1 + 1;
  ly      = y2 - y1 + 1;
  wrapin  = rin->wrap;
  wrapout = rout->wrap;

  rowin  = (UCHAR *)rin->buffer + wrapin * y1 + x1;
  rowout = (USHORT *)rout->buffer + wrapout * newy + newx;
  while (ly-- > 0) {
    pixin  = rowin;
    pixout = rowout;
    for (x = 0; x < lx; x++) {
      val             = *pixin++;
      D 60 *pixout++ = ROP_RGB16_FROM_BYTES(val, val, val);
      E 60
I 60 *pixout++      = PIX_RGB16_FROM_BYTES(val, val, val);
      E 60
    }
    rowin += wrapin;
    rowout += wrapout;
  }
}

/*---------------------------------------------------------------------------*/

static void rop_zoom_out_gr8_rgb16(RASTER *rin, RASTER *rout, int x1, int y1,
                                   int x2, int y2, int newx, int newy,
                                   int abs_zoom_level) {
  UCHAR *rowin, *pixin, *in;
  USHORT *rowout, *pixout;
  int tmp;
  int wrapin, wrapout;
  int x, y, lx, ly, xlast, ylast, xrest, yrest, i, j;
  int factor, fac_fac_2_bits;
  int fac_fac, yrest_fac, fac_xrest, yrest_xrest;
  int fac_fac_2, yrest_fac_2, fac_xrest_2, yrest_xrest_2;
  int fac_fac_4;

  lx             = x2 - x1 + 1;
  ly             = y2 - y1 + 1;
  factor         = 1 << abs_zoom_level;
  D 61 xrest    = lx % factor;
  yrest          = ly % factor;
  E 61
I 61 xrest     = lx & (factor - 1);
  yrest          = ly & (factor - 1);
  E 61 xlast    = x2 - xrest + 1;
  ylast          = y2 - yrest + 1;
  fac_fac        = factor * factor;
  fac_fac_2      = fac_fac >> 1;
  fac_fac_4      = fac_fac >> 2;
  fac_fac_2_bits = 2 * abs_zoom_level - 1;
  yrest_fac      = yrest * factor;
  yrest_fac_2    = yrest_fac >> 1;
  fac_xrest      = factor * xrest;
  fac_xrest_2    = fac_xrest >> 1;
  yrest_xrest    = yrest * xrest;
  yrest_xrest_2  = yrest_xrest >> 1;
  wrapin         = rin->wrap;
  wrapout        = rout->wrap;

  rowin  = (UCHAR *)rin->buffer + wrapin * y1 + x1;
  rowout = (USHORT *)rout->buffer + wrapout * newy + newx;
  for (y = y1; y < ylast; y += factor) {
    pixin  = rowin;
    pixout = rowout;
    for (x = x1; x < xlast; x += factor) {
      tmp = 0;
      in  = pixin;
      for (j = 0; j < factor; j += 2) {
        for (i = 0; i < factor; i += 2) {
          tmp += *in;
          in += wrapin;
          in++;
          tmp += *in;
          in -= wrapin;
          in++;
        }
        in += 2 * wrapin - factor;
      }
      tmp             = (tmp + fac_fac_4) >> fac_fac_2_bits;
      D 60 *pixout++ = ROP_RGB16_FROM_BYTES(tmp, tmp, tmp);
      E 60
I 60 *pixout++      = PIX_RGB16_FROM_BYTES(tmp, tmp, tmp);
      E 60 pixin += factor;
    }
    if (xrest) {
      tmp = 0;
      for (j = 0; j < factor; j++)
        for (i = 0; i < xrest; i++) {
          tmp += pixin[i + j * wrapin];
        }
      tmp           = (tmp + fac_xrest_2) / fac_xrest;
      D 60 *pixout = ROP_RGB16_FROM_BYTES(tmp, tmp, tmp);
      E 60
I 60 *pixout      = PIX_RGB16_FROM_BYTES(tmp, tmp, tmp);
      E 60
    }
    rowin += wrapin * factor;
    rowout += wrapout;
  }
  if (yrest) {
    pixin  = rowin;
    pixout = rowout;
    for (x = x1; x < xlast; x += factor) {
      tmp = 0;
      for (j = 0; j < yrest; j++)
        for (i = 0; i < factor; i++) {
          tmp += pixin[i + j * wrapin];
        }
      tmp             = (tmp + yrest_fac_2) / yrest_fac;
      D 60 *pixout++ = ROP_RGB16_FROM_BYTES(tmp, tmp, tmp);
      E 60
I 60 *pixout++      = PIX_RGB16_FROM_BYTES(tmp, tmp, tmp);
      E 60 pixin += factor;
    }
    if (xrest) {
      tmp = 0;
      for (j = 0; j < yrest; j++)
        for (i = 0; i < xrest; i++) {
          tmp += pixin[i + j * wrapin];
        }
      tmp           = (tmp + yrest_xrest_2) / yrest_xrest;
      D 60 *pixout = ROP_RGB16_FROM_BYTES(tmp, tmp, tmp);
      E 60
I 60 *pixout      = PIX_RGB16_FROM_BYTES(tmp, tmp, tmp);
      E 60
    }
  }
}

/*---------------------------------------------------------------------------*/

E 54
I 52 static void rop_copy_cm16_rgb16(RASTER *rin, RASTER *rout, int x1,
                                       int y1, int x2, int y2, int newx,
                                       int newy) {
  USHORT *rowin, *pixin;
  LPIXEL *cmap, val;
  USHORT *rowout, *pixout;
  int wrapin, wrapout;
  int x, y, lx, ly;

  D 56 cmap = rin->cmap.buffer - rin->cmap.offset;
  E 56
I 56 cmap  = rin->cmap.buffer - rin->cmap.info.offset_mask;
  E 56 lx   = x2 - x1 + 1;
  ly         = y2 - y1 + 1;
  wrapin     = rin->wrap;
  wrapout    = rout->wrap;

  rowin  = (USHORT *)rin->buffer + wrapin * y1 + x1;
  rowout = (USHORT *)rout->buffer + wrapout * newy + newx;
  while (ly-- > 0) {
    pixin  = rowin;
    pixout = rowout;
    for (x = 0; x < lx; x++) {
      val             = cmap[*pixin++];
      D 54 *pixout++ = BYTES_TO_RGB16(val.r, val.g, val.b);
      E 54
I 54
D 60 *pixout++      = ROP_RGB16_FROM_BYTES(val.r, val.g, val.b);
      E 60
I 60 *pixout++      = PIX_RGB16_FROM_BYTES(val.r, val.g, val.b);
      E 60
E 54
    }
    rowin += wrapin;
    rowout += wrapout;
  }
}

/*---------------------------------------------------------------------------*/

I 96 static void rop_copy_cm16_xrgb1555(RASTER *rin, RASTER *rout, int x1,
                                         int y1, int x2, int y2, int newx,
                                         int newy) {
  USHORT *rowin, *pixin;
  LPIXEL *cmap, val;
  USHORT *rowout, *pixout;
  int wrapin, wrapout;
  int x, y, lx, ly;

  cmap    = rin->cmap.buffer - rin->cmap.info.offset_mask;
  lx      = x2 - x1 + 1;
  ly      = y2 - y1 + 1;
  wrapin  = rin->wrap;
  wrapout = rout->wrap;

  rowin  = (USHORT *)rin->buffer + wrapin * y1 + x1;
  rowout = (USHORT *)rout->buffer + wrapout * newy + newx;
  while (ly-- > 0) {
    pixin  = rowin;
    pixout = rowout;
    for (x = 0; x < lx; x++) {
      val       = cmap[*pixin++];
      *pixout++ = PIX_XRGB1555_FROM_BYTES(val.r, val.g, val.b);
    }
    rowin += wrapin;
    rowout += wrapout;
  }
}

/*---------------------------------------------------------------------------*/

E 96
I 84 static void rop_copy_cm24_rgb16(RASTER *rin, RASTER *rout, int x1,
                                       int y1, int x2, int y2, int newx,
                                       int newy) {
  ULONG *rowin, *pixin, win;
  LPIXEL *penmap, *colmap, val;
  USHORT *rowout, *pixout;
  int wrapin, wrapout;
  int x, y, lx, ly;

  penmap  = rin->cmap.penbuffer;
  colmap  = rin->cmap.colbuffer;
  lx      = x2 - x1 + 1;
  ly      = y2 - y1 + 1;
  wrapin  = rin->wrap;
  wrapout = rout->wrap;

  rowin  = (ULONG *)rin->buffer + wrapin * y1 + x1;
  rowout = (USHORT *)rout->buffer + wrapout * newy + newx;
  I 89

E 89 while (ly-- > 0) {
    pixin  = rowin;
    pixout = rowout;
    for (x = 0; x < lx; x++) {
      win = *pixin++;
      MAP24(win, penmap, colmap, val)
      *pixout++ = PIX_RGB16_FROM_BYTES(val.r, val.g, val.b);
    }
    rowin += wrapin;
    rowout += wrapout;
  }
  I 89 if (rout->extra_mask)
      rop_copy_extra(rin, rout, x1, y1, x2, y2, newx, newy);
  E 89
}

/*---------------------------------------------------------------------------*/

I 96 static void rop_copy_cm24_xrgb1555(RASTER *rin, RASTER *rout, int x1,
                                         int y1, int x2, int y2, int newx,
                                         int newy) {
  ULONG *rowin, *pixin, win;
  LPIXEL *penmap, *colmap, val;
  USHORT *rowout, *pixout;
  int wrapin, wrapout;
  int x, y, lx, ly;

  penmap  = rin->cmap.penbuffer;
  colmap  = rin->cmap.colbuffer;
  lx      = x2 - x1 + 1;
  ly      = y2 - y1 + 1;
  wrapin  = rin->wrap;
  wrapout = rout->wrap;

  rowin  = (ULONG *)rin->buffer + wrapin * y1 + x1;
  rowout = (USHORT *)rout->buffer + wrapout * newy + newx;

  while (ly-- > 0) {
    pixin  = rowin;
    pixout = rowout;
    for (x = 0; x < lx; x++) {
      win = *pixin++;
      MAP24(win, penmap, colmap, val)
      *pixout++ = PIX_XRGB1555_FROM_BYTES(val.r, val.g, val.b);
    }
    rowin += wrapin;
    rowout += wrapout;
  }
  if (rout->extra_mask) rop_copy_extra(rin, rout, x1, y1, x2, y2, newx, newy);
}

/*---------------------------------------------------------------------------*/

E 96
E 84 static void rop_zoom_out_cm16_rgb16(RASTER *rin, RASTER *rout, int x1,
                                           int y1, int x2, int y2, int newx,
                                           int newy, int abs_zoom_level) {
  USHORT *rowin, *pixin, *in;
  LPIXEL *cmap, valin;
  USHORT *rowout, *pixout;
  int tmp_r, tmp_g, tmp_b;
  int wrapin, wrapout;
  int x, y, lx, ly, xlast, ylast, xrest, yrest, i, j;
  int factor, fac_fac_2_bits;
  int fac_fac, yrest_fac, fac_xrest, yrest_xrest;
  int fac_fac_2, yrest_fac_2, fac_xrest_2, yrest_xrest_2;
  int fac_fac_4;

  D 56 cmap     = rin->cmap.buffer - rin->cmap.offset;
  E 56
I 56 cmap      = rin->cmap.buffer - rin->cmap.info.offset_mask;
  E 56 lx       = x2 - x1 + 1;
  ly             = y2 - y1 + 1;
  factor         = 1 << abs_zoom_level;
  D 61 xrest    = lx % factor;
  yrest          = ly % factor;
  E 61
I 61 xrest     = lx & (factor - 1);
  yrest          = ly & (factor - 1);
  E 61 xlast    = x2 - xrest + 1;
  ylast          = y2 - yrest + 1;
  fac_fac        = factor * factor;
  fac_fac_2      = fac_fac >> 1;
  fac_fac_4      = fac_fac >> 2;
  fac_fac_2_bits = 2 * abs_zoom_level - 1;
  yrest_fac      = yrest * factor;
  yrest_fac_2    = yrest_fac >> 1;
  fac_xrest      = factor * xrest;
  fac_xrest_2    = fac_xrest >> 1;
  yrest_xrest    = yrest * xrest;
  yrest_xrest_2  = yrest_xrest >> 1;
  wrapin         = rin->wrap;
  wrapout        = rout->wrap;

  rowin  = (USHORT *)rin->buffer + wrapin * y1 + x1;
  rowout = (USHORT *)rout->buffer + wrapout * newy + newx;
  for (y = y1; y < ylast; y += factor) {
    pixin  = rowin;
    pixout = rowout;
    for (x = x1; x < xlast; x += factor) {
      tmp_r = tmp_g = tmp_b = 0;
      in                    = pixin;
      for (j = 0; j < factor; j += 2) {
        for (i = 0; i < factor; i += 2) {
          valin = cmap[*in];
          tmp_r += valin.r;
          tmp_g += valin.g;
          tmp_b += valin.b;
          in += wrapin;
          in++;
          valin = cmap[*in];
          tmp_r += valin.r;
          tmp_g += valin.g;
          tmp_b += valin.b;
          in -= wrapin;
          in++;
        }
        in += 2 * wrapin - factor;
      }
      tmp_r           = (tmp_r + fac_fac_4) >> fac_fac_2_bits;
      tmp_g           = (tmp_g + fac_fac_4) >> fac_fac_2_bits;
      tmp_b           = (tmp_b + fac_fac_4) >> fac_fac_2_bits;
      D 54 *pixout++ = BYTES_TO_RGB16(tmp_r, tmp_g, tmp_b);
      E 54
I 54
D 60 *pixout++      = ROP_RGB16_FROM_BYTES(tmp_r, tmp_g, tmp_b);
      E 60
I 60 *pixout++      = PIX_RGB16_FROM_BYTES(tmp_r, tmp_g, tmp_b);
      E 60
E 54 pixin += factor;
    }
    if (xrest) {
      tmp_r = tmp_g = tmp_b = 0;
      for (j = 0; j < factor; j++)
        for (i = 0; i < xrest; i++) {
          valin = cmap[pixin[i + j * wrapin]];
          tmp_r += valin.r;
          tmp_g += valin.g;
          tmp_b += valin.b;
        }
      tmp_r         = (tmp_r + fac_xrest_2) / fac_xrest;
      tmp_g         = (tmp_g + fac_xrest_2) / fac_xrest;
      tmp_b         = (tmp_b + fac_xrest_2) / fac_xrest;
      D 54 *pixout = BYTES_TO_RGB16(tmp_r, tmp_g, tmp_b);
      E 54
I 54
D 60 *pixout      = ROP_RGB16_FROM_BYTES(tmp_r, tmp_g, tmp_b);
      E 60
I 60 *pixout      = PIX_RGB16_FROM_BYTES(tmp_r, tmp_g, tmp_b);
      E 60
E 54
    }
    rowin += wrapin * factor;
    rowout += wrapout;
  }
  if (yrest) {
    pixin  = rowin;
    pixout = rowout;
    for (x = x1; x < xlast; x += factor) {
      tmp_r = tmp_g = tmp_b = 0;
      for (j = 0; j < yrest; j++)
        for (i = 0; i < factor; i++) {
          valin = cmap[pixin[i + j * wrapin]];
          tmp_r += valin.r;
          tmp_g += valin.g;
          tmp_b += valin.b;
        }
      tmp_r           = (tmp_r + yrest_fac_2) / yrest_fac;
      tmp_g           = (tmp_g + yrest_fac_2) / yrest_fac;
      tmp_b           = (tmp_b + yrest_fac_2) / yrest_fac;
      D 54 *pixout++ = BYTES_TO_RGB16(tmp_r, tmp_g, tmp_b);
      E 54
I 54
D 60 *pixout++      = ROP_RGB16_FROM_BYTES(tmp_r, tmp_g, tmp_b);
      E 60
I 60 *pixout++      = PIX_RGB16_FROM_BYTES(tmp_r, tmp_g, tmp_b);
      E 60
E 54 pixin += factor;
    }
    if (xrest) {
      tmp_r = tmp_g = tmp_b = 0;
      for (j = 0; j < yrest; j++)
        for (i = 0; i < xrest; i++) {
          valin = cmap[pixin[i + j * wrapin]];
          tmp_r += valin.r;
          tmp_g += valin.g;
          tmp_b += valin.b;
        }
      tmp_r         = (tmp_r + yrest_xrest_2) / yrest_xrest;
      tmp_g         = (tmp_g + yrest_xrest_2) / yrest_xrest;
      tmp_b         = (tmp_b + yrest_xrest_2) / yrest_xrest;
      D 54 *pixout = BYTES_TO_RGB16(tmp_r, tmp_g, tmp_b);
      E 54
I 54
D 60 *pixout      = ROP_RGB16_FROM_BYTES(tmp_r, tmp_g, tmp_b);
      E 60
I 60 *pixout      = PIX_RGB16_FROM_BYTES(tmp_r, tmp_g, tmp_b);
      E 60
E 54
    }
  }
}

/*---------------------------------------------------------------------------*/

I 84 static void rop_zoom_out_cm24_rgb16(RASTER *rin, RASTER *rout, int x1,
                                          int y1, int x2, int y2, int newx,
                                          int newy, int abs_zoom_level) {
  ULONG *rowin, *pixin, *in, win;
  LPIXEL *penmap, *colmap, valin;
  USHORT *rowout, *pixout;
  int tmp_r, tmp_g, tmp_b;
  int wrapin, wrapout;
  int x, y, lx, ly, xlast, ylast, xrest, yrest, i, j;
  int factor, fac_fac_2_bits;
  int fac_fac, yrest_fac, fac_xrest, yrest_xrest;
  int fac_fac_2, yrest_fac_2, fac_xrest_2, yrest_xrest_2;
  int fac_fac_4;

  penmap         = rin->cmap.penbuffer;
  colmap         = rin->cmap.colbuffer;
  lx             = x2 - x1 + 1;
  ly             = y2 - y1 + 1;
  factor         = 1 << abs_zoom_level;
  xrest          = lx & (factor - 1);
  yrest          = ly & (factor - 1);
  xlast          = x2 - xrest + 1;
  ylast          = y2 - yrest + 1;
  fac_fac        = factor * factor;
  fac_fac_2      = fac_fac >> 1;
  fac_fac_4      = fac_fac >> 2;
  fac_fac_2_bits = 2 * abs_zoom_level - 1;
  yrest_fac      = yrest * factor;
  yrest_fac_2    = yrest_fac >> 1;
  fac_xrest      = factor * xrest;
  fac_xrest_2    = fac_xrest >> 1;
  yrest_xrest    = yrest * xrest;
  yrest_xrest_2  = yrest_xrest >> 1;
  wrapin         = rin->wrap;
  wrapout        = rout->wrap;

  rowin  = (ULONG *)rin->buffer + wrapin * y1 + x1;
  rowout = (USHORT *)rout->buffer + wrapout * newy + newx;
  for (y = y1; y < ylast; y += factor) {
    pixin  = rowin;
    pixout = rowout;
    for (x = x1; x < xlast; x += factor) {
      tmp_r = tmp_g = tmp_b = 0;
      in                    = pixin;
      for (j = 0; j < factor; j += 2) {
        for (i = 0; i < factor; i += 2) {
          win = *in;
          MAP24_64(win, penmap, colmap, valin)
          tmp_r += valin.r;
          tmp_g += valin.g;
          tmp_b += valin.b;
          in += wrapin;
          in++;
          win = *in;
          MAP24_64(win, penmap, colmap, valin)
          tmp_r += valin.r;
          tmp_g += valin.g;
          tmp_b += valin.b;
          in -= wrapin;
          in++;
        }
        in += 2 * wrapin - factor;
      }
      tmp_r     = (tmp_r + fac_fac_4) >> fac_fac_2_bits;
      tmp_g     = (tmp_g + fac_fac_4) >> fac_fac_2_bits;
      tmp_b     = (tmp_b + fac_fac_4) >> fac_fac_2_bits;
      *pixout++ = PIX_RGB16_FROM_BYTES(tmp_r, tmp_g, tmp_b);
      pixin += factor;
    }
    if (xrest) {
      tmp_r = tmp_g = tmp_b = 0;
      for (j = 0; j < factor; j++)
        for (i = 0; i < xrest; i++) {
          win = pixin[i + j * wrapin];
          MAP24_64(win, penmap, colmap, valin)
          tmp_r += valin.r;
          tmp_g += valin.g;
          tmp_b += valin.b;
        }
      tmp_r   = (tmp_r + fac_xrest_2) / fac_xrest;
      tmp_g   = (tmp_g + fac_xrest_2) / fac_xrest;
      tmp_b   = (tmp_b + fac_xrest_2) / fac_xrest;
      *pixout = PIX_RGB16_FROM_BYTES(tmp_r, tmp_g, tmp_b);
    }
    rowin += wrapin * factor;
    rowout += wrapout;
  }
  if (yrest) {
    pixin  = rowin;
    pixout = rowout;
    for (x = x1; x < xlast; x += factor) {
      tmp_r = tmp_g = tmp_b = 0;
      for (j = 0; j < yrest; j++)
        for (i = 0; i < factor; i++) {
          win = pixin[i + j * wrapin];
          MAP24_64(win, penmap, colmap, valin)
          tmp_r += valin.r;
          tmp_g += valin.g;
          tmp_b += valin.b;
        }
      tmp_r     = (tmp_r + yrest_fac_2) / yrest_fac;
      tmp_g     = (tmp_g + yrest_fac_2) / yrest_fac;
      tmp_b     = (tmp_b + yrest_fac_2) / yrest_fac;
      *pixout++ = PIX_RGB16_FROM_BYTES(tmp_r, tmp_g, tmp_b);
      pixin += factor;
    }
    if (xrest) {
      tmp_r = tmp_g = tmp_b = 0;
      for (j = 0; j < yrest; j++)
        for (i = 0; i < xrest; i++) {
          win = pixin[i + j * wrapin];
          MAP24_64(win, penmap, colmap, valin)
          tmp_r += valin.r;
          tmp_g += valin.g;
          tmp_b += valin.b;
        }
      tmp_r   = (tmp_r + yrest_xrest_2) / yrest_xrest;
      tmp_g   = (tmp_g + yrest_xrest_2) / yrest_xrest;
      tmp_b   = (tmp_b + yrest_xrest_2) / yrest_xrest;
      *pixout = PIX_RGB16_FROM_BYTES(tmp_r, tmp_g, tmp_b);
    }
  }
}

/*---------------------------------------------------------------------------*/

E 84 static void rop_copy_rgbx_rgb16(RASTER *rin, RASTER *rout, int x1, int y1,
                                      int x2, int y2, int newx, int newy) {
  LPIXEL *rowin, *pixin, val;
  USHORT *rowout, *pixout;
  int wrapin, wrapout;
  int x, y, lx, ly;

  lx      = x2 - x1 + 1;
  ly      = y2 - y1 + 1;
  wrapin  = rin->wrap;
  wrapout = rout->wrap;

  rowin  = (LPIXEL *)rin->buffer + wrapin * y1 + x1;
  rowout = (USHORT *)rout->buffer + wrapout * newy + newx;
  while (ly-- > 0) {
    pixin  = rowin;
    pixout = rowout;
    for (x = 0; x < lx; x++) {
      val = *pixin++;
      D 70 *pixout++ =
          (val.r << 8) & 0xf800 | (val.g << 3) & 0x7e0 | (val.b >> 3);
      E 70
I 70 *pixout++ = PIX_RGB16_FROM_RGBX(val);
      E 70
    }
    rowin += wrapin;
    rowout += wrapout;
  }
}
I 95
    /*---------------------------------------------------------------------------*/
I 96
#define SWAP_SHORT(N) (((N) >> 8) | ((N) << 8))
E 96

I 96

    static void
    rop_copy_rgbx_xrgb1555(RASTER *rin, RASTER *rout, int x1, int y1, int x2,
                           int y2, int newx, int newy) {
  LPIXEL *rowin, *pixin, val;
  USHORT *rowout, *pixout, outval;
  int wrapin, wrapout;
  int x, y, lx, ly;

  lx      = x2 - x1 + 1;
  ly      = y2 - y1 + 1;
  wrapin  = rin->wrap;
  wrapout = rout->wrap;

  rowin  = (LPIXEL *)rin->buffer + wrapin * y1 + x1;
  rowout = (USHORT *)rout->buffer + wrapout * newy + newx;
  while (ly-- > 0) {
    pixin  = rowin;
    pixout = rowout;
    for (x = 0; x < lx; x++) {
      val       = *pixin++;
      outval    = PIX_XRGB1555_FROM_RGBX(val);
      *pixout++ = SWAP_SHORT(outval);
    }
    rowin += wrapin;
    rowout += wrapout;
  }
}
/*---------------------------------------------------------------------------*/

E 96 static void rop_copy_rgbx64_rgb16(RASTER *rin, RASTER *rout, int x1,
                                        int y1, int x2, int y2, int newx,
                                        int newy) {
  SPIXEL *rowin, *pixin, val;
  USHORT *rowout, *pixout;
  int wrapin, wrapout;
  int x, y, lx, ly;

  lx         = x2 - x1 + 1;
  ly         = y2 - y1 + 1;
  wrapin     = rin->wrap;
  wrapout    = rout->wrap;
  E 95

I 95 rowin = (SPIXEL *)rin->buffer + wrapin * y1 + x1;
  rowout     = (USHORT *)rout->buffer + wrapout * newy + newx;
  while (ly-- > 0) {
    pixin  = rowin;
    pixout = rowout;
    for (x = 0; x < lx; x++) {
      val       = *pixin++;
      *pixout++ = PIX_RGB16_FROM_RGBX64(val);
    }
    rowin += wrapin;
    rowout += wrapout;
  }
}
E 95
    /*---------------------------------------------------------------------------*/

I 96 static void
rop_copy_rgbx64_xrgb1555(RASTER *rin, RASTER *rout, int x1, int y1, int x2,
                         int y2, int newx, int newy) {
  SPIXEL *rowin, *pixin, val;
  USHORT *rowout, *pixout;
  int wrapin, wrapout;
  int x, y, lx, ly;

  lx      = x2 - x1 + 1;
  ly      = y2 - y1 + 1;
  wrapin  = rin->wrap;
  wrapout = rout->wrap;

  rowin  = (SPIXEL *)rin->buffer + wrapin * y1 + x1;
  rowout = (USHORT *)rout->buffer + wrapout * newy + newx;
  while (ly-- > 0) {
    pixin  = rowin;
    pixout = rowout;
    for (x = 0; x < lx; x++) {
      val       = *pixin++;
      *pixout++ = PIX_XRGB1555_FROM_RGBX64(val);
    }
    rowin += wrapin;
    rowout += wrapout;
  }
}
/*---------------------------------------------------------------------------*/

E 96
I 70 static void rop_copy_rgb16_rgbm(RASTER *rin, RASTER *rout, int x1,
                                       int y1, int x2, int y2, int newx,
                                       int newy) {
  USHORT *rowin, *pixin;
  LPIXEL *rowout, *pixout;
  int wrapin, wrapout;
  int x, y, lx, ly;

  lx      = x2 - x1 + 1;
  ly      = y2 - y1 + 1;
  wrapin  = rin->wrap;
  wrapout = rout->wrap;

  rowin  = (USHORT *)rin->buffer + wrapin * y1 + x1;
  rowout = (LPIXEL *)rout->buffer + wrapout * newy + newx;
  while (ly-- > 0) {
    pixin  = rowin;
    pixout = rowout;
    for (x = 0; x < lx; x++, *pixin++, *pixout++)
      PIX_RGB16_TO_RGBM(*pixin, *pixout)
    rowin += wrapin;
    rowout += wrapout;
  }
}

/*---------------------------------------------------------------------------*/

I 96 static void rop_copy_xrgb1555_rgbm(RASTER *rin, RASTER *rout, int x1,
                                         int y1, int x2, int y2, int newx,
                                         int newy) {
  USHORT *rowin, *pixin;
  LPIXEL *rowout, *pixout;
  int wrapin, wrapout;
  int x, y, lx, ly;

  lx      = x2 - x1 + 1;
  ly      = y2 - y1 + 1;
  wrapin  = rin->wrap;
  wrapout = rout->wrap;

  rowin  = (USHORT *)rin->buffer + wrapin * y1 + x1;
  rowout = (LPIXEL *)rout->buffer + wrapout * newy + newx;
  while (ly-- > 0) {
    pixin  = rowin;
    pixout = rowout;
    for (x = 0; x < lx; x++, *pixin++, *pixout++)
      PIX_XRGB1555_TO_RGBM(*pixin, *pixout)
    rowin += wrapin;
    rowout += wrapout;
  }
}

/*---------------------------------------------------------------------------*/

E 96
E 70 static void rop_zoom_out_rgbx_rgb16(RASTER *rin, RASTER *rout, int x1,
                                           int y1, int x2, int y2, int newx,
                                           int newy, int abs_zoom_level) {
  LPIXEL *rowin, *pixin, *in, valin;
  USHORT *rowout, *pixout;
  int tmp_r, tmp_g, tmp_b;
  int wrapin, wrapout;
  int x, y, lx, ly, xlast, ylast, xrest, yrest, i, j;
  int factor, fac_fac_2_bits;
  int fac_fac, yrest_fac, fac_xrest, yrest_xrest;
  int fac_fac_2, yrest_fac_2, fac_xrest_2, yrest_xrest_2;
  int fac_fac_4;

  lx             = x2 - x1 + 1;
  ly             = y2 - y1 + 1;
  factor         = 1 << abs_zoom_level;
  D 61 xrest    = lx % factor;
  yrest          = ly % factor;
  E 61
I 61 xrest     = lx & (factor - 1);
  yrest          = ly & (factor - 1);
  E 61 xlast    = x2 - xrest + 1;
  ylast          = y2 - yrest + 1;
  fac_fac        = factor * factor;
  fac_fac_2      = fac_fac >> 1;
  fac_fac_4      = fac_fac >> 2;
  fac_fac_2_bits = 2 * abs_zoom_level - 1;
  yrest_fac      = yrest * factor;
  yrest_fac_2    = yrest_fac >> 1;
  fac_xrest      = factor * xrest;
  fac_xrest_2    = fac_xrest >> 1;
  yrest_xrest    = yrest * xrest;
  yrest_xrest_2  = yrest_xrest >> 1;
  wrapin         = rin->wrap;
  wrapout        = rout->wrap;

  rowin  = (LPIXEL *)rin->buffer + wrapin * y1 + x1;
  rowout = (USHORT *)rout->buffer + wrapout * newy + newx;
  for (y = y1; y < ylast; y += factor) {
    pixin  = rowin;
    pixout = rowout;
    for (x = x1; x < xlast; x += factor) {
      tmp_r = tmp_g = tmp_b = 0;
      in                    = pixin;
      for (j = 0; j < factor; j += 2) {
        for (i = 0; i < factor; i += 2) {
          valin = *in;
          tmp_r += valin.r;
          tmp_g += valin.g;
          tmp_b += valin.b;
          in += wrapin;
          in++;
          valin = *in;
          tmp_r += valin.r;
          tmp_g += valin.g;
          tmp_b += valin.b;
          in -= wrapin;
          in++;
        }
        in += 2 * wrapin - factor;
      }
      tmp_r           = (tmp_r + fac_fac_4) >> fac_fac_2_bits;
      tmp_g           = (tmp_g + fac_fac_4) >> fac_fac_2_bits;
      tmp_b           = (tmp_b + fac_fac_4) >> fac_fac_2_bits;
      D 54 *pixout++ = BYTES_TO_RGB16(tmp_r, tmp_g, tmp_b);
      E 54
I 54
D 60 *pixout++      = ROP_RGB16_FROM_BYTES(tmp_r, tmp_g, tmp_b);
      E 60
I 60 *pixout++      = PIX_RGB16_FROM_BYTES(tmp_r, tmp_g, tmp_b);
      E 60
E 54 pixin += factor;
    }
    if (xrest) {
      tmp_r = tmp_g = tmp_b = 0;
      for (j = 0; j < factor; j++)
        for (i = 0; i < xrest; i++) {
          valin = pixin[i + j * wrapin];
          tmp_r += valin.r;
          tmp_g += valin.g;
          tmp_b += valin.b;
        }
      tmp_r         = (tmp_r + fac_xrest_2) / fac_xrest;
      tmp_g         = (tmp_g + fac_xrest_2) / fac_xrest;
      tmp_b         = (tmp_b + fac_xrest_2) / fac_xrest;
      D 54 *pixout = BYTES_TO_RGB16(tmp_r, tmp_g, tmp_b);
      E 54
I 54
D 60 *pixout      = ROP_RGB16_FROM_BYTES(tmp_r, tmp_g, tmp_b);
      E 60
I 60 *pixout      = PIX_RGB16_FROM_BYTES(tmp_r, tmp_g, tmp_b);
      E 60
E 54
    }
    rowin += wrapin * factor;
    rowout += wrapout;
  }
  if (yrest) {
    pixin  = rowin;
    pixout = rowout;
    for (x = x1; x < xlast; x += factor) {
      tmp_r = tmp_g = tmp_b = 0;
      for (j = 0; j < yrest; j++)
        for (i = 0; i < factor; i++) {
          valin = pixin[i + j * wrapin];
          tmp_r += valin.r;
          tmp_g += valin.g;
          tmp_b += valin.b;
        }
      tmp_r           = (tmp_r + yrest_fac_2) / yrest_fac;
      tmp_g           = (tmp_g + yrest_fac_2) / yrest_fac;
      tmp_b           = (tmp_b + yrest_fac_2) / yrest_fac;
      D 54 *pixout++ = BYTES_TO_RGB16(tmp_r, tmp_g, tmp_b);
      E 54
I 54
D 60 *pixout++      = ROP_RGB16_FROM_BYTES(tmp_r, tmp_g, tmp_b);
      E 60
I 60 *pixout++      = PIX_RGB16_FROM_BYTES(tmp_r, tmp_g, tmp_b);
      E 60
E 54 pixin += factor;
    }
    if (xrest) {
      tmp_r = tmp_g = tmp_b = 0;
      for (j = 0; j < yrest; j++)
        for (i = 0; i < xrest; i++) {
          valin = pixin[i + j * wrapin];
          tmp_r += valin.r;
          tmp_g += valin.g;
          tmp_b += valin.b;
        }
      tmp_r         = (tmp_r + yrest_xrest_2) / yrest_xrest;
      tmp_g         = (tmp_g + yrest_xrest_2) / yrest_xrest;
      tmp_b         = (tmp_b + yrest_xrest_2) / yrest_xrest;
      D 54 *pixout = BYTES_TO_RGB16(tmp_r, tmp_g, tmp_b);
      E 54
I 54
D 60 *pixout      = ROP_RGB16_FROM_BYTES(tmp_r, tmp_g, tmp_b);
      E 60
I 60 *pixout      = PIX_RGB16_FROM_BYTES(tmp_r, tmp_g, tmp_b);
      E 60
E 54
    }
  }
}

/*---------------------------------------------------------------------------*/

I 95 static void rop_zoom_out_rgbx64_rgb16(RASTER *rin, RASTER *rout, int x1,
                                            int y1, int x2, int y2, int newx,
                                            int newy, int abs_zoom_level) {
  SPIXEL *rowin, *pixin, *in, valin;
  USHORT *rowout, *pixout;
  int tmp_r, tmp_g, tmp_b;
  int wrapin, wrapout;
  int x, y, lx, ly, xlast, ylast, xrest, yrest, i, j;
  int factor, fac_fac_2_bits;
  int fac_fac, yrest_fac, fac_xrest, yrest_xrest;
  int fac_fac_2, yrest_fac_2, fac_xrest_2, yrest_xrest_2;
  int fac_fac_4;

  lx             = x2 - x1 + 1;
  ly             = y2 - y1 + 1;
  factor         = 1 << abs_zoom_level;
  xrest          = lx & (factor - 1);
  yrest          = ly & (factor - 1);
  xlast          = x2 - xrest + 1;
  ylast          = y2 - yrest + 1;
  fac_fac        = factor * factor;
  fac_fac_4      = fac_fac >> 2;
  fac_fac_2_bits = 2 * abs_zoom_level - 1;
  yrest_fac      = yrest * factor;
  yrest_fac_2    = yrest_fac >> 1;
  fac_xrest      = factor * xrest;
  fac_xrest_2    = fac_xrest >> 1;
  yrest_xrest    = yrest * xrest;
  yrest_xrest_2  = yrest_xrest >> 1;
  /*dafare*/
  wrapin  = rin->wrap;
  wrapout = rout->wrap;

  rowin  = (SPIXEL *)rin->buffer + wrapin * y1 + x1;
  rowout = (USHORT *)rout->buffer + wrapout * newy + newx;
  for (y = y1; y < ylast; y += factor) {
    pixin  = rowin;
    pixout = rowout;
    for (x = x1; x < xlast; x += factor) {
      tmp_r = tmp_g = tmp_b = 0;
      in                    = pixin;
      for (j = 0; j < factor; j += 2) {
        for (i = 0; i < factor; i += 2) {
          valin = *in;
          tmp_r += valin.r;
          tmp_g += valin.g;
          tmp_b += valin.b;
          in += wrapin;
          in++;
          valin = *in;
          tmp_r += valin.r;
          tmp_g += valin.g;
          tmp_b += valin.b;
          in -= wrapin;
          in++;
        }
        in += 2 * wrapin - factor;
      }
      tmp_r     = (tmp_r + fac_fac_4) >> fac_fac_2_bits;
      tmp_g     = (tmp_g + fac_fac_4) >> fac_fac_2_bits;
      tmp_b     = (tmp_b + fac_fac_4) >> fac_fac_2_bits;
      *pixout++ = PIX_RGB16_FROM_USHORT(tmp_r, tmp_g, tmp_b);
      pixin += factor;
    }
    if (xrest) {
      tmp_r = tmp_g = tmp_b = 0;
      for (j = 0; j < factor; j++)
        for (i = 0; i < xrest; i++) {
          valin = pixin[i + j * wrapin];
          tmp_r += valin.r;
          tmp_g += valin.g;
          tmp_b += valin.b;
        }
      tmp_r   = (tmp_r + fac_xrest_2) / fac_xrest;
      tmp_g   = (tmp_g + fac_xrest_2) / fac_xrest;
      tmp_b   = (tmp_b + fac_xrest_2) / fac_xrest;
      *pixout = PIX_RGB16_FROM_USHORT(tmp_r, tmp_g, tmp_b);
    }
    rowin += wrapin * factor;
    rowout += wrapout;
  }
  if (yrest) {
    pixin  = rowin;
    pixout = rowout;
    for (x = x1; x < xlast; x += factor) {
      tmp_r = tmp_g = tmp_b = 0;
      for (j = 0; j < yrest; j++)
        for (i = 0; i < factor; i++) {
          valin = pixin[i + j * wrapin];
          tmp_r += valin.r;
          tmp_g += valin.g;
          tmp_b += valin.b;
        }
      tmp_r     = (tmp_r + yrest_fac_2) / yrest_fac;
      tmp_g     = (tmp_g + yrest_fac_2) / yrest_fac;
      tmp_b     = (tmp_b + yrest_fac_2) / yrest_fac;
      *pixout++ = PIX_RGB16_FROM_USHORT(tmp_r, tmp_g, tmp_b);
      pixin += factor;
    }
    if (xrest) {
      tmp_r = tmp_g = tmp_b = 0;
      for (j = 0; j < yrest; j++)
        for (i = 0; i < xrest; i++) {
          valin = pixin[i + j * wrapin];
          tmp_r += valin.r;
          tmp_g += valin.g;
          tmp_b += valin.b;
        }
      tmp_r   = (tmp_r + yrest_xrest_2) / yrest_xrest;
      tmp_g   = (tmp_g + yrest_xrest_2) / yrest_xrest;
      tmp_b   = (tmp_b + yrest_xrest_2) / yrest_xrest;
      *pixout = PIX_RGB16_FROM_USHORT(tmp_r, tmp_g, tmp_b);
    }
  }
}

/*---------------------------------------------------------------------------*/

E 95
E 52
E 46
E 28
E 20

    /* copia un rettangolo da rin a rout.
* Le coordinate sono relative ai due raster: (0, 0) corrisponde
* al primo pixel del raster.
*
*/

    void
    rop_copy(RASTER *rin, RASTER *rout, int x1, int y1, int x2, int y2,
             int newx, int newy) {
  D 13 char *rowin, *rowout;
  E 13
I 13
D 41 UCHAR *rowin, *rowout;
  E 13
D 10 int rowsize, wrapin, wrapout, d, tmp;
  E 10
I 10 int rowsize, bytewrapin, bytewrapout, d, tmp;
  E 10
D 12 int pixbits_in, pixbits_out;
  E 12
I 12 int pixbits_in, pixbits_out;
  int pixbytes_in, pixbytes_out;
  E 41
I 41 int tmp;
  E 41
E 12
I 4 int rasras;
  E 4

      /* raddrizzo gli estremi */
      if (x1 > x2) {
    tmp = x1;
    x1  = x2;
    x2  = tmp;
  }
  if (y1 > y2) {
    tmp = y1;
    y1  = y2;
    y2  = tmp;
  }

  /* controllo gli sconfinamenti */
  if (x1 < 0 || y1 < 0 || x2 >= rin->lx || y2 >= rin->ly || newx < 0 ||
      newy < 0 || newx + x2 - x1 >= rout->lx || newy + y2 - y1 >= rout->ly)
    D 27 {
      printf("### INTERNAL ERROR - rop_copy; access violation\n");
      return;
    }
  E 27
I 27 {
    printf(
        "### INTERNAL ERROR - rop_copy; access violation\n"
        " ((%d,%d)(%d,%d)in(%dx%d)->(%d,%d)in(%dx%d))\n",
        x1, y1, x2, y2, rin->lx, rin->ly, newx, newy, rout->lx, rout->ly);
    return;
  }
  E 27

D 2 if (rin->type == RAS_CM && rout->type == RAS_RGBM) 
E 2
I 2
D 4 if (rin->type == RAS_CM16 && rout->type == RAS_RGBM) 
E 2 {
    D 2 rop_copy_cm_rgbm(rin, rout, x1, y1, x2, y2, newx, newy);
    E 2
I 2 rop_copy_cm16_rgbm(rin, rout, x1, y1, x2, y2, newx, newy);
    E 2 return;
  }

  E 4
D 10 pixbits_in = rop_pixbits(rin->type);
  pixbits_out     = rop_pixbits(rout->type);
  E 10
I 10
D 23 pixbits_in = rop_pixbits(rin->type);
  pixbytes_in     = rop_pixbytes(rin->type);
  pixbits_out     = rop_pixbits(rout->type);
  pixbytes_out    = rop_pixbytes(rout->type);
  E 23
E 10
D 4

E 4
I 4
D 96 rasras     = RASRAS(rin->type, rout->type);
  switch (rasras)
    E 96
I 96 if (rin->type == rout->type)
        rop_copy_same(rin, rout, x1, y1, x2, y2, newx, newy);
  else E 96 {
    I 73
D 96 CASE RASRAS(RAS_RGBM64, RAS_RGBM64)
        : __OR RASRAS(RAS_RGB_64, RAS_RGB_64)
        : __OR RASRAS(RAS_RGBM, RAS_RGB_)
        : __OR RASRAS(RAS_RGBM, RAS_RGBM)
        : __OR RASRAS(RAS_RGB_, RAS_RGB_)
        : __OR RASRAS(RAS_RGB, RAS_RGB)
        : __OR RASRAS(RAS_RGB16, RAS_RGB16)
        :
D 83 __OR RASRAS(RAS_UNICM16, RAS_UNICM16)
        :
E 83 __OR RASRAS(RAS_CM16S8, RAS_CM16S8)
        : __OR RASRAS(RAS_CM16S4, RAS_CM16S4)
        : __OR RASRAS(RAS_CM16, RAS_CM16)
        : __OR RASRAS(RAS_MBW16, RAS_MBW16)
        : __OR RASRAS(RAS_GR8, RAS_GR8)
        :
I 90 __OR RASRAS(RAS_GR16, RAS_GR16)
        :
E 90
D 83 __OR RASRAS(RAS_UNICM8, RAS_UNICM8)
        :
E 83 __OR RASRAS(RAS_CM8, RAS_CM8)
        : __OR RASRAS(RAS_CM8S8, RAS_CM8S8)
        : __OR RASRAS(RAS_CM8S4, RAS_CM8S4)
        :
I 84 __OR RASRAS(RAS_CM24, RAS_CM24)
        :
E 84 rop_copy_same(rin, rout, x1, y1, x2, y2, newx, newy);

    E 73
D 18 CASE RASRAS(RAS_CM16, RAS_RGBM)
        :
E 18
I 18 CASE RASRAS(RAS_GR8, RAS_RGB_)
        : __OR RASRAS(RAS_GR8, RAS_RGBM)
        : rop_copy_gr8_rgbm(rin, rout, x1, y1, x2, y2, newx, newy);
    D 41 return;
    E 41

        CASE RASRAS(RAS_CM16, RAS_RGB_)
        : __OR RASRAS(RAS_CM16, RAS_RGBM)
        :
E 18 rop_copy_cm16_rgbm(rin, rout, x1, y1, x2, y2, newx, newy);
    D 41 return;
    E 41

I 84 CASE RASRAS(RAS_CM24, RAS_RGB_)
        : __OR RASRAS(RAS_CM24, RAS_RGBM)
        : rop_copy_cm24_rgbm(rin, rout, x1, y1, x2, y2, newx, newy);

    E 84
I 58 CASE RASRAS(RAS_CM16, RAS_RGB_64)
        : __OR RASRAS(RAS_CM16, RAS_RGBM64)
        : rop_copy_cm16_rgbm64(rin, rout, x1, y1, x2, y2, newx, newy);

    I 84 CASE RASRAS(RAS_CM24, RAS_RGB_64)
        : __OR RASRAS(RAS_CM24, RAS_RGBM64)
        : rop_copy_cm24_rgbm64(rin, rout, x1, y1, x2, y2, newx, newy);

    E 84 CASE RASRAS(RAS_RGBM, RAS_RGBM64)
        : rop_copy_rgbm_rgbm64(rin, rout, x1, y1, x2, y2, newx, newy);

    CASE RASRAS(RAS_RGBM64, RAS_RGBM)
        : rop_copy_rgbm64_rgbm(rin, rout, x1, y1, x2, y2, newx, newy);

    I 95 CASE RASRAS(RAS_RGB_64, RAS_RGB_)
        : __OR RASRAS(RAS_RGBM64, RAS_RGB_)
        : __OR RASRAS(RAS_RGB_64, RAS_RGBM)
        : rop_copy_rgbx64_rgbx(rin, rout, x1, y1, x2, y2, newx, newy);

    E 95
I 81 CASE RASRAS(RAS_RGB, RAS_RGB16)
        : rop_copy_rgb_rgb16(rin, rout, x1, y1, x2, y2, newx, newy);

    E 81
E 58
I 18 CASE RASRAS(RAS_RGB, RAS_RGB_)
        : __OR RASRAS(RAS_RGB, RAS_RGBM)
        : rop_copy_rgb_rgbm(rin, rout, x1, y1, x2, y2, newx, newy);
    D 41 return;
    E 41

I 68 CASE RASRAS(RAS_BW, RAS_BW)
        : __OR RASRAS(RAS_BW, RAS_WB)
        : __OR RASRAS(RAS_WB, RAS_BW)
        : __OR RASRAS(RAS_WB, RAS_WB)
        :
D 69 rop_copy_bw_bw(rin, rout, x1, y1, x2, y2, newx, newy);
    E 69
I 69 rop_copy_bw(rin, rout, x1, y1, x2, y2, newx, newy);
    E 69

E 68
E 18 CASE RASRAS(RAS_BW, RAS_CM16)
        : __OR RASRAS(RAS_WB, RAS_CM16)
        : rop_copy_bw_cm16(rin, rout, x1, y1, x2, y2, newx, newy);
    I 19
D 41 return;
    E 41

I 84 CASE RASRAS(RAS_BW, RAS_CM24)
        : __OR RASRAS(RAS_WB, RAS_CM24)
        : rop_copy_bw_cm24(rin, rout, x1, y1, x2, y2, newx, newy);

    E 84 CASE RASRAS(RAS_BW, RAS_RGB_)
        : __OR RASRAS(RAS_BW, RAS_RGBM)
        : __OR RASRAS(RAS_WB, RAS_RGB_)
        : __OR RASRAS(RAS_WB, RAS_RGBM)
        : rop_copy_bw_rgbm(rin, rout, x1, y1, x2, y2, newx, newy);
    E 19
D 41 return;
    E 41
I 6

        CASE RASRAS(RAS_GR8, RAS_CM16)
        : rop_copy_gr8_cm16(rin, rout, x1, y1, x2, y2, newx, newy);
    D 41 return;
    E 41
I 23

I 84 CASE RASRAS(RAS_GR8, RAS_CM24)
        : rop_copy_gr8_cm24(rin, rout, x1, y1, x2, y2, newx, newy);

    E 84 CASE RASRAS(RAS_CM8, RAS_CM16)
        : rop_copy_cm8_cm16(rin, rout, x1, y1, x2, y2, newx, newy);
    D 41 return;
    I 39 CASE RASRAS(RAS_CM16, RAS_BW)
        : rop_copy_cm8_cm16(rin, rout, x1, y1, x2, y2, newx, newy);
    return;
  DEFAULT:
    rop_copy_quantize(rasras, rin, rout, x1, y1, x2, y2, newx, newy);
    return;
    E 39
E 23
  }
  I 23 pixbits_in = rop_pixbits(rin->type);
  pixbytes_in      = rop_pixbytes(rin->type);
  pixbits_out      = rop_pixbits(rout->type);
  pixbytes_out     = rop_pixbytes(rout->type);
  E 41

I 84 CASE RASRAS(RAS_CM8, RAS_CM24)
      : rop_copy_cm8_cm24(rin, rout, x1, y1, x2, y2, newx, newy);

  E 84
E 23
D 41
      /* pixsize compatibili */
      if (pixbits_in != pixbits_out) {
    printf("### INTERNAL ERROR - rop_copy; different pixel size\n");
    return;
  }

  /* per adesso niente pixel non multipli di 8 bits */
  if (rop_fillerbits(rin->type) || rop_fillerbits(rout->type)) {
    printf("### INTERNAL ERROR - rop_copy; fillerbits not allowed\n");
    return;
  }
  E 41
I 41
D 52 CASE RASRAS(RAS_GR8, RAS_GR8)
      : __OR RASRAS(RAS_CM8, RAS_CM8)
      : __OR RASRAS(RAS_CM16, RAS_CM16)
      : __OR RASRAS(RAS_RGB, RAS_RGB)
      : __OR RASRAS(RAS_RGB_, RAS_RGB_)
      : __OR RASRAS(RAS_RGBM, RAS_RGB_)
      : __OR RASRAS(RAS_RGBM, RAS_RGBM)
      :
E 52
I 52
D 73 CASE RASRAS(RAS_GR8, RAS_GR8)
      : __OR RASRAS(RAS_CM8, RAS_CM8)
      : __OR RASRAS(RAS_CM16, RAS_CM16)
      : __OR RASRAS(RAS_RGB16, RAS_RGB16)
      : __OR RASRAS(RAS_RGB, RAS_RGB)
      : __OR RASRAS(RAS_RGB_, RAS_RGB_)
      : __OR RASRAS(RAS_RGBM, RAS_RGB_)
      : __OR RASRAS(RAS_RGBM, RAS_RGBM)
      :
I 58 __OR RASRAS(RAS_RGBM64, RAS_RGBM64)
      : __OR RASRAS(RAS_RGB_64, RAS_RGB_64)
      :
E 58
E 52 __OR RASRAS(RAS_MBW16, RAS_MBW16)
      : __OR RASRAS(RAS_UNICM8, RAS_UNICM8)
      : __OR RASRAS(RAS_UNICM16, RAS_UNICM16)
      :
I 72 __OR RASRAS(RAS_CM8S8, RAS_CM8S8)
      : __OR RASRAS(RAS_CM8S4, RAS_CM8S4)
      : __OR RASRAS(RAS_CM16S8, RAS_CM16S8)
      : __OR RASRAS(RAS_CM16S4, RAS_CM16S4)
      :
E 72 rop_copy_same(rin, rout, x1, y1, x2, y2, newx, newy);

  D 71 CASE RASRAS(RAS_RGBM, RAS_UNICM16)
      :
E 71
I 71 CASE RASRAS(RAS_RGB_, RAS_UNICM16)
      : __OR RASRAS(RAS_RGBM, RAS_UNICM16)
      : __OR RASRAS(RAS_RGB_, RAS_UNICM8)
      :
E 71 __OR RASRAS(RAS_RGBM, RAS_UNICM8)
      :
I 71 __OR RASRAS(RAS_RGB_, RAS_GR8)
      :
E 71 __OR RASRAS(RAS_RGBM, RAS_GR8)
      :
I 71 __OR RASRAS(RAS_RGB_, RAS_MBW16)
      :
E 71 __OR RASRAS(RAS_RGBM, RAS_MBW16)
      :
I 72 __OR RASRAS(RAS_RGB_, RAS_BW)
      : __OR RASRAS(RAS_RGBM, RAS_BW)
      : __OR RASRAS(RAS_RGB16, RAS_BW)
      :
E 72
I 42 __OR RASRAS(RAS_CM16, RAS_UNICM16)
      : __OR RASRAS(RAS_CM16, RAS_UNICM8)
      : __OR RASRAS(RAS_CM16, RAS_GR8)
      : __OR RASRAS(RAS_CM16, RAS_MBW16)
      :
E 42 __OR RASRAS(RAS_UNICM16, RAS_UNICM8)
      : __OR RASRAS(RAS_UNICM16, RAS_GR8)
      :
I 71 __OR RASRAS(RAS_RGB_, RAS_CM16S8)
      : __OR RASRAS(RAS_RGBM, RAS_CM16S8)
      : __OR RASRAS(RAS_RGB_, RAS_CM16S4)
      : __OR RASRAS(RAS_RGBM, RAS_CM16S4)
      : __OR RASRAS(RAS_RGB_, RAS_CM8S8)
      : __OR RASRAS(RAS_RGBM, RAS_CM8S8)
      :
I 72 __OR RASRAS(RAS_RGB16, RAS_CM8S8)
      :
E 72 __OR RASRAS(RAS_RGB_, RAS_CM8S4)
      : __OR RASRAS(RAS_RGBM, RAS_CM8S4)
      : __OR RASRAS(RAS_CM16, RAS_CM16S8)
      : __OR RASRAS(RAS_CM16, RAS_CM16S4)
      : __OR RASRAS(RAS_CM16, RAS_CM8S8)
      : __OR RASRAS(RAS_CM16, RAS_CM8S4)
      :
E 71
D 42 __OR RASRAS(RAS_CM16, RAS_MBW16)
      :
E 42 rop_copy_quantize(rasras, rin, rout, x1, y1, x2, y2, newx, newy);
  E 41

E 73
I 54 CASE RASRAS(RAS_BW, RAS_RGB16)
      : __OR RASRAS(RAS_WB, RAS_RGB16)
      : rop_copy_bw_rgb16(rin, rout, x1, y1, x2, y2, newx, newy);

  CASE RASRAS(RAS_GR8, RAS_RGB16)
      : rop_copy_gr8_rgb16(rin, rout, x1, y1, x2, y2, newx, newy);

  E 54
I 52 CASE RASRAS(RAS_CM16, RAS_RGB16)
      : rop_copy_cm16_rgb16(rin, rout, x1, y1, x2, y2, newx, newy);

  I 84 CASE RASRAS(RAS_CM24, RAS_RGB16)
      : rop_copy_cm24_rgb16(rin, rout, x1, y1, x2, y2, newx, newy);

  E 84 CASE RASRAS(RAS_RGB_, RAS_RGB16)
      : __OR RASRAS(RAS_RGBM, RAS_RGB16)
      : rop_copy_rgbx_rgb16(rin, rout, x1, y1, x2, y2, newx, newy);
  I 70

I 95 CASE RASRAS(RAS_RGB_64, RAS_RGB16)
      : __OR RASRAS(RAS_RGBM64, RAS_RGB16)
      : rop_copy_rgbx64_rgb16(rin, rout, x1, y1, x2, y2, newx, newy);

  E 95
D 74 CASE RASRAS(RAS_RGB16, RAS_RGBM)
      :
E 74
I 74 CASE RASRAS(RAS_RGB16, RAS_RGB_)
      : __OR RASRAS(RAS_RGB16, RAS_RGBM)
      :
E 74 rop_copy_rgb16_rgbm(rin, rout, x1, y1, x2, y2, newx, newy);
  E 70

I 67 CASE RASRAS(RAS_RGB_, RAS_RGB)
      : __OR RASRAS(RAS_RGBM, RAS_RGB)
      : rop_copy_rgbx_rgb(rin, rout, x1, y1, x2, y2, newx, newy);
  I 73

D 83 CASE RASRAS(RAS_RGBM, RAS_UNICM16)
      : __OR RASRAS(RAS_RGBM, RAS_CM16S8)
      : __OR RASRAS(RAS_RGBM, RAS_CM16S4)
      : __OR RASRAS(RAS_RGBM, RAS_MBW16)
      : __OR RASRAS(RAS_RGBM, RAS_UNICM8)
      : __OR RASRAS(RAS_RGBM, RAS_GR8)
      : __OR RASRAS(RAS_RGBM, RAS_CM8S8)
      : __OR RASRAS(RAS_RGBM, RAS_CM8S4)
      : __OR RASRAS(RAS_RGBM, RAS_BW)
      :

      __OR RASRAS(RAS_RGB_, RAS_UNICM16)
      : __OR RASRAS(RAS_RGB_, RAS_CM16S8)
      : __OR RASRAS(RAS_RGB_, RAS_CM16S4)
      : __OR RASRAS(RAS_RGB_, RAS_MBW16)
      : __OR RASRAS(RAS_RGB_, RAS_UNICM8)
      : __OR RASRAS(RAS_RGB_, RAS_GR8)
      : __OR RASRAS(RAS_RGB_, RAS_CM8S8)
      : __OR RASRAS(RAS_RGB_, RAS_CM8S4)
      : __OR RASRAS(RAS_RGB_, RAS_BW)
      :

      __OR RASRAS(RAS_RGB16, RAS_CM8S8)
      : __OR RASRAS(RAS_RGB16, RAS_BW)
      :

      __OR RASRAS(RAS_CM16, RAS_UNICM16)
      : __OR RASRAS(RAS_CM16, RAS_CM16S8)
      : __OR RASRAS(RAS_CM16, RAS_CM16S4)
      : __OR RASRAS(RAS_CM16, RAS_MBW16)
      : __OR RASRAS(RAS_CM16, RAS_UNICM8)
      : __OR RASRAS(RAS_CM16, RAS_GR8)
      : __OR RASRAS(RAS_CM16, RAS_CM8S8)
      : __OR RASRAS(RAS_CM16, RAS_CM8S4)
      : __OR RASRAS(RAS_CM16, RAS_BW)
      :

      __OR RASRAS(RAS_UNICM16, RAS_UNICM8)
      : __OR RASRAS(RAS_UNICM16, RAS_GR8)
      :
E 83
I 83 CASE RASRAS(RAS_RGBM, RAS_CM16S8)
      : __OR RASRAS(RAS_RGBM, RAS_CM16S4)
      : __OR RASRAS(RAS_RGBM, RAS_MBW16)
      : __OR RASRAS(RAS_RGBM, RAS_GR8)
      :
I 90 __OR RASRAS(RAS_RGBM64, RAS_GR8)
      : __OR RASRAS(RAS_RGBM, RAS_GR16)
      : __OR RASRAS(RAS_RGBM64, RAS_GR16)
      :
E 90 __OR RASRAS(RAS_RGBM, RAS_CM8S8)
      : __OR RASRAS(RAS_RGBM, RAS_CM8S4)
      : __OR RASRAS(RAS_RGBM, RAS_BW)
      : __OR RASRAS(RAS_RGB_, RAS_CM16S8)
      : __OR RASRAS(RAS_RGB_, RAS_CM16S4)
      : __OR RASRAS(RAS_RGB_, RAS_MBW16)
      : __OR RASRAS(RAS_RGB_, RAS_GR8)
      : __OR RASRAS(RAS_RGB_, RAS_CM8S8)
      : __OR RASRAS(RAS_RGB_, RAS_CM8S4)
      : __OR RASRAS(RAS_RGB_, RAS_BW)
      : __OR RASRAS(RAS_RGB16, RAS_CM8S8)
      : __OR RASRAS(RAS_RGB16, RAS_BW)
      : __OR RASRAS(RAS_CM16, RAS_CM16S8)
      : __OR RASRAS(RAS_CM16, RAS_CM16S4)
      : __OR RASRAS(RAS_CM16, RAS_MBW16)
      : __OR RASRAS(RAS_CM16, RAS_GR8)
      :
I 90 __OR RASRAS(RAS_CM16, RAS_GR16)
      :
E 90 __OR RASRAS(RAS_CM16, RAS_CM8S8)
      : __OR RASRAS(RAS_CM16, RAS_CM8S4)
      : __OR RASRAS(RAS_CM16, RAS_BW)
      :
I 84 __OR RASRAS(RAS_CM24, RAS_CM16S8)
      : __OR RASRAS(RAS_CM24, RAS_CM16S4)
      : __OR RASRAS(RAS_CM24, RAS_MBW16)
      : __OR RASRAS(RAS_CM24, RAS_GR8)
      :
D 89 __OR RASRAS(RAS_CM24, RAS_CM8S8)
      : __OR RASRAS(RAS_CM24, RAS_CM8S4)
      : __OR RASRAS(RAS_CM24, RAS_BW)
      :
E 84
E 83 rop_copy_quantize(rasras, rin, rout, x1, y1, x2, y2, newx, newy);
  E 73

E 67
E 52
D 41
      /* copia */
D 10 rowsize = (pixbits_in * (x2 - x1 + 1)) >> 3;
  I 7
#ifdef CHE_CAVOLO_VADO_A_SCRIVERE___WALTER
E 7 wrapin = (rin->wrap * pixbits_in) >> 3;
  wrapout    = (rout->wrap * pixbits_out) >> 3;
  D 7

E 7
I 7
#else
      wrapin = rin->wrap;
  wrapout    = rout->wrap;
#endif
E 7 rowin = (char *)rin->buffer + y1 * wrapin + ((x1 * pixbits_in) >> 3);
  rowout = (char *)rout->buffer + newy * wrapout + ((newx * pixbits_out) >> 3);
E 10
I 10
D 11
rowsize = pixbytes_in * (x2-x1+1));
E 11
I 11 rowsize   = pixbytes_in * (x2 - x1 + 1);
E 11 bytewrapin = rin->wrap * pixbytes_in;
bytewrapout      = rout->wrap * pixbytes_out;
D 12 rowin = (UCHAR *)rin->buffer + y1 * wrapin + ((x1 * pixbits_in) >> 3);
rowout = (UCHAR *)rout->buffer + newy * wrapout + ((newx * pixbits_out) >> 3);
E 12
I 12 rowin =
    (UCHAR *)rin->buffer + y1 * bytewrapin + ((x1 * pixbits_in) >> 3);
rowout =
    (UCHAR *)rout->buffer + newy * bytewrapout + ((newx * pixbits_out) >> 3);
E 12
E 10 d = y2 - y1 + 1;
while (d-- > 0) {
  memmove(rowout, rowin, rowsize);
  D 10 rowin += wrapin;
  rowout += wrapout;
  E 10
I 10 rowin += bytewrapin;
  rowout += bytewrapout;
  E 41
I 41 DEFAULT :
D 52 assert(FALSE);
  E 52
I 52 assert(!"rop_copy; invalid raster combination");
  E 52
E 41
I 20
}
}

/*---------------------------------------------------------------------------*/
D 84

    /* copia un rettangolo da rin a rout.
* Le coordinate sono relative ai due raster: (0, 0) corrisponde
* al primo pixel del raster.
* Riduce le dimensioni di un certo fattore facendo la media dei valori
* dei pixel.
*/

    void
    rop_reduce(RASTER *rin, RASTER *rout, int x1, int y1, int x2, int y2,
               int newx, int newy, int factor) {
  int tmp, newlx, newly;
  int rasras;

  if (factor == 1) rop_copy(rin, rout, x1, y1, x2, y2, newx, newy);

  /* raddrizzo gli estremi */
  if (x1 > x2) {
    tmp = x1;
    x1  = x2;
    x2  = tmp;
  }
  if (y1 > y2) {
    tmp = y1;
    y1  = y2;
    y2  = tmp;
  }

  /* controllo gli sconfinamenti */
  newlx = (x2 - x1 + factor) / factor;
  newly = (y2 - y1 + factor) / factor;
  if (x1 < 0 || y1 < 0 || x2 >= rin->lx || y2 >= rin->ly || newx < 0 ||
      newy < 0 || newx + newlx > rout->lx || newy + newly > rout->ly) {
    printf("### INTERNAL ERROR - rop_reduce; access violation\n");
    return;
  }

  rasras = RASRAS(rin->type, rout->type);
  switch (rasras) {
    CASE RASRAS(RAS_GR8, RAS_RGB_)
        : __OR RASRAS(RAS_GR8, RAS_RGBM)
        : rop_reduce_gr8_rgbm(rin, rout, x1, y1, x2, y2, newx, newy, factor);
    return;

    CASE RASRAS(RAS_CM16, RAS_RGB_)
        :
D 28 __OR RASRAS(RAS_CM16, RAS_RGBM)
        :
E 28
I 28
D 46 rop_reduce_cm16_rgb_(rin, rout, x1, y1, x2, y2, newx, newy, factor);
    E 46
I 46 rop_reduce_cm16_rgbx(rin, rout, x1, y1, x2, y2, newx, newy, factor);
    E 46 return;

    CASE RASRAS(RAS_CM16, RAS_RGBM)
        :
E 28 rop_reduce_cm16_rgbm(rin, rout, x1, y1, x2, y2, newx, newy, factor);
    return;

    I 81 CASE RASRAS(RAS_RGB, RAS_RGB16)
        : rop_reduce_rgb_rgb16(rin, rout, x1, y1, x2, y2, newx, newy, factor);
    return;

    E 81 CASE RASRAS(RAS_RGB, RAS_RGB_)
        : __OR RASRAS(RAS_RGB, RAS_RGBM)
        : rop_reduce_rgb_rgbm(rin, rout, x1, y1, x2, y2, newx, newy, factor);
    return;

    CASE RASRAS(RAS_BW, RAS_CM16)
        : __OR RASRAS(RAS_WB, RAS_CM16)
        : rop_reduce_bw_cm16(rin, rout, x1, y1, x2, y2, newx, newy, factor);
    return;

    CASE RASRAS(RAS_BW, RAS_RGB_)
        : __OR RASRAS(RAS_BW, RAS_RGBM)
        : __OR RASRAS(RAS_WB, RAS_RGB_)
        : __OR RASRAS(RAS_WB, RAS_RGBM)
        : rop_reduce_bw_rgbm(rin, rout, x1, y1, x2, y2, newx, newy, factor);
    return;

    CASE RASRAS(RAS_GR8, RAS_CM16)
        : rop_reduce_gr8_cm16(rin, rout, x1, y1, x2, y2, newx, newy, factor);
    return;

    CASE RASRAS(RAS_WB, RAS_GR8)
        : __OR RASRAS(RAS_BW, RAS_GR8)
        : rop_reduce_bw_gr8(rin, rout, x1, y1, x2, y2, newx, newy, factor);
    return;

    CASE RASRAS(RAS_GR8, RAS_GR8)
        : rop_reduce_gr8(rin, rout, x1, y1, x2, y2, newx, newy, factor);
    return;

    CASE RASRAS(RAS_CM16, RAS_CM16)
        : rop_reduce_cm16(rin, rout, x1, y1, x2, y2, newx, newy, factor);
    return;

    CASE RASRAS(RAS_RGB_, RAS_RGB_)
        : __OR RASRAS(RAS_RGB_, RAS_RGBM)
        : __OR RASRAS(RAS_RGBM, RAS_RGB_)
        : __OR RASRAS(RAS_RGBM, RAS_RGBM)
        : rop_reduce_rgbm(rin, rout, x1, y1, x2, y2, newx, newy, factor);
    return;

  DEFAULT:
    D 52 printf(
        "### INTERNAL ERROR - rop_reduce; invalid raster combination\n");
    E 52
I 52 assert(!"rop_reduce; invalid raster combination");
    E 52
I 28
  }
}

/*---------------------------------------------------------------------------*/
E 84
I 39
    /* copia un rettangolo da rin a rout.
* Le coordinate sono relative ai due raster: (0, 0) corrisponde
* al primo pixel del raster.
* Riduce le dimensioni di un fattore di subsampling pari a shrink.
* I due raster devono essere dello stesso tipo.
*/

    void
    rop_shrink(RASTER *rin, RASTER *rout, int x1, int y1, int x2, int y2,
               int newx, int newy, int shrink) {
  UCHAR *curr_pixin, *bufferin;
  int i, j, tmp, bpp;
  int xsize, ysize, wrapin, wrapout;
  UCHAR *curr_pixout, *bufferout;
  I 84 USHORT *curr_pixin_2, *curr_pixout_2;
  ULONG *curr_pixin_4, *curr_pixout_4;
  E 84

      if (shrink == 1) return;

  /* raddrizzo gli estremi */
  if (x1 > x2) {
    tmp = x1;
    x1  = x2;
    x2  = tmp;
  }
  if (y1 > y2) {
    tmp = y1;
    y1  = y2;
    y2  = tmp;
  }
  E 39

I 39
      /* controllo gli sconfinamenti */

      if (x1 < 0 || y1 < 0 || x2 >= rin->lx || y2 >= rin->ly || newx < 0 ||
          newy < 0 || newx + (x2 - x1 + shrink) / shrink > rout->lx ||
          newy + (y2 - y1 + shrink) / shrink > rout->ly ||
          rin->type != rout->type) {
    printf("### INTERNAL ERROR - rop_shrink; access violation\n");
    return;
  }

  bpp = rop_pixbytes(rin->type);

  xsize = (x2 - x1 + shrink) / shrink;
  ysize = (y2 - y1 + shrink) / shrink;

  wrapin = rin->wrap * bpp;

  bufferin = (UCHAR *)(rin->buffer) + wrapin * y1 + x1 * bpp;

  wrapout   = rout->wrap * bpp;
  bufferout = (UCHAR *)(rout->buffer) + wrapout * newy + newx * bpp;

  wrapin *= shrink;
  D 84 shrink *= bpp;
  E 84

      for (i = 0; i < ysize; i++) {
    curr_pixin  = bufferin;
    curr_pixout = bufferout;
    bufferin += wrapin;
    bufferout += wrapout;

    D 84 for (j = 0; j < xsize; j++)
E 84
I 84 switch (bpp)
E 84 {
      D 84 memcpy(curr_pixout, curr_pixin, bpp);
      curr_pixin += shrink;
      curr_pixout += bpp;
    }
  }
}

/*---------------------------------------------------------------------------*/
E 39

    /* copia un rettangolo da rin a rout.
* Le coordinate sono relative ai due raster: (0, 0) corrisponde
* al primo pixel del raster.
* Riduce le dimensioni di una potenza di due facendo la media dei valori
D 29
* dei pixel, ma solo su circa 3/4 dei pixel.
E 29
I 29
* dei pixel, ma solo su una parte dei pixel.
E 29
*/

    void
    rop_zoom_out(RASTER *rin, RASTER *rout, int x1, int y1, int x2, int y2,
                 int newx, int newy, int abs_zoom_level) {
  int tmp, newlx, newly;
  int rasras, factor, abszl;

  abszl  = abs_zoom_level;
  factor = 1 << abs_zoom_level;
  if (factor == 1) rop_copy(rin, rout, x1, y1, x2, y2, newx, newy);

  /* raddrizzo gli estremi */
  if (x1 > x2) {
    tmp = x1;
    x1  = x2;
    x2  = tmp;
  }
  if (y1 > y2) {
    tmp = y1;
    y1  = y2;
    y2  = tmp;
  }

  /* controllo gli sconfinamenti */
  newlx = (x2 - x1 + factor) / factor;
  newly = (y2 - y1 + factor) / factor;
  if (x1 < 0 || y1 < 0 || x2 >= rin->lx || y2 >= rin->ly || newx < 0 ||
      newy < 0 || newx + newlx > rout->lx || newy + newly > rout->ly)
    D 30 {
      printf("### INTERNAL ERROR - rop_zoom_out; access violation\n");
      return;
    }
  E 30
I 30 {
    printf(
        "### INTERNAL ERROR - rop_zoom_out; access violation\n"
        " ((%d,%d)(%d,%d)in(%dx%d)->(%d,%d)in(%dx%d))\n",
        x1, y1, x2, y2, rin->lx, rin->ly, newx, newy, rout->lx, rout->ly);
    return;
  }
  E 30

      rasras = RASRAS(rin->type, rout->type);
  switch (rasras) {
    CASE RASRAS(RAS_GR8, RAS_RGB_)
        : __OR RASRAS(RAS_GR8, RAS_RGBM)
        : rop_zoom_out_gr8_rgbm(rin, rout, x1, y1, x2, y2, newx, newy, abszl);
    D 57 return;
    E 57

        CASE RASRAS(RAS_CM16, RAS_RGB_)
        :
D 46 rop_zoom_out_cm16_rgb_(rin, rout, x1, y1, x2, y2, newx, newy, abszl);
    E 46
I 46 rop_zoom_out_cm16_rgbx(rin, rout, x1, y1, x2, y2, newx, newy, abszl);
    E 46
D 57 return;
    E 57

        CASE RASRAS(RAS_CM16, RAS_RGBM)
        : rop_zoom_out_cm16_rgbm(rin, rout, x1, y1, x2, y2, newx, newy, abszl);
    D 57 return;
    E 57

I 81 CASE RASRAS(RAS_RGB, RAS_RGB16)
        : rop_zoom_out_rgb_rgb16(rin, rout, x1, y1, x2, y2, newx, newy, abszl);

    E 81 CASE RASRAS(RAS_RGB, RAS_RGB_)
        : __OR RASRAS(RAS_RGB, RAS_RGBM)
        : rop_zoom_out_rgb_rgbm(rin, rout, x1, y1, x2, y2, newx, newy, abszl);
    D 57 return;
    E 57

        CASE RASRAS(RAS_BW, RAS_CM16)
        : __OR RASRAS(RAS_WB, RAS_CM16)
        : rop_zoom_out_bw_cm16(rin, rout, x1, y1, x2, y2, newx, newy, abszl);
    D 57 return;
    E 57

        CASE RASRAS(RAS_BW, RAS_RGB_)
        : __OR RASRAS(RAS_BW, RAS_RGBM)
        : __OR RASRAS(RAS_WB, RAS_RGB_)
        : __OR RASRAS(RAS_WB, RAS_RGBM)
        : rop_zoom_out_bw_rgbm(rin, rout, x1, y1, x2, y2, newx, newy, abszl);
    D 57 return;
    E 57

        CASE RASRAS(RAS_GR8, RAS_CM16)
        : rop_zoom_out_gr8_cm16(rin, rout, x1, y1, x2, y2, newx, newy, abszl);
    D 57 return;
    E 57

        CASE RASRAS(RAS_WB, RAS_GR8)
        : __OR RASRAS(RAS_BW, RAS_GR8)
        : rop_zoom_out_bw_gr8(rin, rout, x1, y1, x2, y2, newx, newy, abszl);
    D 57 return;
    E 57

        CASE RASRAS(RAS_GR8, RAS_GR8)
        : rop_zoom_out_gr8(rin, rout, x1, y1, x2, y2, newx, newy, abszl);
    D 57 return;
    E 57

        CASE RASRAS(RAS_CM16, RAS_CM16)
        : rop_zoom_out_cm16(rin, rout, x1, y1, x2, y2, newx, newy, abszl);
    D 57 return;
    E 57

        CASE RASRAS(RAS_RGB_, RAS_RGB_)
        : __OR RASRAS(RAS_RGB_, RAS_RGBM)
        : __OR RASRAS(RAS_RGBM, RAS_RGB_)
        :
D 46 __OR RASRAS(RAS_RGBM, RAS_RGBM)
        :
E 46
I 46 rop_zoom_out_rgbx(rin, rout, x1, y1, x2, y2, newx, newy, abszl);
    D 57 return;
    E 57

        CASE RASRAS(RAS_RGBM, RAS_RGBM)
        :
E 46 rop_zoom_out_rgbm(rin, rout, x1, y1, x2, y2, newx, newy, abszl);
    D 57 return;
    E 57
I 54

I 61 CASE RASRAS(RAS_BW, RAS_RGB16)
        : __OR RASRAS(RAS_WB, RAS_RGB16)
        : rop_zoom_out_bw_rgb16(rin, rout, x1, y1, x2, y2, newx, newy, abszl);

    E 61 CASE RASRAS(RAS_GR8, RAS_RGB16)
        : rop_zoom_out_gr8_rgb16(rin, rout, x1, y1, x2, y2, newx, newy, abszl);
    E 54

I 52 CASE RASRAS(RAS_CM16, RAS_RGB16)
        : rop_zoom_out_cm16_rgb16(rin, rout, x1, y1, x2, y2, newx, newy, abszl);

    CASE RASRAS(RAS_RGB_, RAS_RGB16)
        : __OR RASRAS(RAS_RGBM, RAS_RGB16)
        : rop_zoom_out_rgbx_rgb16(rin, rout, x1, y1, x2, y2, newx, newy, abszl);

    E 52 DEFAULT :
D 52 printf(
        "### INTERNAL ERROR - rop_zoom_out; invalid raster combination\n");
    E 52
I 52 assert(!"rop_zoom_out; invalid raster combination");
    E 52
E 28
E 20
E 10
  }
}

/*---------------------------------------------------------------------------*/

D 14
/*   TOPPA TEMPORANEA PER INPUT, rifare completamente
 *
 *   chiedere a Walter
E 14
I 14
/* copia un rettangolo da rin a rout,
 * specchiandolo orizzontalmente se mirror e' dispari,
 * e poi ruotandolo del multiplo di novanta gradi specificato
 * da ninety in senso antiorario
E 14
 *
 */

I 14
D 17
/* RIVEDERE COMPLETAMENTE, PER ADESSO FUNZIA SOLO PER BW->BW e GR8->GR8 */
E 17
I 17
D 23
/* RIVEDERE COMPLETAMENTE, PER ADESSO FUNZIA SOLO PER POCHI TIPI DI RASTER */
E 17

E 23
E 14
void rop_copy_90(RASTER *rin, RASTER *rout,
                 int x1, int y1, int x2, int y2, int newx, int newy,
D 14
		 int ninety, int flip)
E 14
I 14
D 15
		 int mirror, int ninety)
E 15
I 15
                 int mirror, int ninety)
E 15
E 14
{
  D 13 char *rowin, *rowout;
  E 13
I 13
D 23 UCHAR *rowin, *rowout;
  E 13
D 10 int rowsize, wrapin, wrapout, d, tmp;
  E 10
I 10
D 14 int rowsize, bytewrapin, bytewrapout, d, tmp;
  E 14
I 14 int rowsize, bytewrapin, bytewrapout, d, tmp, newdx, newdy;
  E 14
E 10
D 12 int pixbits_in, pixbits_out;
  E 12
I 12 int pixbits_in, pixbits_out;
  int pixbytes_in, pixbytes_out;
  E 23
I 23 int newdx, newdy, tmp;
  E 23
E 12 int rasras;

  D 14 if (ninety != 1 || flip != 1) {
    printf("### INTERNAL ERROR - rop_copy_90; ?????????????????\n");
    return;
  }
  E 14
I 14 mirror &= 1;
  ninety &= 3;
  I 18

      if (!ninety && !mirror) {
    rop_copy(rin, rout, x1, y1, x2, y2, newx, newy);
    return;
  }
  E 18
E 14

      /* raddrizzo gli estremi */
      if (x1 > x2) {
    tmp = x1;
    x1  = x2;
    x2  = tmp;
  }
  if (y1 > y2) {
    tmp = y1;
    y1  = y2;
    y2  = tmp;
  }

  I 14 switch (ninety) {
    CASE 0 : __OR 2 : newdx = x2 - x1 + 1;
    newdy                   = y2 - y1 + 1;
    CASE 1 : __OR 3 : newdx = y2 - y1 + 1;
    newdy                   = x2 - x1 + 1;
    I 50
D 78 DEFAULT : assert(FALSE);
    newdx = newdy = 0;
    E 78
I 78 DEFAULT : abort();
    newdx = newdy = 0;
    E 78
E 50
  }

E 14
/* controllo gli sconfinamenti */
if(x1<0 || y1<0 || x2>=rin->lx || y2>=rin->ly
  || newx<0 || newy<0 || 
D 14
     newx+x2-x1>=rout->lx || newy+y2-y1>=rout->ly)
E 14
I 14
     newx+newdx>rout->lx || newy+newdy>rout->ly)
E 14
{
  printf("### INTERNAL ERROR - rop_copy_90; access violation\n");
  return;
}

D 12 pixbits_in  = rop_pixbits(rin->type);
pixbits_out       = rop_pixbits(rout->type);
E 12
I 12
D 23 pixbits_in = rop_pixbits(rin->type);
pixbytes_in       = rop_pixbytes(rin->type);
pixbits_out       = rop_pixbits(rout->type);
pixbytes_out      = rop_pixbytes(rout->type);
E 23
E 12 rasras     = RASRAS(rin->type, rout->type);
switch (rasras) {
  CASE RASRAS(RAS_WB, RAS_WB)
      : __OR RASRAS(RAS_BW, RAS_BW)
      :
D 14
      /*
rop_copy_90_bw(rin, rout, x1, y1, x2, y2, newx, newy, ninety, flip);
*/
      rop_copy(rin, rout, x1, y1, x2, y2, newx, newy);
  E 14
I 14 rop_copy_90_bw(rin, rout, x1, y1, x2, y2, newx, newy, mirror, ninety);
  E 14 return;

  I 17 CASE RASRAS(RAS_WB, RAS_GR8)
      : __OR RASRAS(RAS_BW, RAS_GR8)
      : rop_copy_90_bw_gr8(rin, rout, x1, y1, x2, y2, newx, newy, mirror,
                           ninety);
  return;

  I 21 CASE RASRAS(RAS_WB, RAS_CM16)
      : __OR RASRAS(RAS_BW, RAS_CM16)
      : rop_copy_90_bw_cm16(rin, rout, x1, y1, x2, y2, newx, newy, mirror,
                            ninety);
  return;

  I 55 CASE RASRAS(RAS_WB, RAS_RGB16)
      : __OR RASRAS(RAS_BW, RAS_RGB16)
      : rop_copy_90_bw_rgb16(rin, rout, x1, y1, x2, y2, newx, newy, mirror,
                             ninety);

  E 55
I 22 CASE RASRAS(RAS_WB, RAS_RGB_)
      : __OR RASRAS(RAS_WB, RAS_RGBM)
      : __OR RASRAS(RAS_BW, RAS_RGB_)
      : __OR RASRAS(RAS_BW, RAS_RGBM)
      : rop_copy_90_bw_rgbm(rin, rout, x1, y1, x2, y2, newx, newy, mirror,
                            ninety);
  return;

  E 22
E 21
E 17 CASE RASRAS(RAS_GR8, RAS_GR8)
      :
D 14
      /*
rop_copy_90_gr8(rin, rout, x1, y1, x2, y2, newx, newy, ninety, flip);
*/
      rop_copy(rin, rout, x1, y1, x2, y2, newx, newy);
  E 14
I 14 rop_copy_90_gr8(rin, rout, x1, y1, x2, y2, newx, newy, mirror, ninety);
  E 14 return;

  I 21 CASE RASRAS(RAS_GR8, RAS_CM16)
      : rop_copy_90_gr8_cm16(rin, rout, x1, y1, x2, y2, newx, newy, mirror,
                             ninety);
  return;

  I 55 CASE RASRAS(RAS_GR8, RAS_RGB16)
      : rop_copy_90_gr8_rgb16(rin, rout, x1, y1, x2, y2, newx, newy, mirror,
                              ninety);
  return;

  E 55
I 22 CASE RASRAS(RAS_GR8, RAS_RGB_)
      : __OR RASRAS(RAS_GR8, RAS_RGBM)
      : rop_copy_90_gr8_rgbm(rin, rout, x1, y1, x2, y2, newx, newy, mirror,
                             ninety);
  return;

  I 23 CASE RASRAS(RAS_CM8, RAS_CM16)
      : rop_copy_90_cm8_cm16(rin, rout, x1, y1, x2, y2, newx, newy, mirror,
                             ninety);
  return;

  I 26 CASE RASRAS(RAS_RGB_, RAS_RGB_)
      : __OR RASRAS(RAS_RGBM, RAS_RGB_)
      : __OR RASRAS(RAS_RGBM, RAS_RGBM)
      : rop_copy_90_rgbm(rin, rout, x1, y1, x2, y2, newx, newy, mirror, ninety);
  return;

  I 63 CASE RASRAS(RAS_RGB, RAS_RGB16)
      : rop_copy_90_rgb_rgb16(rin, rout, x1, y1, x2, y2, newx, newy, mirror,
                              ninety);
  return;

  E 63
I 45 CASE RASRAS(RAS_RGB, RAS_RGB_)
      :
E 45
I 43
D 64 CASE RASRAS(RAS_RGB, RAS_RGBM)
      :
E 64
I 64 __OR RASRAS(RAS_RGB, RAS_RGBM)
      :
E 64 rop_copy_90_rgb_rgbm(rin, rout, x1, y1, x2, y2, newx, newy, mirror,
                            ninety);
  return;

  E 43
E 26
E 23
E 22
E 21 DEFAULT :
D 45 printf("### INTERNAL ERROR - rop_copy_90; ?????????????????\n");
  E 45
I 45
D 78 assert(FALSE);
  E 78
I 78 abort();
  E 78
E 45
    
E 6
}
I 6
D 21

    /* QUI NON CI SI ARRIVA */

E 6
E 4
    /* pixsize compatibili */
    if (pixbits_in != pixbits_out) {
  printf("### INTERNAL ERROR - rop_copy; different pixel size\n");
  return;
}

/* per adesso niente pixel non multipli di 8 bits */
D 2 if (pixbits_in & 7) {
  printf("### INTERNAL ERROR - rop_copy; byte fraction pixels\n");
  E 2
I 2 if (rop_fillerbits(rin->type) || rop_fillerbits(rout->type)) {
    printf("### INTERNAL ERROR - rop_copy; fillerbits not allowed\n");
    E 2 return;
  }
  D 4
  
E 4
I 4

E 4
      /* copia */
      rowsize     = (pixbits_in * (x2 - x1 + 1)) >> 3;
  D 10 wrapin    = (rin->wrap * pixbits_in) >> 3;
  wrapout         = (rout->wrap * pixbits_out) >> 3;
  E 10
I 10 bytewrapin = rin->wrap * pixbytes_in;
  bytewrapout     = rout->wrap * pixbytes_out;
  E 10

D 10 rowin = (char *)rin->buffer + y1 * wrapin + ((x1 * pixbits_in) >> 3);
  rowout = (char *)rout->buffer + newy * wrapout + ((newx * pixbits_out) >> 3);
  E 10
I 10
D 12 rowin = (UCHAR *)rin->buffer + y1 * wrapin + ((x1 * pixbits_in) >> 3);
  rowout = (UCHAR *)rout->buffer + newy * wrapout + ((newx * pixbits_out) >> 3);
  E 12
I 12 rowin =
      (UCHAR *)rin->buffer + y1 * bytewrapin + ((x1 * pixbits_in) >> 3);
  rowout =
      (UCHAR *)rout->buffer + newy * bytewrapout + ((newx * pixbits_out) >> 3);
  E 12
E 10 d = y2 - y1 + 1;
  while (d-- > 0) {
    memmove(rowout, rowin, rowsize);
    D 10 rowin += wrapin;
    rowout += wrapout;
    E 10
I 10 rowin += bytewrapin;
    rowout += bytewrapout;
    E 10
  }
  E 21
}

/*---------------------------------------------------------------------------*/

I 15
#ifdef VERSIONE_LENTA

E 15
I 6
static void rop_copy_90_bw(RASTER *rin, RASTER *rout,
                           int x1, int y1, int x2, int y2,
			   int newx, int newy,
D 14
		           int ninety, int flip)
E 14
I 14
		           int mirror, int ninety)
E 14
{
  D 14
#ifdef WALTER

      UCHAR *rowin,
      *bytein;
  LPIXEL *cmap;
  int wrapin, wrapout, tmp, cmap_offset, bit, bit_offs, startbit;
  int x, lx, ly;

#define SET_BIT(BYTE, BIT, VAL)                                                \
  {                                                                            \
    if (VAL)                                                                   \
      *BYTE |= (1 << (BIT));                                                   \
    else                                                                       \
      *BYTE &= ~(1 << (BIT));                                                  \
  }
  E 14
I 14 UCHAR *bufin, *bufout;
  int bytewrapin, bytewrapout, bitoffsin, bitoffsout, wrapin;
  int u, v, lu, lv, su, sv, u00, v00, u0, v0, dudx, dudy, dvdx, dvdy;
  int x, y, lx, ly;
  int u1, v1, u2, v2;

  D 61
#define BUFBYTE(X, Y, BUF, BYTEWRAP, BITOFFS)                                  \
  (((UCHAR *)(BUF))[(((X) + (BITOFFS)) >> 3) + (Y) * (BYTEWRAP)])

#define GET_BIT(X, Y, BUF, BYTEWRAP, BITOFFS)                                  \
  ((BUFBYTE(X, Y, BUF, BYTEWRAP, BITOFFS) >> (7 - (((X) + (BITOFFS)) & 7))) & 1)

#define SET_BIT_0(X, Y, BUF, BYTEWRAP, BITOFFS)                                \
  (BUFBYTE(X, Y, BUF, BYTEWRAP, BITOFFS) &=                                    \
   ~(1 << (7 - (((X) + (BITOFFS)) & 7))))

#define SET_BIT_1(X, Y, BUF, BYTEWRAP, BITOFFS)                                \
  (BUFBYTE(X, Y, BUF, BYTEWRAP, BITOFFS) |=                                    \
   (1 << (7 - (((X) + (BITOFFS)) & 7))))

E 61
#ifdef DEBUG
      STW_INIT(0, "rop_copy_90_bw")
E 84
I 84 CASE 1 : for (j = 0; j < xsize; j++) {
    *curr_pixout = *curr_pixin;
    curr_pixin += shrink;
    curr_pixout++;
  }
  CASE 2 : curr_pixin_2 = (USHORT *)curr_pixin;
  curr_pixout_2         = (USHORT *)curr_pixout;
  for (j = 0; j < xsize; j++) {
    *curr_pixout_2 = *curr_pixin_2;
    curr_pixin_2 += shrink;
    curr_pixout_2++;
  }
  CASE 4 : curr_pixin_4 = (ULONG *)curr_pixin;
  curr_pixout_4         = (ULONG *)curr_pixout;
  for (j = 0; j < xsize; j++) {
    *curr_pixout_4 = *curr_pixin_4;
    E 84
I 84 curr_pixin_4 += shrink;
    curr_pixout_4++;
  }
DEFAULT:
  for (j = 0; j < xsize; j++) {
    memcpy(curr_pixout, curr_pixin, bpp);
    curr_pixin += shrink * bpp;
    curr_pixout += bpp;
  }
}
}
}

/*---------------------------------------------------------------------------*/

/* copia un rettangolo da rin a rout.
 * Le coordinate sono relative ai due raster: (0, 0) corrisponde
 * al primo pixel del raster.
 * Riduce le dimensioni di una potenza di due facendo la media dei valori
 * dei pixel, ma solo su una parte dei pixel.
 */

void rop_zoom_out(RASTER *rin, RASTER *rout, int x1, int y1, int x2, int y2,
                  int newx, int newy, int abs_zoom_level) {
  int tmp, newlx, newly;
  int rasras, factor, abszl;

  abszl  = abs_zoom_level;
  factor = 1 << abs_zoom_level;
  if (factor == 1) rop_copy(rin, rout, x1, y1, x2, y2, newx, newy);

  /* raddrizzo gli estremi */
  if (x1 > x2) {
    tmp = x1;
    x1  = x2;
    x2  = tmp;
  }
  if (y1 > y2) {
    tmp = y1;
    y1  = y2;
    y2  = tmp;
  }

  /* controllo gli sconfinamenti */
  newlx = (x2 - x1 + factor) / factor;
  newly = (y2 - y1 + factor) / factor;
  E 89
I 89 __OR RASRAS(RAS_CM24, RAS_CM8S8)
      : __OR RASRAS(RAS_CM24, RAS_CM8S4)
      : __OR RASRAS(RAS_CM24, RAS_BW)
      : rop_copy_quantize(rasras, rin, rout, x1, y1, x2, y2, newx, newy);

DEFAULT:
  assert(!"rop_copy; invalid raster combination");
  E 96
I 96 rasras = RASRAS(rin->type, rout->type);
  switch (rasras) {
    CASE RASRAS(RAS_RGBM, RAS_RGB_)
        : rop_copy_same(rin, rout, x1, y1, x2, y2, newx, newy);

    CASE RASRAS(RAS_GR8, RAS_RGB_)
        : __OR RASRAS(RAS_GR8, RAS_RGBM)
        : rop_copy_gr8_rgbm(rin, rout, x1, y1, x2, y2, newx, newy);

    CASE RASRAS(RAS_CM16, RAS_RGB_)
        : __OR RASRAS(RAS_CM16, RAS_RGBM)
        : rop_copy_cm16_rgbm(rin, rout, x1, y1, x2, y2, newx, newy);

    CASE RASRAS(RAS_CM24, RAS_RGB_)
        : __OR RASRAS(RAS_CM24, RAS_RGBM)
        : rop_copy_cm24_rgbm(rin, rout, x1, y1, x2, y2, newx, newy);

    CASE RASRAS(RAS_CM16, RAS_RGB_64)
        : __OR RASRAS(RAS_CM16, RAS_RGBM64)
        : rop_copy_cm16_rgbm64(rin, rout, x1, y1, x2, y2, newx, newy);

    CASE RASRAS(RAS_CM24, RAS_RGB_64)
        : __OR RASRAS(RAS_CM24, RAS_RGBM64)
        : rop_copy_cm24_rgbm64(rin, rout, x1, y1, x2, y2, newx, newy);

    CASE RASRAS(RAS_RGBM, RAS_RGBM64)
        : rop_copy_rgbm_rgbm64(rin, rout, x1, y1, x2, y2, newx, newy);

    CASE RASRAS(RAS_RGBM64, RAS_RGBM)
        : rop_copy_rgbm64_rgbm(rin, rout, x1, y1, x2, y2, newx, newy);

    CASE RASRAS(RAS_RGB_64, RAS_RGB_)
        : __OR RASRAS(RAS_RGBM64, RAS_RGB_)
        : __OR RASRAS(RAS_RGB_64, RAS_RGBM)
        : rop_copy_rgbx64_rgbx(rin, rout, x1, y1, x2, y2, newx, newy);

    CASE RASRAS(RAS_RGB, RAS_RGB16)
        : rop_copy_rgb_rgb16(rin, rout, x1, y1, x2, y2, newx, newy);

    CASE RASRAS(RAS_RGB, RAS_RGB_)
        : __OR RASRAS(RAS_RGB, RAS_RGBM)
        : rop_copy_rgb_rgbm(rin, rout, x1, y1, x2, y2, newx, newy);

    D 104 CASE RASRAS(RAS_BW, RAS_BW)
        : __OR RASRAS(RAS_BW, RAS_WB)
        :
E 104
I 104 CASE RASRAS(RAS_BW, RAS_WB)
        :
E 104 __OR RASRAS(RAS_WB, RAS_BW)
        : __OR RASRAS(RAS_WB, RAS_WB)
        : rop_copy_bw(rin, rout, x1, y1, x2, y2, newx, newy);

    CASE RASRAS(RAS_BW, RAS_CM16)
        : __OR RASRAS(RAS_WB, RAS_CM16)
        : rop_copy_bw_cm16(rin, rout, x1, y1, x2, y2, newx, newy);

    CASE RASRAS(RAS_BW, RAS_CM24)
        : __OR RASRAS(RAS_WB, RAS_CM24)
        : rop_copy_bw_cm24(rin, rout, x1, y1, x2, y2, newx, newy);

    CASE RASRAS(RAS_BW, RAS_RGB_)
        : __OR RASRAS(RAS_BW, RAS_RGBM)
        : __OR RASRAS(RAS_WB, RAS_RGB_)
        : __OR RASRAS(RAS_WB, RAS_RGBM)
        : rop_copy_bw_rgbm(rin, rout, x1, y1, x2, y2, newx, newy);

    CASE RASRAS(RAS_GR8, RAS_CM16)
        : rop_copy_gr8_cm16(rin, rout, x1, y1, x2, y2, newx, newy);

    CASE RASRAS(RAS_GR8, RAS_CM24)
        : rop_copy_gr8_cm24(rin, rout, x1, y1, x2, y2, newx, newy);

    CASE RASRAS(RAS_CM8, RAS_CM16)
        : rop_copy_cm8_cm16(rin, rout, x1, y1, x2, y2, newx, newy);

    CASE RASRAS(RAS_CM8, RAS_CM24)
        : rop_copy_cm8_cm24(rin, rout, x1, y1, x2, y2, newx, newy);

    CASE RASRAS(RAS_BW, RAS_RGB16)
        : __OR RASRAS(RAS_WB, RAS_RGB16)
        : rop_copy_bw_rgb16(rin, rout, x1, y1, x2, y2, newx, newy);

    CASE RASRAS(RAS_GR8, RAS_RGB16)
        : rop_copy_gr8_rgb16(rin, rout, x1, y1, x2, y2, newx, newy);

    CASE RASRAS(RAS_CM16, RAS_RGB16)
        : rop_copy_cm16_rgb16(rin, rout, x1, y1, x2, y2, newx, newy);

    CASE RASRAS(RAS_CM24, RAS_RGB16)
        : rop_copy_cm24_rgb16(rin, rout, x1, y1, x2, y2, newx, newy);

    D 97 CASE RASRAS(RAS_CM16, RAS_XRGB1555)
        : rop_copy_cm16_xrgb1555(rin, rout, x1, y1, x2, y2, newx, newy);

    CASE RASRAS(RAS_CM24, RAS_XRGB1555)
        : rop_copy_cm24_xrgb1555(rin, rout, x1, y1, x2, y2, newx, newy);

    E 97 CASE RASRAS(RAS_RGB_, RAS_RGB16)
        : __OR RASRAS(RAS_RGBM, RAS_RGB16)
        : rop_copy_rgbx_rgb16(rin, rout, x1, y1, x2, y2, newx, newy);

    D 97 CASE RASRAS(RAS_RGB_, RAS_XRGB1555)
        : __OR RASRAS(RAS_RGBM, RAS_XRGB1555)
        : rop_copy_rgbx_xrgb1555(rin, rout, x1, y1, x2, y2, newx, newy);

    E 97 CASE RASRAS(RAS_RGB_64, RAS_RGB16)
        : __OR RASRAS(RAS_RGBM64, RAS_RGB16)
        : rop_copy_rgbx64_rgb16(rin, rout, x1, y1, x2, y2, newx, newy);

    D 97 CASE RASRAS(RAS_RGB_64, RAS_XRGB1555)
        : __OR RASRAS(RAS_RGBM64, RAS_XRGB1555)
        : rop_copy_rgbx64_xrgb1555(rin, rout, x1, y1, x2, y2, newx, newy);

    E 97 CASE RASRAS(RAS_RGB16, RAS_RGB_)
        : __OR RASRAS(RAS_RGB16, RAS_RGBM)
        : rop_copy_rgb16_rgbm(rin, rout, x1, y1, x2, y2, newx, newy);

    D 97 CASE RASRAS(RAS_XRGB1555, RAS_RGB_)
        : __OR RASRAS(RAS_XRGB1555, RAS_RGBM)
        : rop_copy_xrgb1555_rgbm(rin, rout, x1, y1, x2, y2, newx, newy);

    E 97 CASE RASRAS(RAS_RGB_, RAS_RGB)
        : __OR RASRAS(RAS_RGBM, RAS_RGB)
        : rop_copy_rgbx_rgb(rin, rout, x1, y1, x2, y2, newx, newy);

    CASE RASRAS(RAS_RGBM, RAS_CM16S8)
        :
I 98 __OR RASRAS(RAS_RGBM, RAS_CM16S12)
        :
E 98 __OR RASRAS(RAS_RGBM, RAS_CM16S4)
        : __OR RASRAS(RAS_RGBM, RAS_MBW16)
        : __OR RASRAS(RAS_RGBM, RAS_GR8)
        : __OR RASRAS(RAS_RGBM64, RAS_GR8)
        : __OR RASRAS(RAS_RGBM, RAS_GR16)
        : __OR RASRAS(RAS_RGBM64, RAS_GR16)
        : __OR RASRAS(RAS_RGBM, RAS_CM8S8)
        : __OR RASRAS(RAS_RGBM, RAS_CM8S4)
        : __OR RASRAS(RAS_RGBM, RAS_BW)
        : __OR RASRAS(RAS_RGB_, RAS_CM16S8)
        : __OR RASRAS(RAS_RGB_, RAS_CM16S4)
        : __OR RASRAS(RAS_RGB_, RAS_MBW16)
        : __OR RASRAS(RAS_RGB_, RAS_GR8)
        : __OR RASRAS(RAS_RGB_, RAS_CM8S8)
        : __OR RASRAS(RAS_RGB_, RAS_CM8S4)
        : __OR RASRAS(RAS_RGB_, RAS_BW)
        : __OR RASRAS(RAS_RGB16, RAS_CM8S8)
        : __OR RASRAS(RAS_RGB16, RAS_BW)
        :
I 98 __OR RASRAS(RAS_CM16, RAS_CM16S12)
        :
E 98 __OR RASRAS(RAS_CM16, RAS_CM16S8)
        : __OR RASRAS(RAS_CM16, RAS_CM16S4)
        : __OR RASRAS(RAS_CM16, RAS_MBW16)
        : __OR RASRAS(RAS_CM16, RAS_GR8)
        : __OR RASRAS(RAS_CM16, RAS_GR16)
        : __OR RASRAS(RAS_CM16, RAS_CM8S8)
        : __OR RASRAS(RAS_CM16, RAS_CM8S4)
        : __OR RASRAS(RAS_CM16, RAS_BW)
        :
I 98 __OR RASRAS(RAS_CM24, RAS_CM16S12)
        :
E 98 __OR RASRAS(RAS_CM24, RAS_CM16S8)
        : __OR RASRAS(RAS_CM24, RAS_CM16S4)
        : __OR RASRAS(RAS_CM24, RAS_MBW16)
        : __OR RASRAS(RAS_CM24, RAS_GR8)
        :
I 103 __OR RASRAS(RAS_CM24, RAS_GR16)
        :  
E 103 __OR RASRAS(RAS_CM24, RAS_CM8S8)
        : __OR RASRAS(RAS_CM24, RAS_CM8S4)
        : __OR RASRAS(RAS_CM24, RAS_BW)
        :
I 97 __OR RASRAS(RAS_CM8S8, RAS_RGBM)
        : __OR RASRAS(RAS_CM8S8, RAS_RGB_)
        :
I 101 __OR RASRAS(RAS_CM16S12, RAS_RGBM)
        : __OR RASRAS(RAS_CM16S12, RAS_RGB_)
        :
E 101 __OR RASRAS(RAS_RLEBW, RAS_RGB_)
        : __OR RASRAS(RAS_RLEBW, RAS_RGBM)
        :

E 97 rop_copy_quantize(rasras, rin, rout, x1, y1, x2, y2, newx, newy);

  DEFAULT:
    assert(!"rop_copy; invalid raster combination");
  }
  E 96
}
if (rout->extra_mask)
  if (!(rin->type == RAS_CM24 || rout->type == RAS_CM24))
    rop_copy_extra(rin, rout, x1, y1, x2, y2, newx, newy);
/* else e' gia' stato fatto dalle copy particolari */
}

/*---------------------------------------------------------------------------*/
/* copia un rettangolo da rin a rout.
 * Le coordinate sono relative ai due raster: (0, 0) corrisponde
 * al primo pixel del raster.
 * Riduce le dimensioni di un fattore di subsampling pari a shrink.
 * I due raster devono essere dello stesso tipo.
 */

D 94 void rop_shrink(RASTER *rin, RASTER *rout, int x1, int y1, int x2, int y2,
                      int newx, int newy, int shrink) {
  E 94
I 94 void rop_shrink(RASTER * rin, RASTER * rout, int x1, int y1, int x2,
                       int y2, int newx, int newy, int shrink) {
    E 94 UCHAR *curr_pixin, *bufferin;
    int i, j, tmp, bpp;
    int xsize, ysize, wrapin, wrapout;
    UCHAR *curr_pixout, *bufferout;
    USHORT *curr_pixin_2, *curr_pixout_2;
    ULONG *curr_pixin_4, *curr_pixout_4;

    if (shrink == 1) {
      rop_copy(rin, rout, x1, y1, x2, y2, newx, newy);
      return;
    }
    /* raddrizzo gli estremi */
    if (x1 > x2) {
      tmp = x1;
      x1  = x2;
      x2  = tmp;
    }
    if (y1 > y2) {
      tmp = y1;
      y1  = y2;
      y2  = tmp;
    }

    /* controllo gli sconfinamenti */

    if (x1 < 0 || y1 < 0 || x2 >= rin->lx || y2 >= rin->ly || newx < 0 ||
        newy < 0 || newx + (x2 - x1 + shrink) / shrink > rout->lx ||
        newy + (y2 - y1 + shrink) / shrink > rout->ly ||
        rin->type != rout->type) {
      printf("### INTERNAL ERROR - rop_shrink; access violation\n");
      return;
    }

    bpp = rop_pixbytes(rin->type);

    xsize = (x2 - x1 + shrink) / shrink;
    ysize = (y2 - y1 + shrink) / shrink;

    wrapin = rin->wrap * bpp;

    bufferin = (UCHAR *)(rin->buffer) + wrapin * y1 + x1 * bpp;

    wrapout   = rout->wrap * bpp;
    bufferout = (UCHAR *)(rout->buffer) + wrapout * newy + newx * bpp;

    wrapin *= shrink;

    for (i = 0; i < ysize; i++) {
      curr_pixin  = bufferin;
      curr_pixout = bufferout;
      bufferin += wrapin;
      bufferout += wrapout;

      switch (bpp) {
        CASE 1 : for (j = 0; j < xsize; j++) {
          *curr_pixout = *curr_pixin;
          curr_pixin += shrink;
          curr_pixout++;
        }
        CASE 2 : curr_pixin_2 = (USHORT *)curr_pixin;
        curr_pixout_2         = (USHORT *)curr_pixout;
        for (j = 0; j < xsize; j++) {
          *curr_pixout_2 = *curr_pixin_2;
          curr_pixin_2 += shrink;
          curr_pixout_2++;
        }
        CASE 4 : curr_pixin_4 = (ULONG *)curr_pixin;
        curr_pixout_4         = (ULONG *)curr_pixout;
        for (j = 0; j < xsize; j++) {
          *curr_pixout_4 = *curr_pixin_4;
          curr_pixin_4 += shrink;
          curr_pixout_4++;
        }
      DEFAULT:
        for (j = 0; j < xsize; j++) {
          memcpy(curr_pixout, curr_pixin, bpp);
          curr_pixin += shrink * bpp;
          curr_pixout += bpp;
        }
      }
    }
    if (rin->extra_mask && rout->extra_mask &&
        !(rin->type == RAS_CM24 || rout->type == RAS_CM24)) {
      wrapin    = rin->wrap * shrink;
      wrapout   = rout->wrap;
      bufferin  = rin->extra + x1 + y1 * wrapin;
      bufferout = rout->extra + newx + newy * wrapout;
      for (i = 0; i < ysize; i++) {
        curr_pixin  = bufferin;
        curr_pixout = bufferout;
        bufferin += wrapin;
        bufferout += wrapout;
        for (j = 0; j < xsize; j++) {
          *curr_pixout = *curr_pixin;
          curr_pixin += shrink;
          curr_pixout++;
        }
      }
    }
  }

  /*---------------------------------------------------------------------------*/

  /* copia un rettangolo da rin a rout.
* Le coordinate sono relative ai due raster: (0, 0) corrisponde
* al primo pixel del raster.
* Riduce le dimensioni di una potenza di due facendo la media dei valori
* dei pixel, ma solo su una parte dei pixel.
*/

  void rop_zoom_out(RASTER * rin, RASTER * rout, int x1, int y1, int x2, int y2,
                    int newx, int newy, int abs_zoom_level) {
    int tmp, newlx, newly;
    int rasras, factor, abszl;

    if (rout->extra_mask) assert(0);

    abszl  = abs_zoom_level;
    factor = 1 << abs_zoom_level;
    if (factor == 1) rop_copy(rin, rout, x1, y1, x2, y2, newx, newy);

    /* raddrizzo gli estremi */
    if (x1 > x2) {
      tmp = x1;
      x1  = x2;
      x2  = tmp;
    }
    if (y1 > y2) {
      tmp = y1;
      y1  = y2;
      y2  = tmp;
    }

    /* controllo gli sconfinamenti */
    newlx = (x2 - x1 + factor) / factor;
    newly = (y2 - y1 + factor) / factor;
    E 89 if (x1 < 0 || y1 < 0 || x2 >= rin->lx || y2 >= rin->ly || newx < 0 ||
              newy < 0 || newx + newlx > rout->lx || newy + newly > rout->ly) {
      printf(
          "### INTERNAL ERROR - rop_zoom_out; access violation\n"
          " ((%d,%d)(%d,%d)in(%dx%d)->(%d,%d)in(%dx%d))\n",
          x1, y1, x2, y2, rin->lx, rin->ly, newx, newy, rout->lx, rout->ly);
      return;
    }

    rasras = RASRAS(rin->type, rout->type);
    switch (rasras) {
      CASE RASRAS(RAS_GR8, RAS_RGB_)
          : __OR RASRAS(RAS_GR8, RAS_RGBM)
          : rop_zoom_out_gr8_rgbm(rin, rout, x1, y1, x2, y2, newx, newy, abszl);

      CASE RASRAS(RAS_CM16, RAS_RGB_)
          : rop_zoom_out_cm16_rgbx(rin, rout, x1, y1, x2, y2, newx, newy,
                                   abszl);

      CASE RASRAS(RAS_CM24, RAS_RGB_)
          : rop_zoom_out_cm24_rgbx(rin, rout, x1, y1, x2, y2, newx, newy,
                                   abszl);

      CASE RASRAS(RAS_CM16, RAS_RGBM)
          : rop_zoom_out_cm16_rgbm(rin, rout, x1, y1, x2, y2, newx, newy,
                                   abszl);

      CASE RASRAS(RAS_CM24, RAS_RGBM)
          : rop_zoom_out_cm24_rgbm(rin, rout, x1, y1, x2, y2, newx, newy,
                                   abszl);

      CASE RASRAS(RAS_RGB, RAS_RGB16)
          : rop_zoom_out_rgb_rgb16(rin, rout, x1, y1, x2, y2, newx, newy,
                                   abszl);

      CASE RASRAS(RAS_RGB, RAS_RGB_)
          : __OR RASRAS(RAS_RGB, RAS_RGBM)
          : rop_zoom_out_rgb_rgbm(rin, rout, x1, y1, x2, y2, newx, newy, abszl);

      CASE RASRAS(RAS_BW, RAS_CM16)
          : __OR RASRAS(RAS_WB, RAS_CM16)
          : rop_zoom_out_bw_cm16(rin, rout, x1, y1, x2, y2, newx, newy, abszl);

      CASE RASRAS(RAS_BW, RAS_CM24)
          : __OR RASRAS(RAS_WB, RAS_CM24)
          : rop_zoom_out_bw_cm24(rin, rout, x1, y1, x2, y2, newx, newy, abszl);

      CASE RASRAS(RAS_BW, RAS_RGB_)
          : __OR RASRAS(RAS_BW, RAS_RGBM)
          : __OR RASRAS(RAS_WB, RAS_RGB_)
          : __OR RASRAS(RAS_WB, RAS_RGBM)
          : rop_zoom_out_bw_rgbm(rin, rout, x1, y1, x2, y2, newx, newy, abszl);

      CASE RASRAS(RAS_GR8, RAS_CM16)
          : rop_zoom_out_gr8_cm16(rin, rout, x1, y1, x2, y2, newx, newy, abszl);

      CASE RASRAS(RAS_GR8, RAS_CM24)
          : rop_zoom_out_gr8_cm24(rin, rout, x1, y1, x2, y2, newx, newy, abszl);

      CASE RASRAS(RAS_WB, RAS_GR8)
          : __OR RASRAS(RAS_BW, RAS_GR8)
          : rop_zoom_out_bw_gr8(rin, rout, x1, y1, x2, y2, newx, newy, abszl);

      CASE RASRAS(RAS_GR8, RAS_GR8)
          : rop_zoom_out_gr8(rin, rout, x1, y1, x2, y2, newx, newy, abszl);

      CASE RASRAS(RAS_CM16, RAS_CM16)
          : rop_zoom_out_cm16(rin, rout, x1, y1, x2, y2, newx, newy, abszl);

      CASE RASRAS(RAS_CM16, RAS_CM24)
          : rop_zoom_out_cm24(rin, rout, x1, y1, x2, y2, newx, newy, abszl);

      CASE RASRAS(RAS_RGB_, RAS_RGB_)
          : __OR RASRAS(RAS_RGB_, RAS_RGBM)
          : __OR RASRAS(RAS_RGBM, RAS_RGB_)
          : rop_zoom_out_rgbx(rin, rout, x1, y1, x2, y2, newx, newy, abszl);

      CASE RASRAS(RAS_RGBM, RAS_RGBM)
          : rop_zoom_out_rgbm(rin, rout, x1, y1, x2, y2, newx, newy, abszl);

      CASE RASRAS(RAS_BW, RAS_RGB16)
          : __OR RASRAS(RAS_WB, RAS_RGB16)
          : rop_zoom_out_bw_rgb16(rin, rout, x1, y1, x2, y2, newx, newy, abszl);

      CASE RASRAS(RAS_GR8, RAS_RGB16)
          : rop_zoom_out_gr8_rgb16(rin, rout, x1, y1, x2, y2, newx, newy,
                                   abszl);

      CASE RASRAS(RAS_CM16, RAS_RGB16)
          : rop_zoom_out_cm16_rgb16(rin, rout, x1, y1, x2, y2, newx, newy,
                                    abszl);

      CASE RASRAS(RAS_CM24, RAS_RGB16)
          : rop_zoom_out_cm24_rgb16(rin, rout, x1, y1, x2, y2, newx, newy,
                                    abszl);

      CASE RASRAS(RAS_RGB_, RAS_RGB16)
          : __OR RASRAS(RAS_RGBM, RAS_RGB16)
          : rop_zoom_out_rgbx_rgb16(rin, rout, x1, y1, x2, y2, newx, newy,
                                    abszl);

      I 95 CASE RASRAS(RAS_RGBM64, RAS_RGB_)
          : __OR RASRAS(RAS_RGB_64, RAS_RGB_)
          : __OR RASRAS(RAS_RGB_64, RAS_RGBM)
          : rop_zoom_out_rgbx64_rgbx(rin, rout, x1, y1, x2, y2, newx, newy,
                                     abszl);

      CASE RASRAS(RAS_RGBM64, RAS_RGB16)
          : __OR RASRAS(RAS_RGB_64, RAS_RGB16)
          : rop_zoom_out_rgbx64_rgb16(rin, rout, x1, y1, x2, y2, newx, newy,
                                      abszl);

      CASE RASRAS(RAS_RGBM64, RAS_RGBM)
          : rop_zoom_out_rgbm64_rgbm(rin, rout, x1, y1, x2, y2, newx, newy,
                                     abszl);

      E 95 DEFAULT : assert(!"rop_zoom_out; invalid raster combination");
    }
  }

  /*---------------------------------------------------------------------------*/

  /* copia un rettangolo da rin a rout,
* specchiandolo orizzontalmente se mirror e' dispari,
* e poi ruotandolo del multiplo di novanta gradi specificato
* da ninety in senso antiorario
*
*/

  void rop_copy_90(RASTER * rin, RASTER * rout, int x1, int y1, int x2, int y2,
                   int newx, int newy, int mirror, int ninety) {
    int newdx, newdy, tmp;
    int rasras;

    I 89 if (rout->extra_mask) assert(0);

    E 89 mirror &= 1;
    ninety &= 3;

    if (!ninety && !mirror) {
      rop_copy(rin, rout, x1, y1, x2, y2, newx, newy);
      return;
    }

    /* raddrizzo gli estremi */
    if (x1 > x2) {
      tmp = x1;
      x1  = x2;
      x2  = tmp;
    }
    if (y1 > y2) {
      tmp = y1;
      y1  = y2;
      y2  = tmp;
    }

    switch (ninety) {
      CASE 0 : __OR 2 : newdx = x2 - x1 + 1;
      newdy                   = y2 - y1 + 1;
      CASE 1 : __OR 3 : newdx = y2 - y1 + 1;
      newdy                   = x2 - x1 + 1;
    DEFAULT:
      abort();
      newdx = newdy = 0;
    }

    /* controllo gli sconfinamenti */
    if (x1 < 0 || y1 < 0 || x2 >= rin->lx || y2 >= rin->ly || newx < 0 ||
        newy < 0 || newx + newdx > rout->lx || newy + newdy > rout->ly) {
      printf("### INTERNAL ERROR - rop_copy_90; access violation\n");
      return;
    }

    rasras = RASRAS(rin->type, rout->type);
    switch (rasras) {
      CASE RASRAS(RAS_WB, RAS_WB)
          : __OR RASRAS(RAS_BW, RAS_BW)
          : rop_copy_90_bw(rin, rout, x1, y1, x2, y2, newx, newy, mirror,
                           ninety);
      return;

      CASE RASRAS(RAS_WB, RAS_GR8)
          : __OR RASRAS(RAS_BW, RAS_GR8)
          : rop_copy_90_bw_gr8(rin, rout, x1, y1, x2, y2, newx, newy, mirror,
                               ninety);
      return;

      CASE RASRAS(RAS_WB, RAS_CM16)
          : __OR RASRAS(RAS_BW, RAS_CM16)
          : rop_copy_90_bw_cm16(rin, rout, x1, y1, x2, y2, newx, newy, mirror,
                                ninety);
      return;

      CASE RASRAS(RAS_WB, RAS_CM24)
          : __OR RASRAS(RAS_BW, RAS_CM24)
          : rop_copy_90_bw_cm24(rin, rout, x1, y1, x2, y2, newx, newy, mirror,
                                ninety);
      return;

      CASE RASRAS(RAS_WB, RAS_RGB16)
          : __OR RASRAS(RAS_BW, RAS_RGB16)
          : rop_copy_90_bw_rgb16(rin, rout, x1, y1, x2, y2, newx, newy, mirror,
                                 ninety);

      CASE RASRAS(RAS_WB, RAS_RGB_)
          : __OR RASRAS(RAS_WB, RAS_RGBM)
          : __OR RASRAS(RAS_BW, RAS_RGB_)
          : __OR RASRAS(RAS_BW, RAS_RGBM)
          : rop_copy_90_bw_rgbm(rin, rout, x1, y1, x2, y2, newx, newy, mirror,
                                ninety);
      return;

      CASE RASRAS(RAS_GR8, RAS_GR8)
          : rop_copy_90_gr8(rin, rout, x1, y1, x2, y2, newx, newy, mirror,
                            ninety);
      return;

      CASE RASRAS(RAS_GR8, RAS_CM16)
          : rop_copy_90_gr8_cm16(rin, rout, x1, y1, x2, y2, newx, newy, mirror,
                                 ninety);
      return;

      CASE RASRAS(RAS_GR8, RAS_CM24)
          : rop_copy_90_gr8_cm24(rin, rout, x1, y1, x2, y2, newx, newy, mirror,
                                 ninety);
      return;

      CASE RASRAS(RAS_GR8, RAS_RGB16)
          : rop_copy_90_gr8_rgb16(rin, rout, x1, y1, x2, y2, newx, newy, mirror,
                                  ninety);
      return;

      CASE RASRAS(RAS_GR8, RAS_RGB_)
          : __OR RASRAS(RAS_GR8, RAS_RGBM)
          : rop_copy_90_gr8_rgbm(rin, rout, x1, y1, x2, y2, newx, newy, mirror,
                                 ninety);
      return;

      CASE RASRAS(RAS_CM8, RAS_CM16)
          : rop_copy_90_cm8_cm16(rin, rout, x1, y1, x2, y2, newx, newy, mirror,
                                 ninety);
      return;

      CASE RASRAS(RAS_CM8, RAS_CM24)
          : rop_copy_90_cm8_cm24(rin, rout, x1, y1, x2, y2, newx, newy, mirror,
                                 ninety);
      return;

      CASE RASRAS(RAS_RGB_, RAS_RGB_)
          : __OR RASRAS(RAS_RGBM, RAS_RGB_)
          : __OR RASRAS(RAS_RGBM, RAS_RGBM)
          : rop_copy_90_rgbm(rin, rout, x1, y1, x2, y2, newx, newy, mirror,
                             ninety);
      return;

      CASE RASRAS(RAS_RGB, RAS_RGB16)
          : rop_copy_90_rgb_rgb16(rin, rout, x1, y1, x2, y2, newx, newy, mirror,
                                  ninety);
      return;

      CASE RASRAS(RAS_RGB, RAS_RGB_)
          : __OR RASRAS(RAS_RGB, RAS_RGBM)
          : rop_copy_90_rgb_rgbm(rin, rout, x1, y1, x2, y2, newx, newy, mirror,
                                 ninety);
      return;

    DEFAULT:
      abort();
    }
  }

/*---------------------------------------------------------------------------*/

#ifdef VERSIONE_LENTA

  static void rop_copy_90_bw(RASTER * rin, RASTER * rout, int x1, int y1,
                             int x2, int y2, int newx, int newy, int mirror,
                             int ninety) {
    UCHAR *bufin, *bufout;
    int bytewrapin, bytewrapout, bitoffsin, bitoffsout, wrapin;
    int u, v, lu, lv, su, sv, u00, v00, u0, v0, dudx, dudy, dvdx, dvdy;
    int x, y, lx, ly;
    int u1, v1, u2, v2;

#ifdef DEBUG
    STW_INIT(0, "rop_copy_90_bw")
    E 84 STW_START(0)
#endif
E 14

I 14 mirror &= 1;
    ninety &= 3;
    E 14

D 14 if (!ninety && !flip)
E 14
I 14 if (!ninety && !mirror)
E 14 {
      rop_copy(rin, rout, x1, y1, x2, y2, newx, newy);
      return;
    }

    D 14 lx     = x2 - x1 + 1;
    ly           = y2 - y1 + 1;
    D 10 wrapin = rin->wrap;
    wrapout      = rout->wrap;
    bit_offs     = rin->bit_offs;
    E 10
I 10 wrapin    = rin->wrap / 8;
    bit_offs     = rin->bit_offs;
    wrapout      = rout->wrap;
    E 14
I 14 u1        = x1;
    v1           = y1;
    u2           = x2;
    v2           = y2;

    su       = u2 - u1;
    sv       = v2 - v1;
    lu       = u2 - u1 + 1;
    lv       = v2 - v1 + 1;
    E 14
E 10

D 14 rowin = (UCHAR *)rin->buffer + wrapin * y1 + ((x1 + bit_offs) >> 3);
    startbit = 7 - ((x1 + bit_offs) & 7);
    rowout   = (USHORT *)rout->buffer + wrapout * newy + newx;
    while (ly-- > 0)
			E 14
I 14 if (ninety & 1)
E 14 {
        D 14 bytein = rowin;
        bit          = startbit;
        pixout       = rowout;
        for (x = lx; x > 0; x--) {
        outbyte = outx
    SET_BIT (rout->buffer
    *pixout++ = ((*bytein >> bit) & 1) ? i_1 : i_0;
    if (bit==0)
E 14
I 14
  lx = lv;
  ly = lu;
        }
        else {
          lx = lu;
          ly = lv;
        }

        dudx = 0;
        dudy = 0;
        dvdx = 0;
        dvdy = 0;

        switch ((mirror << 2) + ninety) {
          CASE(0 << 2) + 0 : u00 = u1;
          v00                    = v1;
          dudx                   = 1;
          dvdy                   = 1;
          CASE(0 << 2) + 1 : u00 = u1;
          v00                    = v1 + sv;
          dudy                   = 1;
          dvdx                   = -1;
          CASE(0 << 2) + 2 : u00 = u1 + su;
          v00                    = v1 + sv;
          dudx                   = -1;
          dvdy                   = -1;
          CASE(0 << 2) + 3 : u00 = u1 + su;
          v00                    = v1;
          dudy                   = -1;
          dvdx                   = 1;
          CASE(1 << 2) + 0 : u00 = u1 + su;
          v00                    = v1;
          dudx                   = -1;
          dvdy                   = 1;
          CASE(1 << 2) + 1 : u00 = u1 + su;
          v00                    = v1 + sv;
          dudy                   = -1;
          dvdx                   = -1;
          CASE(1 << 2) + 2 : u00 = u1;
          v00                    = v1 + sv;
          dudx                   = 1;
          dvdy                   = -1;
          CASE(1 << 2) + 3 : u00 = u1;
          v00                    = v1;
          dudy                   = 1;
          dvdx                   = 1;
        }

        D 16 bufin       = rin->buffer;
        bytewrapin        = rin->wrap / 8;
        bitoffsin         = rin->bit_offs;
        E 16
I 16 bufin              = rin->buffer;
        D 47 bytewrapin  = (rin->wrap + 7) / 8;
        E 47
I 47 bytewrapin         = (rin->wrap + 7) >> 3;
        E 47 bitoffsin   = rin->bit_offs;
        E 16 bufout      = rout->buffer;
        D 16 bytewrapout = rout->wrap / 8;
        E 16
I 16
D 47 bytewrapout        = (rout->wrap + 7) / 8;
        E 47
I 47 bytewrapout        = (rout->wrap + 7) >> 3;
        E 47
E 16 bitoffsout         = rout->bit_offs;

        if (dudx)
          for (u0 = u00, v0 = v00, y = newy; y < newy + ly; v0 += dvdy, y++)
            for (u = u0, v = v0, x = newx; x < newx + lx; u += dudx, x++)
              E 14 {
                D 14 bytein++;
                bit = 7;
                E 14
I 14 if (GET_BIT(u, v, bufin, bytewrapin, bitoffsin))
                    SET_BIT_1(x, y, bufout, bytewrapout, bitoffsout);
                else SET_BIT_0(x, y, bufout, bytewrapout, bitoffsout);
                E 14
              }
        D 14 else bit--;
      }
    rowin += wrapin;
    rowout += wrapout;
  }
  E 14
I 14 else for (u0 = u00, v0 = v00, y = newy; y < newy + ly;
                 u0 += dudy, y++) for (u = u0, v = v0, x = newx; x < newx + lx;
                                       v += dvdx, x++) {
    if (GET_BIT(u, v, bufin, bytewrapin, bitoffsin))
      SET_BIT_1(x, y, bufout, bytewrapout, bitoffsout);
    else
      SET_BIT_0(x, y, bufout, bytewrapout, bitoffsout);
    I 82
  }
  E 82
D 82
}
E 82
I 15

#ifdef DEBUG
    STW_STOP(0) STW_PRINT(stdout)
#endif
}

#endif

/*---------------------------------------------------------------------------*/

static void rop_copy_90_bw(RASTER *rin, RASTER *rout, int x1, int y1, int x2,
                           int y2, int newx, int newy, int mirror, int ninety) {
  UCHAR *bufin, *bufout;
  UCHAR tmpbyte;
  int bytewrapin, bytewrapout, bitoffsin, bitoffsout, wrapin;
  I 83 int u, v, lu, lv, su, sv, u00, v00, u0, v0, dudx, dudy, dvdx, dvdy;
  int x, y, lx, ly;
  int u1, v1, u2, v2;

#ifdef DEBUG
  STW_INIT(0, "rop_copy_90_bw")
  STW_START(0)
#endif

  mirror &= 1;
  ninety &= 3;

  if (!ninety && !mirror) {
    rop_copy(rin, rout, x1, y1, x2, y2, newx, newy);
    return;
  }

  u1 = x1;
  v1 = y1;
  E 83
D 83 int u, v, lu, lv, su, sv, u00, v00, u0, v0, dudx, dudy, dvdx, dvdy;
  int x, y, lx, ly;
  int u1, v1, u2, v2;

  D 61
#define BUFBYTE(X, Y, BUF, BYTEWRAP, BITOFFS)                                  \
  (((UCHAR *)(BUF))[(((X) + (BITOFFS)) >> 3) + (Y) * (BYTEWRAP)])

#define GET_BIT(X, Y, BUF, BYTEWRAP, BITOFFS)                                  \
  ((BUFBYTE(X, Y, BUF, BYTEWRAP, BITOFFS) >> (7 - (((X) + (BITOFFS)) & 7))) &  \
   (UCHAR)1)

#define SET_BIT_0(X, Y, BUF, BYTEWRAP, BITOFFS)                                \
  (BUFBYTE(X, Y, BUF, BYTEWRAP, BITOFFS) &=                                    \
   ~(1 << (7 - (((X) + (BITOFFS)) & 7))))

#define SET_BIT_1(X, Y, BUF, BYTEWRAP, BITOFFS)                                \
  (BUFBYTE(X, Y, BUF, BYTEWRAP, BITOFFS) |=                                    \
   (1 << (7 - (((X) + (BITOFFS)) & 7))))

#define BITCPY(XO, YO, BUFO, BYTEWRAPO, BITOFFSO, XI, YI, BUFI, BYTEWRAPI,     \
               BITOFFSI)                                                       \
  {                                                                            \
    if (GET_BIT(XI, YI, BUFI, BYTEWRAPI, BITOFFSI))                            \
      SET_BIT_1(XO, YO, BUFO, BYTEWRAPO, BITOFFSO);                            \
    else                                                                       \
      SET_BIT_0(XO, YO, BUFO, BYTEWRAPO, BITOFFSO);                            \
  }

E 61
#ifdef DEBUG
      STW_INIT(0, "rop_copy_90_bw") STW_START(0)
#endif

          mirror &= 1;
  ninety &= 3;

  if (!ninety && !mirror) {
    rop_copy(rin, rout, x1, y1, x2, y2, newx, newy);
    return;
  }

  u1       = x1;
  v1       = y1;
  E 83 u2 = x2;
  v2       = y2;

  su = u2 - u1;
  sv = v2 - v1;
  lu = u2 - u1 + 1;
  lv = v2 - v1 + 1;

  if (ninety & 1) {
    lx = lv;
    ly = lu;
  } else {
    lx = lu;
    ly = lv;
  }

  dudx = 0;
  dudy = 0;
  dvdx = 0;
  dvdy = 0;

  switch ((mirror << 2) + ninety) {
    CASE(0 << 2) + 0 : u00 = u1;
    v00                    = v1;
    dudx                   = 1;
    dvdy                   = 1;
    CASE(0 << 2) + 1 : u00 = u1;
    v00                    = v1 + sv;
    dudy                   = 1;
    dvdx                   = -1;
    CASE(0 << 2) + 2 : u00 = u1 + su;
    v00                    = v1 + sv;
    dudx                   = -1;
    dvdy                   = -1;
    CASE(0 << 2) + 3 : u00 = u1 + su;
    v00                    = v1;
    dudy                   = -1;
    dvdx                   = 1;
    CASE(1 << 2) + 0 : u00 = u1 + su;
    v00                    = v1;
    dudx                   = -1;
    dvdy                   = 1;
    CASE(1 << 2) + 1 : u00 = u1 + su;
    v00                    = v1 + sv;
    dudy                   = -1;
    dvdx                   = -1;
    CASE(1 << 2) + 2 : u00 = u1;
    v00                    = v1 + sv;
    dudx                   = 1;
    dvdy                   = -1;
    CASE(1 << 2) + 3 : u00 = u1;
    v00                    = v1;
    dudy                   = 1;
    dvdx                   = 1;
    I 50
D 78 DEFAULT : assert(FALSE);
    u00 = v00 = dudy = dvdx = 0;
    E 78
I 78 DEFAULT : abort();
    u00 = v00 = dudy = dvdx = 0;
    E 78
E 50
  }

  D 16 bufin       = rin->buffer;
  bytewrapin        = rin->wrap / 8;
  bitoffsin         = rin->bit_offs;
  E 16
I 16 bufin        = rin->buffer;
  D 47 bytewrapin  = (rin->wrap + 7) / 8;
  E 47
I 47 bytewrapin   = (rin->wrap + 7) >> 3;
  E 47 bitoffsin   = rin->bit_offs;
  E 16 bufout      = rout->buffer;
  D 16 bytewrapout = rout->wrap / 8;
  E 16
I 16
D 47 bytewrapout  = (rout->wrap + 7) / 8;
  E 47
I 47 bytewrapout  = (rout->wrap + 7) >> 3;
  E 47
E 16 bitoffsout   = rout->bit_offs;

  if (dudx)
    for (u0 = u00, v0 = v00, y = newy; y < newy + ly; v0 += dvdy, y++) {
      u = u0;
      v = v0;
      x = newx;
      for (; x < newx + lx && ((x + bitoffsout) & 7); u += dudx, x++)
        BITCPY(x, y, bufout, bytewrapout, bitoffsout, u, v, bufin, bytewrapin,
               bitoffsin)
      for (; x < newx + lx - 7; x += 8) {
        tmpbyte = GET_BIT(u, v, bufin, bytewrapin, bitoffsin) << 7;
        u += dudx;
        tmpbyte |= GET_BIT(u, v, bufin, bytewrapin, bitoffsin) << 6;
        u += dudx;
        tmpbyte |= GET_BIT(u, v, bufin, bytewrapin, bitoffsin) << 5;
        u += dudx;
        tmpbyte |= GET_BIT(u, v, bufin, bytewrapin, bitoffsin) << 4;
        u += dudx;
        tmpbyte |= GET_BIT(u, v, bufin, bytewrapin, bitoffsin) << 3;
        u += dudx;
        tmpbyte |= GET_BIT(u, v, bufin, bytewrapin, bitoffsin) << 2;
        u += dudx;
        tmpbyte |= GET_BIT(u, v, bufin, bytewrapin, bitoffsin) << 1;
        u += dudx;
        tmpbyte |= GET_BIT(u, v, bufin, bytewrapin, bitoffsin);
        u += dudx;
        BUFBYTE(x, y, bufout, bytewrapout, bitoffsout) = tmpbyte;
      }
      for (; x < newx + lx; u += dudx, x++)
        BITCPY(x, y, bufout, bytewrapout, bitoffsout, u, v, bufin, bytewrapin,
               bitoffsin)
    }
  else
    for (u0 = u00, v0 = v00, y = newy; y < newy + ly; u0 += dudy, y++) {
      u = u0;
      v = v0;
      x = newx;
      for (; x < newx + lx && ((x + bitoffsout) & 7); v += dvdx, x++)
        BITCPY(x, y, bufout, bytewrapout, bitoffsout, u, v, bufin, bytewrapin,
               bitoffsin)
      for (; x < newx + lx - 7; x += 8) {
        tmpbyte = GET_BIT(u, v, bufin, bytewrapin, bitoffsin) << 7;
        v += dvdx;
        tmpbyte |= GET_BIT(u, v, bufin, bytewrapin, bitoffsin) << 6;
        v += dvdx;
        tmpbyte |= GET_BIT(u, v, bufin, bytewrapin, bitoffsin) << 5;
        v += dvdx;
        tmpbyte |= GET_BIT(u, v, bufin, bytewrapin, bitoffsin) << 4;
        v += dvdx;
        tmpbyte |= GET_BIT(u, v, bufin, bytewrapin, bitoffsin) << 3;
        v += dvdx;
        tmpbyte |= GET_BIT(u, v, bufin, bytewrapin, bitoffsin) << 2;
        v += dvdx;
        tmpbyte |= GET_BIT(u, v, bufin, bytewrapin, bitoffsin) << 1;
        v += dvdx;
        tmpbyte |= GET_BIT(u, v, bufin, bytewrapin, bitoffsin);
        v += dvdx;
        BUFBYTE(x, y, bufout, bytewrapout, bitoffsout) = tmpbyte;
      }
      for (; x < newx + lx; v += dvdx, x++)
        BITCPY(x, y, bufout, bytewrapout, bitoffsout, u, v, bufin, bytewrapin,
               bitoffsin)
    }
  E 15

#ifdef DEBUG
      STW_STOP(0) STW_PRINT(stdout)
#endif
}

/*---------------------------------------------------------------------------*/
E 14

I 14
#ifdef VERSIONE_LENTA

    static void
    rop_copy_90_gr8(RASTER *rin, RASTER *rout, int x1, int y1, int x2, int y2,
                    int newx, int newy, int mirror, int ninety) {
  UCHAR *bufin, *bufout;
  int wrapin, wrapout;
  int u, v, lu, lv, su, sv, u00, v00, u0, v0, dudx, dudy, dvdx, dvdy;
  int x, y, lx, ly;
  int u1, v1, u2, v2;

#ifdef DEBUG
  STW_INIT(0, "rop_copy_90_gr8")
  STW_START(0)
#endif
E 14

D 14
#else
E 14
I 14 mirror &= 1;
  ninety &= 3;
  E 14

D 14 if (!ninety && !flip)
E 14
I 14 if (!ninety && !mirror)
E 14 {
    rop_copy(rin, rout, x1, y1, x2, y2, newx, newy);
    return;
  }

  I 14 u1 = x1;
  v1       = y1;
  u2       = x2;
  v2       = y2;

  su = u2 - u1;
  sv = v2 - v1;
  lu = u2 - u1 + 1;
  lv = v2 - v1 + 1;

  if (ninety & 1) {
    lx = lv;
    ly = lu;
  } else {
    lx = lu;
    ly = lv;
  }

  dudx = 0;
  dudy = 0;
  dvdx = 0;
  dvdy = 0;

  switch ((mirror << 2) + ninety) {
    CASE(0 << 2) + 0 : u00 = u1;
    v00                    = v1;
    dudx                   = 1;
    dvdy                   = 1;
    CASE(0 << 2) + 1 : u00 = u1;
    v00                    = v1 + sv;
    dudy                   = 1;
    dvdx                   = -1;
    CASE(0 << 2) + 2 : u00 = u1 + su;
    v00                    = v1 + sv;
    dudx                   = -1;
    dvdy                   = -1;
    CASE(0 << 2) + 3 : u00 = u1 + su;
    v00                    = v1;
    dudy                   = -1;
    dvdx                   = 1;
    CASE(1 << 2) + 0 : u00 = u1 + su;
    v00                    = v1;
    dudx                   = -1;
    dvdy                   = 1;
    CASE(1 << 2) + 1 : u00 = u1 + su;
    v00                    = v1 + sv;
    dudy                   = -1;
    dvdx                   = -1;
    CASE(1 << 2) + 2 : u00 = u1;
    v00                    = v1 + sv;
    dudx                   = 1;
    dvdy                   = -1;
    CASE(1 << 2) + 3 : u00 = u1;
    v00                    = v1;
    dudy                   = 1;
    dvdx                   = 1;
  }

  bufin   = rin->buffer;
  wrapin  = rin->wrap;
  bufout  = rout->buffer;
  wrapout = rout->wrap;

  if (dudx)
    for (u0 = u00, v0 = v00, y = newy; y < newy + ly; v0 += dvdy, y++)
      for (u = u0, v = v0, x = newx; x < newx + lx; u += dudx, x++) {
        bufout[x + y * wrapout] = bufin[u + v * wrapin];
      }
  else
    for (u0 = u00, v0 = v00, y = newy; y < newy + ly; u0 += dudy, y++)
      for (u = u0, v = v0, x = newx; x < newx + lx; v += dvdx, x++) {
        bufout[x + y * wrapout] = bufin[u + v * wrapin];
      }

#ifdef DEBUG
  STW_STOP(0)
  STW_PRINT(stdout)
  E 14
#endif
}

I 14
#endif

E 14
/*---------------------------------------------------------------------------*/

static void rop_copy_90_gr8(RASTER *rin, RASTER *rout,
                            int x1, int y1, int x2, int y2,
			    int newx, int newy,
D 14
		            int ninety, int flip)
E 14
I 14
		            int mirror, int ninety)
E 14
{
  D 14 if (!ninety && !flip)
E 14
I 14 UCHAR *bufin, *bufout, *bytein, *byteout;
  int wrapin, wrapout;
  int u, v, lu, lv, su, sv, u00, v00, u0, v0, dindx, dudy, dvdy;
  int x, y, lx, ly;
  int u1, v1, u2, v2;

#ifdef DEBUG
  STW_INIT(0, "rop_copy_90_gr8")
  STW_START(0)
#endif

  mirror &= 1;
  ninety &= 3;

  if (!ninety && !mirror)
		E 14 {
      rop_copy(rin, rout, x1, y1, x2, y2, newx, newy);
      return;
    }
  I 14

      u1 = x1;
  v1     = y1;
  u2     = x2;
  v2     = y2;

  su = u2 - u1;
  sv = v2 - v1;
  lu = u2 - u1 + 1;
  lv = v2 - v1 + 1;

  if (ninety & 1) {
    lx = lv;
    ly = lu;
  } else {
    lx = lu;
    ly = lv;
  }

  bufin   = rin->buffer;
  wrapin  = rin->wrap;
  bufout  = rout->buffer;
  wrapout = rout->wrap;

  dudy = 0;
  dvdy = 0;

  switch ((mirror << 2) + ninety) {
    CASE(0 << 2) + 0 : u00       = u1;
    v00                          = v1;
    dvdy                         = 1;
    dindx                        = 1;
    CASE(0 << 2) + 1 : u00       = u1;
    v00                          = v1 + sv;
    dudy                         = 1;
    dindx                        = -wrapin;
    D 61 CASE(0 << 2) + 2 : u00 = u1 + su;
    v00                          = v1 + sv;
    dvdy                         = -1;
    dindx                        = -1;
    CASE(0 << 2) + 3 : u00       = u1 + su;
    v00                          = v1;
    dudy                         = -1;
    dindx                        = wrapin;
    CASE(1 << 2) + 0 : u00       = u1 + su;
    v00                          = v1;
    dvdy                         = 1;
    dindx                        = -1;
    CASE(1 << 2) + 1 : u00       = u1 + su;
    v00                          = v1 + sv;
    dudy                         = -1;
    dindx                        = -wrapin;
    CASE(1 << 2) + 2 : u00       = u1;
    v00                          = v1 + sv;
    dvdy                         = -1;
    dindx                        = 1;
    CASE(1 << 2) + 3 : u00       = u1;
    v00                          = v1;
    dudy                         = 1;
    dindx                        = wrapin;
    I 50 DEFAULT : assert(FALSE);
    u00 = v00 = dudy = dindx = 0;
    E 50
  }

  for (u0 = u00, v0 = v00, y = newy; y < newy + ly;
       u0 += dudy, v0 += dvdy, y++) {
    bytein  = bufin + u0 + v0 * wrapin;
    byteout = bufout + newx + y * wrapout;
    for (x = newx; x < newx + lx; x++) {
      *byteout++ = *bytein;
      bytein += dindx;
    }
  }
  I 17

#ifdef DEBUG
      STW_STOP(0) STW_PRINT(stdout)
#endif
}

/*---------------------------------------------------------------------------*/

I 21 static void rop_copy_90_gr8_cm16(RASTER * rin, RASTER * rout, int x1,
                                       int y1, int x2, int y2, int newx,
                                       int newy, int mirror, int ninety) {
  UCHAR *bufin, *bytein;
  USHORT *bufout, *pixout;
  LPIXEL *cmap;
  int wrapin, wrapout, cmap_offset;
  int u, v, lu, lv, su, sv, u00, v00, u0, v0, dindx, dudy, dvdy;
  int x, y, lx, ly;
  int u1, v1, u2, v2;
  int reversed;

  mirror &= 1;
  ninety &= 3;

  if (!ninety && !mirror) {
    rop_copy(rin, rout, x1, y1, x2, y2, newx, newy);
    return;
  }
  if (!rout->cmap.buffer) {
    msg(MSG_IE, "rop_copy_90_gr8_cm16: missing color map");
    return;
  }
  cmap              = rout->cmap.buffer;
  D 56 cmap_offset = rout->cmap.offset;
  E 56
I 56 cmap_offset  = rout->cmap.info.offset_mask;
  E 56 reversed    = cmap[0].r != 0;

  u1 = x1;
  v1 = y1;
  u2 = x2;
  v2 = y2;

  su = u2 - u1;
  sv = v2 - v1;
  lu = u2 - u1 + 1;
  lv = v2 - v1 + 1;

  if (ninety & 1) {
    lx = lv;
    ly = lu;
  } else {
    lx = lu;
    ly = lv;
  }

  bufin   = rin->buffer;
  wrapin  = rin->wrap;
  bufout  = rout->buffer;
  wrapout = rout->wrap;

  dudy = 0;
  dvdy = 0;

  switch ((mirror << 2) + ninety) {
    CASE(0 << 2) + 0 : u00 = u1;
    v00                    = v1;
    dvdy                   = 1;
    dindx                  = 1;
    CASE(0 << 2) + 1 : u00 = u1;
    v00                    = v1 + sv;
    dudy                   = 1;
    dindx                  = -wrapin;
    CASE(0 << 2) + 2 : u00 = u1 + su;
    v00                    = v1 + sv;
    dvdy                   = -1;
    dindx                  = -1;
    CASE(0 << 2) + 3 : u00 = u1 + su;
    v00                    = v1;
    dudy                   = -1;
    dindx                  = wrapin;
    CASE(1 << 2) + 0 : u00 = u1 + su;
    v00                    = v1;
    dvdy                   = 1;
    dindx                  = -1;
    CASE(1 << 2) + 1 : u00 = u1 + su;
    v00                    = v1 + sv;
    dudy                   = -1;
    dindx                  = -wrapin;
    CASE(1 << 2) + 2 : u00 = u1;
    v00                    = v1 + sv;
    dvdy                   = -1;
    dindx                  = 1;
    CASE(1 << 2) + 3 : u00 = u1;
    v00                    = v1;
    dudy                   = 1;
    dindx                  = wrapin;
    I 50 DEFAULT : assert(FALSE);
    u00 = v00 = dudy = dindx = 0;
    E 50
  }

  for (u0 = u00, v0 = v00, y = newy; y < newy + ly;
       u0 += dudy, v0 += dvdy, y++) {
    bytein = bufin + u0 + v0 * wrapin;
    pixout = bufout + newx + y * wrapout;
    if (reversed)
      for (x = newx; x < newx + lx; x++) {
        *pixout++ = cmap_offset + (255 - *bytein);
        bytein += dindx;
      }
    else
      for (x = newx; x < newx + lx; x++) {
        *pixout++ = cmap_offset + *bytein;
        bytein += dindx;
      }
    I 23
  }
}

/*---------------------------------------------------------------------------*/

static void rop_copy_90_cm8_cm16(RASTER * rin, RASTER * rout, int x1, int y1,
                                 int x2, int y2, int newx, int newy, int mirror,
                                 int ninety) {
  UCHAR *bufin, *pixin;
  USHORT *bufout, *pixout;
  LPIXEL *cmapin, *cmapout;
  int wrapin, wrapout, cmapin_offset, cmapout_offset, cmap_offset_delta;
  int u, v, lu, lv, su, sv, u00, v00, u0, v0, dindx, dudy, dvdy;
  int x, y, lx, ly;
  int u1, v1, u2, v2;

  mirror &= 1;
  ninety &= 3;

  if (!ninety && !mirror) {
    rop_copy(rin, rout, x1, y1, x2, y2, newx, newy);
    return;
  }
  if (!rin->cmap.buffer || !rout->cmap.buffer) {
    msg(MSG_IE, "rop_copy_90_cm8_cm16: missing color map");
    return;
  }
  cmapin                  = rin->cmap.buffer;
  cmapout                 = rout->cmap.buffer;
  D 56 cmapin_offset     = rin->cmap.offset;
  cmapout_offset          = rout->cmap.offset;
  E 56
I 56 cmapin_offset      = rin->cmap.info.offset_mask;
  cmapout_offset          = rout->cmap.info.offset_mask;
  E 56 cmap_offset_delta = cmapout_offset - cmapin_offset;

  u1 = x1;
  v1 = y1;
  u2 = x2;
  v2 = y2;

  su = u2 - u1;
  sv = v2 - v1;
  lu = u2 - u1 + 1;
  lv = v2 - v1 + 1;

  if (ninety & 1) {
    lx = lv;
    ly = lu;
  } else {
    lx = lu;
    ly = lv;
  }

  bufin   = rin->buffer;
  wrapin  = rin->wrap;
  bufout  = rout->buffer;
  wrapout = rout->wrap;

  dudy = 0;
  dvdy = 0;

  switch ((mirror << 2) + ninety) {
    CASE(0 << 2) + 0 : u00 = u1;
    v00                    = v1;
    dvdy                   = 1;
    dindx                  = 1;
    CASE(0 << 2) + 1 : u00 = u1;
    v00                    = v1 + sv;
    dudy                   = 1;
    dindx                  = -wrapin;
    CASE(0 << 2) + 2 : u00 = u1 + su;
    v00                    = v1 + sv;
    dvdy                   = -1;
    dindx                  = -1;
    CASE(0 << 2) + 3 : u00 = u1 + su;
    v00                    = v1;
    dudy                   = -1;
    dindx                  = wrapin;
    CASE(1 << 2) + 0 : u00 = u1 + su;
    v00                    = v1;
    dvdy                   = 1;
    dindx                  = -1;
    CASE(1 << 2) + 1 : u00 = u1 + su;
    v00                    = v1 + sv;
    dudy                   = -1;
    dindx                  = -wrapin;
    CASE(1 << 2) + 2 : u00 = u1;
    v00                    = v1 + sv;
    dvdy                   = -1;
    dindx                  = 1;
    CASE(1 << 2) + 3 : u00 = u1;
    v00                    = v1;
    dudy                   = 1;
    dindx                  = wrapin;
    I 50 DEFAULT : assert(FALSE);
    u00 = v00 = dudy = dindx = 0;
    E 50
  }

  for (u0 = u00, v0 = v00, y = newy; y < newy + ly;
       u0 += dudy, v0 += dvdy, y++) {
    pixin  = bufin + u0 + v0 * wrapin;
    pixout = bufout + newx + y * wrapout;
    for (x = newx; x < newx + lx; x++) {
      *pixout++ = *pixin + cmap_offset_delta;
      pixin += dindx;
    }
    E 23
  }
}

/*---------------------------------------------------------------------------*/

I 22
D 55 static void rop_copy_90_gr8_rgbm(RASTER * rin, RASTER * rout, int x1,
                                        int y1, int x2, int y2, int newx,
                                        int newy, int mirror, int ninety)
E 55
I 55 static void
rop_copy_90_gr8_rgb16(RASTER * rin, RASTER * rout, int x1, int y1, int x2,
                      int y2, int newx, int newy, int mirror, int ninety)
E 55 {
  UCHAR *bufin, *bytein;
  D 55 LPIXEL *bufout, *pixout, tmp;
  E 55
I 55 USHORT *bufout, *pixout;
  E 55 int wrapin, wrapout;
  int u, v, lu, lv, su, sv, u00, v00, u0, v0, dindx, dudy, dvdy;
  int x, y, lx, ly;
  int u1, v1, u2, v2;

  mirror &= 1;
  ninety &= 3;

  if (!ninety && !mirror) {
    rop_copy(rin, rout, x1, y1, x2, y2, newx, newy);
    return;
  }

  u1 = x1;
  v1 = y1;
  u2 = x2;
  v2 = y2;

  su = u2 - u1;
  sv = v2 - v1;
  lu = u2 - u1 + 1;
  lv = v2 - v1 + 1;

  if (ninety & 1) {
    lx = lv;
    ly = lu;
  } else {
    lx = lu;
    ly = lv;
  }

  bufin                       = rin->buffer;
  wrapin                      = rin->wrap;
  E 61
I 61 CASE(0 << 2) + 2 : u00 = u1 + su;
  v00                         = v1 + sv;
  dvdy                        = -1;
  dindx                       = -1;
  CASE(0 << 2) + 3 : u00      = u1 + su;
  v00                         = v1;
  dudy                        = -1;
  dindx                       = wrapin;
  CASE(1 << 2) + 0 : u00      = u1 + su;
  v00                         = v1;
  dvdy                        = 1;
  dindx                       = -1;
  CASE(1 << 2) + 1 : u00      = u1 + su;
  v00                         = v1 + sv;
  dudy                        = -1;
  dindx                       = -wrapin;
  CASE(1 << 2) + 2 : u00      = u1;
  v00                         = v1 + sv;
  dvdy                        = -1;
  dindx                       = 1;
  CASE(1 << 2) + 3 : u00      = u1;
  v00                         = v1;
  dudy                        = 1;
  dindx                       = wrapin;
  D 78 DEFAULT : assert(FALSE);
  u00 = v00 = dudy = dindx = 0;
  E 78
I 78 DEFAULT : abort();
  u00 = v00 = dudy = dindx = 0;
  E 78
}

for (u0 = u00, v0 = v00, y = newy; y < newy + ly; u0 += dudy, v0 += dvdy, y++) {
  bytein  = bufin + u0 + v0 * wrapin;
  byteout = bufout + newx + y * wrapout;
  for (x = newx; x < newx + lx; x++) {
    *byteout++ = *bytein;
    bytein += dindx;
    D 62
  }
}

E 62
I 62
}
}

E 62
#ifdef DEBUG
    STW_STOP(0) STW_PRINT(stdout)
#endif
}

/*---------------------------------------------------------------------------*/

static void rop_copy_90_gr8_cm16(RASTER *rin, RASTER *rout, int x1, int y1,
                                 int x2, int y2, int newx, int newy, int mirror,
                                 int ninety) {
  UCHAR *bufin, *bytein;
  USHORT *bufout, *pixout;
  LPIXEL *cmap;
  int wrapin, wrapout, cmap_offset;
  int u, v, lu, lv, su, sv, u00, v00, u0, v0, dindx, dudy, dvdy;
  int x, y, lx, ly;
  int u1, v1, u2, v2;
  int reversed;

  mirror &= 1;
  ninety &= 3;

  if (!ninety && !mirror) {
    rop_copy(rin, rout, x1, y1, x2, y2, newx, newy);
    return;
  }
  D 84 if (!rout->cmap.buffer) {
    msg(MSG_IE, "rop_copy_90_gr8_cm16: missing color map");
    return;
  }
  E 84 cmap  = rout->cmap.buffer;
  cmap_offset = rout->cmap.info.offset_mask;
  reversed    = cmap[0].r != 0;

  u1 = x1;
  v1 = y1;
  u2 = x2;
  v2 = y2;

  su = u2 - u1;
  sv = v2 - v1;
  lu = u2 - u1 + 1;
  lv = v2 - v1 + 1;

  if (ninety & 1) {
    lx = lv;
    ly = lu;
  } else {
    lx = lu;
    ly = lv;
  }

  bufin   = rin->buffer;
  wrapin  = rin->wrap;
  bufout  = rout->buffer;
  wrapout = rout->wrap;

  dudy = 0;
  dvdy = 0;

  switch ((mirror << 2) + ninety) {
    CASE(0 << 2) + 0 : u00 = u1;
    v00                    = v1;
    dvdy                   = 1;
    dindx                  = 1;
    CASE(0 << 2) + 1 : u00 = u1;
    v00                    = v1 + sv;
    dudy                   = 1;
    dindx                  = -wrapin;
    CASE(0 << 2) + 2 : u00 = u1 + su;
    v00                    = v1 + sv;
    dvdy                   = -1;
    dindx                  = -1;
    CASE(0 << 2) + 3 : u00 = u1 + su;
    v00                    = v1;
    dudy                   = -1;
    dindx                  = wrapin;
    CASE(1 << 2) + 0 : u00 = u1 + su;
    v00                    = v1;
    dvdy                   = 1;
    dindx                  = -1;
    CASE(1 << 2) + 1 : u00 = u1 + su;
    v00                    = v1 + sv;
    dudy                   = -1;
    dindx                  = -wrapin;
    CASE(1 << 2) + 2 : u00 = u1;
    v00                    = v1 + sv;
    dvdy                   = -1;
    dindx                  = 1;
    CASE(1 << 2) + 3 : u00 = u1;
    v00                    = v1;
    dudy                   = 1;
    dindx                  = wrapin;
    D 78 DEFAULT : assert(FALSE);
    u00 = v00 = dudy = dindx = 0;
    E 78
I 78 DEFAULT : abort();
    u00 = v00 = dudy = dindx = 0;
    E 78
  }

  for (u0 = u00, v0 = v00, y = newy; y < newy + ly;
       u0 += dudy, v0 += dvdy, y++) {
    bytein = bufin + u0 + v0 * wrapin;
    pixout = bufout + newx + y * wrapout;
    if (reversed)
      for (x = newx; x < newx + lx; x++) {
        *pixout++ = cmap_offset + (255 - *bytein);
        bytein += dindx;
      }
    else
      for (x = newx; x < newx + lx; x++) {
        *pixout++ = cmap_offset + *bytein;
        bytein += dindx;
      }
  }
}

/*---------------------------------------------------------------------------*/

I 84 static void rop_copy_90_gr8_cm24(RASTER *rin, RASTER *rout, int x1,
                                       int y1, int x2, int y2, int newx,
                                       int newy, int mirror, int ninety) {
  UCHAR *bufin, *bytein;
  ULONG *bufout, *pixout;
  LPIXEL *penmap, *colmap;
  int wrapin, wrapout;
  int u, v, lu, lv, su, sv, u00, v00, u0, v0, dindx, dudy, dvdy;
  int x, y, lx, ly;
  int u1, v1, u2, v2;
  int reversed;

  mirror &= 1;
  ninety &= 3;

  if (!ninety && !mirror) {
    rop_copy(rin, rout, x1, y1, x2, y2, newx, newy);
    return;
  }
  penmap   = rout->cmap.penbuffer;
  colmap   = rout->cmap.colbuffer;
  reversed = penmap[0].r + colmap[0].r != 0;

  u1 = x1;
  v1 = y1;
  u2 = x2;
  v2 = y2;

  su = u2 - u1;
  sv = v2 - v1;
  lu = u2 - u1 + 1;
  lv = v2 - v1 + 1;

  if (ninety & 1) {
    lx = lv;
    ly = lu;
  } else {
    lx = lu;
    ly = lv;
  }

  bufin   = rin->buffer;
  wrapin  = rin->wrap;
  bufout  = rout->buffer;
  wrapout = rout->wrap;

  dudy = 0;
  dvdy = 0;

  switch ((mirror << 2) + ninety) {
    CASE(0 << 2) + 0 : u00 = u1;
    v00                    = v1;
    dvdy                   = 1;
    dindx                  = 1;
    CASE(0 << 2) + 1 : u00 = u1;
    v00                    = v1 + sv;
    dudy                   = 1;
    dindx                  = -wrapin;
    CASE(0 << 2) + 2 : u00 = u1 + su;
    v00                    = v1 + sv;
    dvdy                   = -1;
    dindx                  = -1;
    CASE(0 << 2) + 3 : u00 = u1 + su;
    v00                    = v1;
    dudy                   = -1;
    dindx                  = wrapin;
    CASE(1 << 2) + 0 : u00 = u1 + su;
    v00                    = v1;
    dvdy                   = 1;
    dindx                  = -1;
    CASE(1 << 2) + 1 : u00 = u1 + su;
    v00                    = v1 + sv;
    dudy                   = -1;
    dindx                  = -wrapin;
    CASE(1 << 2) + 2 : u00 = u1;
    v00                    = v1 + sv;
    dvdy                   = -1;
    dindx                  = 1;
    CASE(1 << 2) + 3 : u00 = u1;
    v00                    = v1;
    dudy                   = 1;
    dindx                  = wrapin;
  DEFAULT:
    abort();
    u00 = v00 = dudy = dindx = 0;
  }

  for (u0 = u00, v0 = v00, y = newy; y < newy + ly;
       u0 += dudy, v0 += dvdy, y++) {
    bytein = bufin + u0 + v0 * wrapin;
    pixout = bufout + newx + y * wrapout;
    if (reversed)
      for (x = newx; x < newx + lx; x++) {
        *pixout++ = 255 - *bytein;
        bytein += dindx;
      }
    else
      for (x = newx; x < newx + lx; x++) {
        *pixout++ = *bytein;
        bytein += dindx;
      }
  }
}

/*---------------------------------------------------------------------------*/

E 84 static void rop_copy_90_cm8_cm16(RASTER *rin, RASTER *rout, int x1,
                                       int y1, int x2, int y2, int newx,
                                       int newy, int mirror, int ninety) {
  UCHAR *bufin, *pixin;
  USHORT *bufout, *pixout;
  D 84 LPIXEL *cmapin, *cmapout;
  E 84 int wrapin, wrapout, cmapin_offset, cmapout_offset, cmap_offset_delta;
  int u, v, lu, lv, su, sv, u00, v00, u0, v0, dindx, dudy, dvdy;
  int x, y, lx, ly;
  int u1, v1, u2, v2;

  mirror &= 1;
  ninety &= 3;

  if (!ninety && !mirror) {
    rop_copy(rin, rout, x1, y1, x2, y2, newx, newy);
    return;
  }
  D 84 if (!rin->cmap.buffer || !rout->cmap.buffer) {
    msg(MSG_IE, "rop_copy_90_cm8_cm16: missing color map");
    return;
  }
  cmapin              = rin->cmap.buffer;
  cmapout             = rout->cmap.buffer;
  E 84 cmapin_offset = rin->cmap.info.offset_mask;
  cmapout_offset      = rout->cmap.info.offset_mask;
  cmap_offset_delta   = cmapout_offset - cmapin_offset;

  u1 = x1;
  v1 = y1;
  u2 = x2;
  v2 = y2;

  su = u2 - u1;
  sv = v2 - v1;
  lu = u2 - u1 + 1;
  lv = v2 - v1 + 1;

  if (ninety & 1) {
    lx = lv;
    ly = lu;
  } else {
    lx = lu;
    ly = lv;
  }

  bufin   = rin->buffer;
  wrapin  = rin->wrap;
  bufout  = rout->buffer;
  wrapout = rout->wrap;

  dudy = 0;
  dvdy = 0;

  switch ((mirror << 2) + ninety) {
    CASE(0 << 2) + 0 : u00 = u1;
    v00                    = v1;
    dvdy                   = 1;
    dindx                  = 1;
    CASE(0 << 2) + 1 : u00 = u1;
    v00                    = v1 + sv;
    dudy                   = 1;
    dindx                  = -wrapin;
    CASE(0 << 2) + 2 : u00 = u1 + su;
    v00                    = v1 + sv;
    dvdy                   = -1;
    dindx                  = -1;
    CASE(0 << 2) + 3 : u00 = u1 + su;
    v00                    = v1;
    dudy                   = -1;
    dindx                  = wrapin;
    CASE(1 << 2) + 0 : u00 = u1 + su;
    v00                    = v1;
    dvdy                   = 1;
    dindx                  = -1;
    CASE(1 << 2) + 1 : u00 = u1 + su;
    v00                    = v1 + sv;
    dudy                   = -1;
    dindx                  = -wrapin;
    CASE(1 << 2) + 2 : u00 = u1;
    v00                    = v1 + sv;
    dvdy                   = -1;
    dindx                  = 1;
    CASE(1 << 2) + 3 : u00 = u1;
    v00                    = v1;
    dudy                   = 1;
    dindx                  = wrapin;
    D 78 DEFAULT : assert(FALSE);
    u00 = v00 = dudy = dindx = 0;
    E 78
I 78 DEFAULT : abort();
    u00 = v00 = dudy = dindx = 0;
    E 78
  }

  for (u0 = u00, v0 = v00, y = newy; y < newy + ly;
       u0 += dudy, v0 += dvdy, y++) {
    pixin  = bufin + u0 + v0 * wrapin;
    pixout = bufout + newx + y * wrapout;
    for (x = newx; x < newx + lx; x++) {
      *pixout++ = *pixin + cmap_offset_delta;
      pixin += dindx;
    }
  }
}

/*---------------------------------------------------------------------------*/

I 84 static void rop_copy_90_cm8_cm24(RASTER *rin, RASTER *rout, int x1,
                                       int y1, int x2, int y2, int newx,
                                       int newy, int mirror, int ninety) {
  UCHAR *bufin, *pixin;
  ULONG *bufout, *pixout;
  int wrapin, wrapout, cmapin_offset, cmap_offset_delta;
  int u, v, lu, lv, su, sv, u00, v00, u0, v0, dindx, dudy, dvdy;
  int x, y, lx, ly;
  int u1, v1, u2, v2;

  mirror &= 1;
  ninety &= 3;

  if (!ninety && !mirror) {
    rop_copy(rin, rout, x1, y1, x2, y2, newx, newy);
    return;
  }
  cmapin_offset     = rin->cmap.info.offset_mask;
  cmap_offset_delta = -cmapin_offset;

  u1 = x1;
  v1 = y1;
  u2 = x2;
  v2 = y2;

  su = u2 - u1;
  sv = v2 - v1;
  lu = u2 - u1 + 1;
  lv = v2 - v1 + 1;

  if (ninety & 1) {
    lx = lv;
    ly = lu;
  } else {
    lx = lu;
    ly = lv;
  }

  bufin   = rin->buffer;
  wrapin  = rin->wrap;
  bufout  = rout->buffer;
  wrapout = rout->wrap;

  dudy = 0;
  dvdy = 0;

  switch ((mirror << 2) + ninety) {
    CASE(0 << 2) + 0 : u00 = u1;
    v00                    = v1;
    dvdy                   = 1;
    dindx                  = 1;
    CASE(0 << 2) + 1 : u00 = u1;
    v00                    = v1 + sv;
    dudy                   = 1;
    dindx                  = -wrapin;
    CASE(0 << 2) + 2 : u00 = u1 + su;
    v00                    = v1 + sv;
    dvdy                   = -1;
    dindx                  = -1;
    CASE(0 << 2) + 3 : u00 = u1 + su;
    v00                    = v1;
    dudy                   = -1;
    dindx                  = wrapin;
    CASE(1 << 2) + 0 : u00 = u1 + su;
    v00                    = v1;
    dvdy                   = 1;
    dindx                  = -1;
    CASE(1 << 2) + 1 : u00 = u1 + su;
    v00                    = v1 + sv;
    dudy                   = -1;
    dindx                  = -wrapin;
    CASE(1 << 2) + 2 : u00 = u1;
    v00                    = v1 + sv;
    dvdy                   = -1;
    dindx                  = 1;
    CASE(1 << 2) + 3 : u00 = u1;
    v00                    = v1;
    dudy                   = 1;
    dindx                  = wrapin;
  DEFAULT:
    abort();
    u00 = v00 = dudy = dindx = 0;
  }

  for (u0 = u00, v0 = v00, y = newy; y < newy + ly;
       u0 += dudy, v0 += dvdy, y++) {
    pixin  = bufin + u0 + v0 * wrapin;
    pixout = bufout + newx + y * wrapout;
    for (x = newx; x < newx + lx; x++) {
      *pixout++ = *pixin + cmap_offset_delta;
      pixin += dindx;
    }
  }
}

/*---------------------------------------------------------------------------*/

E 84 static void rop_copy_90_gr8_rgb16(RASTER *rin, RASTER *rout, int x1,
                                        int y1, int x2, int y2, int newx,
                                        int newy, int mirror, int ninety) {
  UCHAR *bufin, *bytein;
  USHORT *bufout, *pixout;
  int wrapin, wrapout;
  int u, v, lu, lv, su, sv, u00, v00, u0, v0, dindx, dudy, dvdy;
  int x, y, lx, ly;
  int u1, v1, u2, v2;

  mirror &= 1;
  ninety &= 3;

  if (!ninety && !mirror) {
    rop_copy(rin, rout, x1, y1, x2, y2, newx, newy);
    return;
  }

  u1 = x1;
  v1 = y1;
  u2 = x2;
  v2 = y2;

  su = u2 - u1;
  sv = v2 - v1;
  lu = u2 - u1 + 1;
  lv = v2 - v1 + 1;

  if (ninety & 1) {
    lx = lv;
    ly = lu;
  } else {
    lx = lu;
    ly = lv;
  }

  bufin        = rin->buffer;
  wrapin       = rin->wrap;
  E 61 bufout = rout->buffer;
  wrapout      = rout->wrap;

  dudy = 0;
  dvdy = 0;

  switch ((mirror << 2) + ninety) {
    CASE(0 << 2) + 0 : u00 = u1;
    v00                    = v1;
    dvdy                   = 1;
    dindx                  = 1;
    CASE(0 << 2) + 1 : u00 = u1;
    v00                    = v1 + sv;
    dudy                   = 1;
    dindx                  = -wrapin;
    CASE(0 << 2) + 2 : u00 = u1 + su;
    v00                    = v1 + sv;
    dvdy                   = -1;
    dindx                  = -1;
    CASE(0 << 2) + 3 : u00 = u1 + su;
    v00                    = v1;
    dudy                   = -1;
    dindx                  = wrapin;
    CASE(1 << 2) + 0 : u00 = u1 + su;
    v00                    = v1;
    dvdy                   = 1;
    dindx                  = -1;
    CASE(1 << 2) + 1 : u00 = u1 + su;
    v00                    = v1 + sv;
    dudy                   = -1;
    dindx                  = -wrapin;
    CASE(1 << 2) + 2 : u00 = u1;
    v00                    = v1 + sv;
    dvdy                   = -1;
    dindx                  = 1;
    CASE(1 << 2) + 3 : u00 = u1;
    v00                    = v1;
    dudy                   = 1;
    dindx                  = wrapin;
    I 50
D 78 DEFAULT : assert(FALSE);
    u00 = v00 = dudy = dindx = 0;
    E 78
I 78 DEFAULT : abort();
    u00 = v00 = dudy = dindx = 0;
    E 78
E 50
  }

  for (u0 = u00, v0 = v00, y = newy; y < newy + ly;
       u0 += dudy, v0 += dvdy, y++) {
    bytein = bufin + u0 + v0 * wrapin;
    pixout = bufout + newx + y * wrapout;
    for (x = newx; x < newx + lx; x++) {
      D 55 tmp.r = *bytein;
      tmp.g       = *bytein;
      tmp.b       = *bytein;
      I 43 tmp.m = 0xff;
      *pixout++   = tmp;
      E 55
I 55
D 60 *pixout++  = ROP_RGB16_FROM_BYTES(*bytein, *bytein, *bytein);
      E 60
I 60 *pixout++  = PIX_RGB16_FROM_BYTES(*bytein, *bytein, *bytein);
      E 60
E 55 bytein += dindx;
    }
  }
}

/*---------------------------------------------------------------------------*/

D 55
static void rop_copy_90_rgb_rgbm(RASTER *rin, RASTER *rout,
E 55
I 55
D 56
static void rop_copy_90_gr8_rgbm(RASTER *rin, RASTER *rout,
E 55
                                 int x1, int y1, int x2, int y2,
			         int newx, int newy,
		                 int mirror, int ninety)
E 56
I 56
D 57
#ifdef DAFARE

E 57
static void rop_zoom_out_90_gr8_rgb16(RASTER *rin, RASTER *rout,
                                      int x1, int y1, int x2, int y2,
			              int newx, int newy,
		                      int abs_zoom_level,
				      int mirror, int ninety)
E 56
{
  D 55 UCHAR *bufin, *bytein, *appo;
  E 55
I 55
D 56 UCHAR *bufin, *bytein;
  E 55 LPIXEL *bufout, *pixout, tmp;
  E 56
I 56 UCHAR *bufin, *rowin, *pixin, *in;
  int tmp;
  USHORT *bufout, *rowout, *pixout;
  E 56 int wrapin, wrapout;
  D 57 int u, v, lu, lv, su, sv, u00, v00, u0, v0, dindx, dudy, dvdy;
  int x, y, lx, ly;
  E 57 int u1, v1, u2, v2;
  I 56
D 57 int ulast, vlast, urest, vrest, i, j;
  E 57
I 57 int u, v, lu, lv, startu, startv;
  int p, q, lp, lq;
  int dudp, dvdp, dudq, dvdq, dindp, dindq;
  int plast, qlast, prest, qrest, i, j;
  E 57 int factor, fac_fac_2_bits;
  D 57 int fac_fac, vrest_fac, fac_urest, vrest_urest;
  int fac_fac_2, vrest_fac_2, fac_urest_2, vrest_urest_2;
  E 57
I 57 int fac_fac, qrest_fac, fac_prest, qrest_prest;
  int fac_fac_2, qrest_fac_2, fac_prest_2, qrest_prest_2;
  E 57 int fac_fac_4;
  E 56

      mirror &= 1;
  ninety &= 3;

  D 44 printf("mirror = %d ninety = %d\n", mirror, ninety);

  E 44 if (!ninety && !mirror) {
    D 56 rop_copy(rin, rout, x1, y1, x2, y2, newx, newy);
    E 56
I 56
D 57 rop_zoom_out(rin, rout, x1, y1, x2, y2, newx, newy);
    E 57
I 57 rop_zoom_out(rin, rout, x1, y1, x2, y2, newx, newy, abs_zoom_level);
    E 57
E 56 return;
  }

  u1 = x1;
  v1 = y1;
  u2 = x2;
  v2 = y2;

  D 57 su = u2 - u1;
  sv       = v2 - v1;
  E 57 lu = u2 - u1 + 1;
  lv       = v2 - v1 + 1;

  if (ninety & 1) {
    D 57 lx = lv;
    ly       = lu;
    E 57
I 57 lp    = lv;
    lq       = lu;
    E 57
  } else {
    D 57 lx = lu;
    ly       = lv;
    E 57
I 57 lp    = lu;
    lq       = lv;
    E 57
  }
  D 57

E 57
I 56 factor     = 1 << abs_zoom_level;
  D 57 urest     = lu % factor;
  vrest           = lv % factor;
  ulast           = u2 - urest + 1;
  vlast           = v2 - vrest + 1;
  E 57
I 57
D 61 prest      = lp % factor;
  qrest           = lq % factor;
  E 61
I 61 prest      = lp & (factor - 1);
  qrest           = lq & (factor - 1);
  E 61 plast     = lp - prest;
  qlast           = lq - qrest;
  E 57 fac_fac   = factor * factor;
  fac_fac_2       = fac_fac >> 1;
  fac_fac_4       = fac_fac >> 2;
  fac_fac_2_bits  = 2 * abs_zoom_level - 1;
  D 57 vrest_fac = vrest * factor;
  vrest_fac_2     = vrest_fac >> 1;
  fac_urest       = factor * urest;
  fac_urest_2     = fac_urest >> 1;
  vrest_urest     = vrest * urest;
  vrest_urest_2   = vrest_urest >> 1;
  E 57
I 57 qrest_fac  = qrest * factor;
  qrest_fac_2     = qrest_fac >> 1;
  fac_prest       = factor * prest;
  fac_prest_2     = fac_prest >> 1;
  qrest_prest     = qrest * prest;
  qrest_prest_2   = qrest_prest >> 1;
  E 57

E 56 bufin      = rin->buffer;
  D 55 wrapin    = rin->wrap * 3; /* mi muovo di bytes */
     E 55
I 55 wrapin     = rin->wrap;
  E 55 bufout    = rout->buffer;
  wrapout         = rout->wrap;

  D 57 dudy = 0;
  dvdy       = 0;

  E 57 switch ((mirror << 2) + ninety) {
    D 55 CASE(0 << 2) + 0 : u00 = u1;
    v00                          = v1;
    dvdy                         = 1;
    dindx                        = 3;
    E 55
I 55
D 57 CASE(0 << 2) + 0 : u00    = u1;
    v00                          = v1;
    dvdy                         = 1;
    dindx                        = 1;
    E 55 CASE(0 << 2) + 1 : u00 = u1;
    v00                          = v1 + sv;
    dudy                         = 1;
    dindx                        = -wrapin;
    D 55 CASE(0 << 2) + 2 : u00 = u1 + su;
    v00                          = v1 + sv;
    dvdy                         = -1;
    dindx                        = -3;
    E 55
I 55 CASE(0 << 2) + 2 : u00    = u1 + su;
    v00                          = v1 + sv;
    dvdy                         = -1;
    dindx                        = -1;
    E 55 CASE(0 << 2) + 3 : u00 = u1 + su;
    v00                          = v1;
    dudy                         = -1;
    dindx                        = wrapin;
    D 55 CASE(1 << 2) + 0 : u00 = u1 + su;
    v00                          = v1;
    dvdy                         = 1;
    dindx                        = -3;
    E 55
I 55 CASE(1 << 2) + 0 : u00    = u1 + su;
    v00                          = v1;
    dvdy                         = 1;
    dindx                        = -1;
    E 55 CASE(1 << 2) + 1 : u00 = u1 + su;
    v00                          = v1 + sv;
    dudy                         = -1;
    dindx                        = -wrapin;
    D 55 CASE(1 << 2) + 2 : u00 = u1;
    v00                          = v1 + sv;
    dvdy                         = -1;
    dindx                        = 3;
    E 55
I 55 CASE(1 << 2) + 2 : u00    = u1;
    v00                          = v1 + sv;
    dvdy                         = -1;
    dindx                        = 1;
    E 55 CASE(1 << 2) + 3 : u00 = u1;
    v00                          = v1;
    dudy                         = 1;
    dindx                        = wrapin;
    I 50 DEFAULT : assert(FALSE);
    u00 = v00 = dudy = dindx = 0;
    E 50
  }

  for (u0 = u00, v0 = v00, y = newy; y < newy + ly;
       u0 += dudy, v0 += dvdy, y++) {
    D 55 bytein = bufin + u0 * 3 + v0 * wrapin;
    E 55
I 55 bytein    = bufin + u0 + v0 * wrapin;
    E 55 pixout = bufout + newx + y * wrapout;
    for (x = newx; x < newx + lx; x++) {
      D 55 appo = bytein;
      tmp.r      = *appo++;
      tmp.g      = *appo++;
      tmp.b      = *appo;
      E 55
I 55
D 56 tmp.r     = *bytein;
      tmp.g      = *bytein;
      tmp.b      = *bytein;
      E 55
E 43 tmp.m     = 0xff;
      *pixout++  = tmp;
      E 56
I 56 *pixout++ = ROP_RGB16_FROM_BYTES(*bytein, *bytein, *bytein);
      E 56 bytein += dindx;
    }
    E 57
I 57 CASE(0 << 2) + 0 : dudp    = 1;
    dvdp                          = 0;
    dudq                          = 0;
    dvdq                          = 1;
    startu                        = u1;
    startv                        = v1;
    CASE(0 << 2) + 1 : dudp       = 0;
    dvdp                          = -1;
    dudq                          = 1;
    dvdq                          = 0;
    startu                        = u1;
    startv                        = v2;
    CASE(0 << 2) + 2 : dudp       = -1;
    dvdp                          = 0;
    dudq                          = 0;
    dvdq                          = -1;
    startu                        = u2;
    startv                        = v2;
    CASE(0 << 2) + 3 : dudp       = 0;
    dvdp                          = 1;
    dudq                          = -1;
    dvdq                          = 0;
    startu                        = u2;
    startv                        = v1;
    CASE(1 << 2) + 0 : dudp       = -1;
    dvdp                          = 0;
    dudq                          = 0;
    dvdq                          = 1;
    startu                        = u2;
    startv                        = v1;
    D 61 CASE(1 << 2) + 1 : dudp = 0;
    dvdp                          = 1;
    dudq                          = 1;
    dvdq                          = 0;
    startu                        = u2;
    startv                        = v2;
    E 61
I 61 CASE(1 << 2) + 1 : dudp    = 0;
    dvdp                          = -1;
    dudq                          = -1;
    dvdq                          = 0;
    startu                        = u2;
    startv                        = v2;
    E 61 CASE(1 << 2) + 2 : dudp = 1;
    dvdp                          = 0;
    dudq                          = 0;
    dvdq                          = -1;
    startu                        = u1;
    startv                        = v2;
    D 61 CASE(1 << 2) + 3 : dudp = 0;
    dvdp                          = -1;
    dudq                          = -1;
    dvdq                          = 0;
    startu                        = u1;
    startv                        = v1;
    E 61
I 61 CASE(1 << 2) + 3 : dudp    = 0;
    dvdp                          = 1;
    dudq                          = 1;
    dvdq                          = 0;
    startu                        = u1;
    startv                        = v1;
    E 61 DEFAULT :
D 78 assert(FALSE);
    dudp = dvdp = dudq = dvdq = startu = startv = 0;
    E 78
I 78 abort();
    dudp = dvdp = dudq = dvdq = startu = startv = 0;
    E 78
E 57
  }
  I 57 dindp = dudp + wrapin * dvdp;
  dindq       = dudq + wrapin * dvdq;
  E 57
D 56
}

/*---------------------------------------------------------------------------*/

E 22
E 21
D 55
static void rop_copy_90_bw_gr8(RASTER *rin, RASTER *rout,
                               int x1, int y1, int x2, int y2,
			       int newx, int newy,
		               int mirror, int ninety)
E 55
I 55
static void rop_copy_90_rgb_rgbm(RASTER *rin, RASTER *rout,
                                 int x1, int y1, int x2, int y2,
			         int newx, int newy,
		                 int mirror, int ninety)
E 55
{
  D 55 UCHAR *bufin, *bufout, *bytein, *byteout;
  int bytewrapin, bitoffsin, wrapout;
  int value_for_0, value_for_1;
  int u, v, lu, lv, su, sv, u00, v00, u0, v0, dudy, dvdy, dudx, dvdx;
  E 55
I 55 UCHAR *bufin, *bytein, *appo;
  LPIXEL *bufout, *pixout, tmp;
  int wrapin, wrapout;
  int u, v, lu, lv, su, sv, u00, v00, u0, v0, dindx, dudy, dvdy;
  E 55 int x, y, lx, ly;
  int u1, v1, u2, v2;

  D 55
#ifdef DEBUG
      STW_INIT(0, "rop_copy_90_bw_gr8") STW_START(0)
#endif

E 55 mirror &= 1;
  D 55 ninety &= 3;

  if (!ninety && !mirror) {
    rop_copy(rin, rout, x1, y1, x2, y2, newx, newy);
    return;
    E 55
I 55 ninety &= 3;

    if (!ninety && !mirror) {
      rop_copy(rin, rout, x1, y1, x2, y2, newx, newy);
      return;
    }
    E 56

D 56 u1 = x1;
    v1    = y1;
    u2    = x2;
    v2    = y2;

    su = u2 - u1;
    sv = v2 - v1;
    lu = u2 - u1 + 1;
    lv = v2 - v1 + 1;

    if (ninety & 1) {
      lx = lv;
      ly = lu;
    } else {
      lx = lu;
      ly = lv;
    }

    bufin   = rin->buffer;
    wrapin  = rin->wrap * 3; /* mi muovo di bytes */
    bufout  = rout->buffer;
    wrapout = rout->wrap;

    dudy = 0;
    dvdy = 0;

    switch ((mirror << 2) + ninety)
      E 56
I 56
D 57 rowin = (UCHAR *)rin->buffer + wrapin * v1 + u1;
    rowout   = (USHORT *)rout->buffer + wrapout * newy + newx;
    for (v = v1; v < vlast; v += factor)
      E 57
I 57 rowin = (UCHAR *)rin->buffer + startu + startv * wrapin;
    rowout   = (USHORT *)rout->buffer + newx + newy * wrapout;
    for (q = 0; q < qlast; q += factor)
			E 57
E 56 {
        D 56 CASE(0 << 2) + 0 : u00 = u1;
        v00                          = v1;
        dvdy                         = 1;
        dindx                        = 3;
        CASE(0 << 2) + 1 : u00       = u1;
        v00                          = v1 + sv;
        dudy                         = 1;
        dindx                        = -wrapin;
        CASE(0 << 2) + 2 : u00       = u1 + su;
        v00                          = v1 + sv;
        dvdy                         = -1;
        dindx                        = -3;
        CASE(0 << 2) + 3 : u00       = u1 + su;
        v00                          = v1;
        dudy                         = -1;
        dindx                        = wrapin;
        CASE(1 << 2) + 0 : u00       = u1 + su;
        v00                          = v1;
        dvdy                         = 1;
        dindx                        = -3;
        CASE(1 << 2) + 1 : u00       = u1 + su;
        v00                          = v1 + sv;
        dudy                         = -1;
        dindx                        = -wrapin;
        CASE(1 << 2) + 2 : u00       = u1;
        v00                          = v1 + sv;
        dvdy                         = -1;
        dindx                        = 3;
        CASE(1 << 2) + 3 : u00       = u1;
        v00                          = v1;
        dudy                         = 1;
        dindx                        = wrapin;
      DEFAULT:
        assert(FALSE);
        u00 = v00 = dudy = dindx = 0;
        E 56
I 56 pixin                     = rowin;
        pixout                   = rowout;
        D 57 for (u = u1; u < ulast; u += factor)
E 57
I 57
D 58 for (p = 0; p < plast; p += factor)
E 57 {
          tmp = 0;
          in  = pixin;
          for (j = 0; j < factor; j += 2) {
            for (i = 0; i < factor; i += 2) {
              tmp += *in;
              D 57 in += wrapin;
              in++;
              E 57
I 57 in += dindp + dindq;
              E 57 tmp += *in;
              D 57 in -= wrapin;
              in++;
              E 57
I 57 in += dindp - dindq;
              E 57
            }
            D 57 in += 2 * wrapin - factor;
            E 57
I 57 in += 2 * dindq - factor * dindp;
            E 57
          }
          tmp       = (tmp + fac_fac_4) >> fac_fac_2_bits;
          *pixout++ = ROP_RGB16_FROM_BYTES(tmp, tmp, tmp);
          D 57 pixin += factor;
          E 57
I 57 pixin += factor * dindp;
          E 57
        }
        D 57 if (urest)
E 57
I 57 if (prest)
E 57 {
          tmp = 0;
          for (j = 0; j < factor; j++)
            D 57 for (i = 0; i < urest; i++)
E 57
I 57 for (i = 0; i < prest; i++)
E 57 {
              D 57 tmp += pixin[i + j * wrapin];
              E 57
I 57 tmp += pixin[i * dindp + j * dindq];
              E 57
            }
          D 57 tmp     = (tmp + fac_urest_2) / fac_urest;
          E 57
I 57 tmp              = (tmp + fac_prest_2) / fac_prest;
          E 57 *pixout = ROP_RGB16_FROM_BYTES(tmp, tmp, tmp);
        }
        D 57 rowin += wrapin * factor;
        E 57
I 57 rowin += factor * dindq;
        E 57 rowout += wrapout;
        E 56
      }
    D 56

        for (u0 = u00, v0 = v00, y = newy; y < newy + ly;
             u0 += dudy, v0 += dvdy, y++)
E 56
I 56
D 57 if (vrest)
E 57
I 57 if (qrest)
E 57
E 56 {
      D 56 bytein = bufin + u0 * 3 + v0 * wrapin;
      pixout       = bufout + newx + y * wrapout;
      for (x = newx; x < newx + lx; x++)
				E 56
I 56 pixin = rowin;
      pixout = rowout;
      D 57 for (u = u1; u < ulast; u += factor)
E 57
I 57 for (p = 0; p < plast; p += factor)
E 57
E 56 {
        D 56 appo = bytein;
        tmp.r      = *appo++;
        tmp.g      = *appo++;
        tmp.b      = *appo;
        tmp.m      = 0xff;
        *pixout++  = tmp;
        bytein += dindx;
        E 56
I 56 tmp = 0;
        D 57 for (j = 0; j < vrest; j++)
E 57
I 57 for (j = 0; j < qrest; j++)
E 57 for (i = 0; i < factor; i++) {
          D 57 tmp += pixin[i + j * wrapin];
          E 57
I 57 tmp += pixin[i * dindp + j * dindq];
          E 57
        }
        D 57 tmp       = (tmp + vrest_fac_2) / vrest_fac;
        E 57
I 57 tmp              = (tmp + qrest_fac_2) / qrest_fac;
        E 57 *pixout++ = ROP_RGB16_FROM_BYTES(tmp, tmp, tmp);
        D 57 pixin += factor;
        E 57
I 57 pixin += factor * dindp;
        E 57
      }
      D 57 if (urest)
E 57
I 57 if (prest)
E 57 {
        tmp = 0;
        D 57 for (j = 0; j < vrest; j++) for (i = 0; i < urest; i++)
E 57
I 57 for (j = 0; j < qrest; j++) for (i = 0; i < prest; i++)
E 57 {
          D 57 tmp += pixin[i + j * wrapin];
          E 57
I 57 tmp += pixin[i * dindp + j * dindq];
          E 57
        }
        D 57 tmp     = (tmp + vrest_urest_2) / vrest_urest;
        E 57
I 57 tmp            = (tmp + qrest_prest_2) / qrest_prest;
        E 57 *pixout = ROP_RGB16_FROM_BYTES(tmp, tmp, tmp);
        E 56
      }
    }
    D 56
  }
  E 56
D 57

E 57
D 56
      /*---------------------------------------------------------------------------*/
E 56
I 56
}
E 56

D 56
static void rop_copy_90_bw_gr8(RASTER *rin, RASTER *rout,
                               int x1, int y1, int x2, int y2,
E 56
I 56
D 57
#endif

E 57
/*---------------------------------------------------------------------------*/

static void rop_copy_90_gr8_rgbm(RASTER *rin, RASTER *rout,
                                 int x1, int y1, int x2, int y2,
			         int newx, int newy,
		                 int mirror, int ninety)
{
  UCHAR *bufin, *bytein;
  LPIXEL *bufout, *pixout, tmp;
  int wrapin, wrapout;
  int u, v, lu, lv, su, sv, u00, v00, u0, v0, dindx, dudy, dvdy;
  int x, y, lx, ly;
  int u1, v1, u2, v2;

  mirror &= 1;
  ninety &= 3;

  if (!ninety && !mirror) {
    rop_copy(rin, rout, x1, y1, x2, y2, newx, newy);
    return;
  }

  u1 = x1;
  v1 = y1;
  u2 = x2;
  v2 = y2;

  su = u2 - u1;
  sv = v2 - v1;
  lu = u2 - u1 + 1;
  lv = v2 - v1 + 1;

  if (ninety & 1) {
    lx = lv;
    ly = lu;
  } else {
    lx = lu;
    ly = lv;
  }

  bufin   = rin->buffer;
  wrapin  = rin->wrap;
  bufout  = rout->buffer;
  wrapout = rout->wrap;

  dudy = 0;
  dvdy = 0;

  switch ((mirror << 2) + ninety) {
    CASE(0 << 2) + 0 : u00 = u1;
    v00                    = v1;
    dvdy                   = 1;
    dindx                  = 1;
    CASE(0 << 2) + 1 : u00 = u1;
    v00                    = v1 + sv;
    dudy                   = 1;
    dindx                  = -wrapin;
    CASE(0 << 2) + 2 : u00 = u1 + su;
    v00                    = v1 + sv;
    dvdy                   = -1;
    dindx                  = -1;
    CASE(0 << 2) + 3 : u00 = u1 + su;
    v00                    = v1;
    dudy                   = -1;
    dindx                  = wrapin;
    CASE(1 << 2) + 0 : u00 = u1 + su;
    v00                    = v1;
    dvdy                   = 1;
    dindx                  = -1;
    CASE(1 << 2) + 1 : u00 = u1 + su;
    v00                    = v1 + sv;
    dudy                   = -1;
    dindx                  = -wrapin;
    E 58
I 58 for (p = 0; p < plast; p += factor) {
      tmp = 0;
      in  = pixin;
      for (j = 0; j < factor; j += 2) {
        for (i = 0; i < factor; i += 2) {
          tmp += *in;
          in += dindp + dindq;
          tmp += *in;
          in += dindp - dindq;
        }
        in += 2 * dindq - factor * dindp;
      }
      tmp             = (tmp + fac_fac_4) >> fac_fac_2_bits;
      D 60 *pixout++ = ROP_RGB16_FROM_BYTES(tmp, tmp, tmp);
      E 60
I 60 *pixout++      = PIX_RGB16_FROM_BYTES(tmp, tmp, tmp);
      E 60 pixin += factor * dindp;
    }
    if (prest) {
      tmp = 0;
      for (j = 0; j < factor; j++)
        for (i = 0; i < prest; i++) {
          tmp += pixin[i * dindp + j * dindq];
        }
      tmp           = (tmp + fac_prest_2) / fac_prest;
      D 60 *pixout = ROP_RGB16_FROM_BYTES(tmp, tmp, tmp);
      E 60
I 60 *pixout      = PIX_RGB16_FROM_BYTES(tmp, tmp, tmp);
      E 60
    }
    rowin += factor * dindq;
    rowout += wrapout;
  }
  D 59 if (qrest)
E 59
I 59 if (qrest)
E 59 {
    pixin  = rowin;
    pixout = rowout;
    for (p = 0; p < plast; p += factor) {
      tmp = 0;
      for (j = 0; j < qrest; j++)
        for (i = 0; i < factor; i++) {
          tmp += pixin[i * dindp + j * dindq];
        }
      tmp             = (tmp + qrest_fac_2) / qrest_fac;
      D 60 *pixout++ = ROP_RGB16_FROM_BYTES(tmp, tmp, tmp);
      E 60
I 60 *pixout++      = PIX_RGB16_FROM_BYTES(tmp, tmp, tmp);
      E 60 pixin += factor * dindp;
    }
    if (prest) {
      tmp = 0;
      for (j = 0; j < qrest; j++)
        for (i = 0; i < prest; i++) {
          tmp += pixin[i * dindp + j * dindq];
        }
      tmp           = (tmp + qrest_prest_2) / qrest_prest;
      D 60 *pixout = ROP_RGB16_FROM_BYTES(tmp, tmp, tmp);
      E 60
I 60 *pixout      = PIX_RGB16_FROM_BYTES(tmp, tmp, tmp);
      E 60
    }
  }
}

/*---------------------------------------------------------------------------*/

static void rop_copy_90_gr8_rgbm(RASTER *rin, RASTER *rout,
                                 int x1, int y1, int x2, int y2,
			         int newx, int newy,
		                 int mirror, int ninety)
{
  UCHAR *bufin, *bytein;
  LPIXEL *bufout, *pixout, tmp;
  int wrapin, wrapout;
  int u, v, lu, lv, su, sv, u00, v00, u0, v0, dindx, dudy, dvdy;
  int x, y, lx, ly;
  int u1, v1, u2, v2;

  mirror &= 1;
  ninety &= 3;

  if (!ninety && !mirror) {
    rop_copy(rin, rout, x1, y1, x2, y2, newx, newy);
    return;
  }

  u1 = x1;
  v1 = y1;
  u2 = x2;
  v2 = y2;

  su = u2 - u1;
  sv = v2 - v1;
  lu = u2 - u1 + 1;
  lv = v2 - v1 + 1;

  if (ninety & 1) {
    lx = lv;
    ly = lu;
  }
  D 65 else {
    lx = lu;
    ly = lv;
  }

  bufin   = rin->buffer;
  wrapin  = rin->wrap;
  bufout  = rout->buffer;
  wrapout = rout->wrap;

  dudy = 0;
  dvdy = 0;

  switch ((mirror << 2) + ninety) {
    E 65
I 65 else {
      lx = lu;
      ly = lv;
    }

    bufin   = rin->buffer;
    wrapin  = rin->wrap;
    bufout  = rout->buffer;
    wrapout = rout->wrap;

    dudy = 0;
    dvdy = 0;

    switch ((mirror << 2) + ninety) {
      E 65 CASE(0 << 2) + 0 : u00 = u1;
      v00                          = v1;
      dvdy                         = 1;
      dindx                        = 1;
      CASE(0 << 2) + 1 : u00       = u1;
      v00                          = v1 + sv;
      dudy                         = 1;
      dindx                        = -wrapin;
      CASE(0 << 2) + 2 : u00       = u1 + su;
      v00                          = v1 + sv;
      dvdy                         = -1;
      dindx                        = -1;
      CASE(0 << 2) + 3 : u00       = u1 + su;
      v00                          = v1;
      dudy                         = -1;
      dindx                        = wrapin;
      CASE(1 << 2) + 0 : u00       = u1 + su;
      v00                          = v1;
      dvdy                         = 1;
      dindx                        = -1;
      CASE(1 << 2) + 1 : u00       = u1 + su;
      v00                          = v1 + sv;
      dudy                         = -1;
      dindx                        = -wrapin;
      E 58 CASE(1 << 2) + 2 : u00 = u1;
      v00                          = v1 + sv;
      dvdy                         = -1;
      dindx                        = 1;
      CASE(1 << 2) + 3 : u00       = u1;
      v00                          = v1;
      dudy                         = 1;
      dindx                        = wrapin;
      D 78 DEFAULT : assert(FALSE);
      u00 = v00 = dudy = dindx = 0;
      E 78
I 78 DEFAULT : abort();
      u00 = v00 = dudy = dindx = 0;
      E 78
    }

    for (u0 = u00, v0 = v00, y = newy; y < newy + ly;
         u0 += dudy, v0 += dvdy, y++) {
      bytein = bufin + u0 + v0 * wrapin;
      pixout = bufout + newx + y * wrapout;
      for (x = newx; x < newx + lx; x++) {
        tmp.r     = *bytein;
        tmp.g     = *bytein;
        tmp.b     = *bytein;
        tmp.m     = 0xff;
        *pixout++ = tmp;
        bytein += dindx;
      }
    }
  }

  /*---------------------------------------------------------------------------*/

  I 57 static void rop_zoom_out_90_gr8_rgbm(
      RASTER * rin, RASTER * rout, int x1, int y1, int x2, int y2, int newx,
      int newy, int abs_zoom_level, int mirror, int ninety) {
    UCHAR *bufin, *rowin, *pixin, *in;
    int tmp;
    LPIXEL *bufout, *rowout, *pixout, valout;
    int wrapin, wrapout;
    int u1, v1, u2, v2;
    int u, v, lu, lv, startu, startv;
    int p, q, lp, lq;
    int dudp, dvdp, dudq, dvdq, dindp, dindq;
    int plast, qlast, prest, qrest, i, j;
    int factor, fac_fac_2_bits;
    int fac_fac, qrest_fac, fac_prest, qrest_prest;
    int fac_fac_2, qrest_fac_2, fac_prest_2, qrest_prest_2;
    int fac_fac_4;

    mirror &= 1;
    ninety &= 3;

    if (!ninety && !mirror) {
      rop_zoom_out(rin, rout, x1, y1, x2, y2, newx, newy, abs_zoom_level);
      return;
    }

    u1 = x1;
    v1 = y1;
    u2 = x2;
    v2 = y2;

    lu = u2 - u1 + 1;
    lv = v2 - v1 + 1;

    if (ninety & 1) {
      lp = lv;
      lq = lu;
    } else {
      lp = lu;
      lq = lv;
    }
    factor         = 1 << abs_zoom_level;
    D 61 prest    = lp % factor;
    qrest          = lq % factor;
    E 61
I 61 prest       = lp & (factor - 1);
    qrest          = lq & (factor - 1);
    E 61 plast    = lp - prest;
    qlast          = lq - qrest;
    fac_fac        = factor * factor;
    fac_fac_2      = fac_fac >> 1;
    fac_fac_4      = fac_fac >> 2;
    fac_fac_2_bits = 2 * abs_zoom_level - 1;
    qrest_fac      = qrest * factor;
    qrest_fac_2    = qrest_fac >> 1;
    fac_prest      = factor * prest;
    fac_prest_2    = fac_prest >> 1;
    qrest_prest    = qrest * prest;
    qrest_prest_2  = qrest_prest >> 1;

    bufin   = rin->buffer;
    wrapin  = rin->wrap;
    bufout  = rout->buffer;
    wrapout = rout->wrap;

    switch ((mirror << 2) + ninety) {
      CASE(0 << 2) + 0 : dudp       = 1;
      dvdp                          = 0;
      dudq                          = 0;
      dvdq                          = 1;
      startu                        = u1;
      startv                        = v1;
      CASE(0 << 2) + 1 : dudp       = 0;
      dvdp                          = -1;
      dudq                          = 1;
      dvdq                          = 0;
      startu                        = u1;
      startv                        = v2;
      CASE(0 << 2) + 2 : dudp       = -1;
      dvdp                          = 0;
      dudq                          = 0;
      dvdq                          = -1;
      startu                        = u2;
      startv                        = v2;
      CASE(0 << 2) + 3 : dudp       = 0;
      dvdp                          = 1;
      dudq                          = -1;
      dvdq                          = 0;
      startu                        = u2;
      startv                        = v1;
      CASE(1 << 2) + 0 : dudp       = -1;
      dvdp                          = 0;
      dudq                          = 0;
      dvdq                          = 1;
      startu                        = u2;
      startv                        = v1;
      D 61 CASE(1 << 2) + 1 : dudp = 0;
      dvdp                          = 1;
      dudq                          = 1;
      dvdq                          = 0;
      startu                        = u2;
      startv                        = v2;
      E 61
I 61 CASE(1 << 2) + 1 : dudp      = 0;
      dvdp                          = -1;
      dudq                          = -1;
      dvdq                          = 0;
      startu                        = u2;
      startv                        = v2;
      E 61 CASE(1 << 2) + 2 : dudp = 1;
      dvdp                          = 0;
      dudq                          = 0;
      dvdq                          = -1;
      startu                        = u1;
      startv                        = v2;
      D 61 CASE(1 << 2) + 3 : dudp = 0;
      dvdp                          = -1;
      dudq                          = -1;
      dvdq                          = 0;
      startu                        = u1;
      startv                        = v1;
      E 61
I 61 CASE(1 << 2) + 3 : dudp      = 0;
      dvdp                          = 1;
      dudq                          = 1;
      dvdq                          = 0;
      startu                        = u1;
      startv                        = v1;
      E 61 DEFAULT :
D 78 assert(FALSE);
      dudp = dvdp = dudq = dvdq = startu = startv = 0;
      E 78
I 78 abort();
      dudp = dvdp = dudq = dvdq = startu = startv = 0;
      E 78
    }
    dindp = dudp + wrapin * dvdp;
    dindq = dudq + wrapin * dvdq;

    rowin    = (UCHAR *)rin->buffer + startu + startv * wrapin;
    rowout   = (LPIXEL *)rout->buffer + newx + newy * wrapout;
    valout.m = 0xff;
    for (q = 0; q < qlast; q += factor) {
      pixin  = rowin;
      pixout = rowout;
      for (p = 0; p < plast; p += factor) {
        tmp = 0;
        in  = pixin;
        for (j = 0; j < factor; j += 2) {
          for (i = 0; i < factor; i += 2) {
            tmp += *in;
            in += dindp + dindq;
            tmp += *in;
            in += dindp - dindq;
          }
          in += 2 * dindq - factor * dindp;
        }
        tmp      = (tmp + fac_fac_4) >> fac_fac_2_bits;
        valout.r = valout.g = valout.b = tmp;
        *pixout++                      = valout;
        pixin += factor * dindp;
      }
      if (prest) {
        tmp = 0;
        for (j = 0; j < factor; j++)
          for (i = 0; i < prest; i++) {
            tmp += pixin[i * dindp + j * dindq];
          }
        tmp      = (tmp + fac_prest_2) / fac_prest;
        valout.r = valout.g = valout.b = tmp;
        *pixout++                      = valout;
      }
      rowin += factor * dindq;
      rowout += wrapout;
    }
    if (qrest) {
      pixin  = rowin;
      pixout = rowout;
      for (p = 0; p < plast; p += factor) {
        tmp = 0;
        for (j = 0; j < qrest; j++)
          for (i = 0; i < factor; i++) {
            tmp += pixin[i * dindp + j * dindq];
          }
        tmp      = (tmp + qrest_fac_2) / qrest_fac;
        valout.r = valout.g = valout.b = tmp;
        *pixout++                      = valout;
        pixin += factor * dindp;
      }
      if (prest) {
        tmp = 0;
        for (j = 0; j < qrest; j++)
          for (i = 0; i < prest; i++) {
            tmp += pixin[i * dindp + j * dindq];
          }
        tmp      = (tmp + qrest_prest_2) / qrest_prest;
        valout.r = valout.g = valout.b = tmp;
        *pixout++                      = valout;
      }
    }
  }

  /*---------------------------------------------------------------------------*/

  I 63 static void rop_copy_90_rgb_rgb16(RASTER * rin, RASTER * rout, int x1,
                                          int y1, int x2, int y2, int newx,
                                          int newy, int mirror, int ninety) {
    UCHAR *bufin, *bytein;
    USHORT *bufout, *pixout;
    int wrapin, wrapout;
    int u, v, lu, lv, su, sv, u00, v00, u0, v0, dindx, dudy, dvdy;
    int x, y, lx, ly;
    int u1, v1, u2, v2;

    mirror &= 1;
    ninety &= 3;

    if (!ninety && !mirror) {
      rop_copy(rin, rout, x1, y1, x2, y2, newx, newy);
      return;
    }

    u1 = x1;
    v1 = y1;
    u2 = x2;
    v2 = y2;

    su = u2 - u1;
    sv = v2 - v1;
    lu = u2 - u1 + 1;
    lv = v2 - v1 + 1;

    if (ninety & 1) {
      lx = lv;
      ly = lu;
    } else {
      lx = lu;
      ly = lv;
    }

    bufin   = rin->buffer;
    wrapin  = rin->wrap * 3; /* mi muovo di bytes */
    bufout  = rout->buffer;
    wrapout = rout->wrap;

    dudy = 0;
    dvdy = 0;

    switch ((mirror << 2) + ninety) {
      CASE(0 << 2) + 0 : u00       = u1;
      v00                          = v1;
      dvdy                         = 1;
      dindx                        = 3;
      D 81 CASE(0 << 2) + 1 : u00 = u1;
      v00                          = v1 + sv;
      dudy                         = 1;
      dindx                        = -wrapin;
      CASE(0 << 2) + 2 : u00       = u1 + su;
      v00                          = v1 + sv;
      dvdy                         = -1;
      dindx                        = -3;
      CASE(0 << 2) + 3 : u00       = u1 + su;
      v00                          = v1;
      dudy                         = -1;
      dindx                        = wrapin;
      CASE(1 << 2) + 0 : u00       = u1 + su;
      v00                          = v1;
      dvdy                         = 1;
      dindx                        = -3;
      CASE(1 << 2) + 1 : u00       = u1 + su;
      v00                          = v1 + sv;
      dudy                         = -1;
      dindx                        = -wrapin;
      CASE(1 << 2) + 2 : u00       = u1;
      v00                          = v1 + sv;
      dvdy                         = -1;
      dindx                        = 3;
      CASE(1 << 2) + 3 : u00       = u1;
      v00                          = v1;
      dudy                         = 1;
      dindx                        = wrapin;
      D 78 DEFAULT : assert(FALSE);
      u00 = v00 = dudy = dindx = 0;
      E 78
I 78 DEFAULT : abort();
      u00 = v00 = dudy = dindx = 0;
      E 78
    }

    for (u0 = u00, v0 = v00, y = newy; y < newy + ly;
         u0 += dudy, v0 += dvdy, y++) {
      bytein = bufin + u0 * 3 + v0 * wrapin;
      pixout = bufout + newx + y * wrapout;
      for (x = newx; x < newx + lx; x++) {
        *pixout++ = PIX_RGB16_FROM_BYTES(bytein[0], bytein[1], bytein[2]);
        bytein += dindx;
      }
    }
  }

  /*---------------------------------------------------------------------------*/

  static void rop_zoom_out_90_rgb_rgb16(
      RASTER * rin, RASTER * rout, int x1, int y1, int x2, int y2, int newx,
      int newy, int abs_zoom_level, int mirror, int ninety) {
    UCHAR *bufin, *rowin, *pixin, *in;
    int tmp_r, tmp_g, tmp_b;
    USHORT *bufout, *rowout, *pixout;
    int wrapin_bytes, wrapin_pixels, wrapout;
    int u1, v1, u2, v2;
    int u, v, lu, lv, startu, startv;
    int p, q, lp, lq;
    int dudp, dvdp, dudq, dvdq, dindp, dindq;
    int plast, qlast, prest, qrest, i, j;
    int factor, fac_fac_2_bits;
    int fac_fac, qrest_fac, fac_prest, qrest_prest;
    int fac_fac_2, qrest_fac_2, fac_prest_2, qrest_prest_2;
    int fac_fac_4;

    mirror &= 1;
    ninety &= 3;

    if (!ninety && !mirror) {
      rop_zoom_out(rin, rout, x1, y1, x2, y2, newx, newy, abs_zoom_level);
      return;
    }

    u1 = x1;
    v1 = y1;
    u2 = x2;
    v2 = y2;

    lu = u2 - u1 + 1;
    lv = v2 - v1 + 1;

    if (ninety & 1) {
      lp = lv;
      lq = lu;
    } else {
      lp = lu;
      lq = lv;
    }
    factor         = 1 << abs_zoom_level;
    prest          = lp & (factor - 1);
    qrest          = lq & (factor - 1);
    plast          = lp - prest;
    qlast          = lq - qrest;
    fac_fac        = factor * factor;
    fac_fac_2      = fac_fac >> 1;
    fac_fac_4      = fac_fac >> 2;
    fac_fac_2_bits = 2 * abs_zoom_level - 1;
    qrest_fac      = qrest * factor;
    qrest_fac_2    = qrest_fac >> 1;
    fac_prest      = factor * prest;
    fac_prest_2    = fac_prest >> 1;
    qrest_prest    = qrest * prest;
    qrest_prest_2  = qrest_prest >> 1;

    bufin         = rin->buffer;
    wrapin_pixels = rin->wrap;
    wrapin_bytes  = wrapin_pixels * 3;
    bufout        = rout->buffer;
    wrapout       = rout->wrap;

    switch ((mirror << 2) + ninety) {
      CASE(0 << 2) + 0 : dudp = 1;
      dvdp                    = 0;
      dudq                    = 0;
      dvdq                    = 1;
      startu                  = u1;
      startv                  = v1;
      CASE(0 << 2) + 1 : dudp = 0;
      dvdp                    = -1;
      dudq                    = 1;
      dvdq                    = 0;
      startu                  = u1;
      startv                  = v2;
      CASE(0 << 2) + 2 : dudp = -1;
      dvdp                    = 0;
      dudq                    = 0;
      dvdq                    = -1;
      startu                  = u2;
      startv                  = v2;
      CASE(0 << 2) + 3 : dudp = 0;
      dvdp                    = 1;
      dudq                    = -1;
      dvdq                    = 0;
      startu                  = u2;
      startv                  = v1;
      CASE(1 << 2) + 0 : dudp = -1;
      dvdp                    = 0;
      dudq                    = 0;
      dvdq                    = 1;
      startu                  = u2;
      startv                  = v1;
      CASE(1 << 2) + 1 : dudp = 0;
      dvdp                    = -1;
      dudq                    = -1;
      dvdq                    = 0;
      startu                  = u2;
      startv                  = v2;
      CASE(1 << 2) + 2 : dudp = 1;
      dvdp                    = 0;
      dudq                    = 0;
      dvdq                    = -1;
      startu                  = u1;
      startv                  = v2;
      CASE(1 << 2) + 3 : dudp = 0;
      dvdp                    = 1;
      dudq                    = 1;
      dvdq                    = 0;
      startu                  = u1;
      startv                  = v1;
    DEFAULT:
      D 78 assert(FALSE);
      dudp = dvdp = dudq = dvdq = startu = startv = 0;
      E 78
I 78 abort();
      dudp = dvdp = dudq = dvdq = startu = startv = 0;
      E 78
    }
    dindp = (dudp + wrapin_pixels * dvdp) * 3;
    dindq = (dudq + wrapin_pixels * dvdq) * 3;

    rowin  = (UCHAR *)rin->buffer + startu * 3 + startv * wrapin_bytes;
    rowout = (USHORT *)rout->buffer + newx + newy * wrapout;
    for (q = 0; q < qlast; q += factor) {
      pixin  = rowin;
      pixout = rowout;
      for (p = 0; p < plast; p += factor) {
        tmp_r = tmp_g = tmp_b = 0;
        in                    = pixin;
        for (j = 0; j < factor; j += 2) {
          for (i = 0; i < factor; i += 2) {
            tmp_r += in[0];
            tmp_g += in[1];
            tmp_b += in[2];
            in += dindp + dindq;
            tmp_r += in[0];
            tmp_g += in[1];
            tmp_b += in[2];
            in += dindp - dindq;
          }
          in += 2 * dindq - factor * dindp;
        }
        tmp_r     = (tmp_r + fac_fac_4) >> fac_fac_2_bits;
        tmp_g     = (tmp_g + fac_fac_4) >> fac_fac_2_bits;
        tmp_b     = (tmp_b + fac_fac_4) >> fac_fac_2_bits;
        *pixout++ = PIX_RGB16_FROM_BYTES(tmp_r, tmp_g, tmp_b);
        pixin += factor * dindp;
      }
      if (prest) {
        tmp_r = tmp_g = tmp_b = 0;
        for (j = 0; j < factor; j++)
          for (i = 0; i < prest; i++) {
            tmp_r += pixin[i * dindp + j * dindq];
            tmp_g += pixin[i * dindp + j * dindq + 1];
            tmp_b += pixin[i * dindp + j * dindq + 2];
          }
        tmp_r     = (tmp_r + fac_prest_2) / fac_prest;
        tmp_g     = (tmp_g + fac_prest_2) / fac_prest;
        tmp_b     = (tmp_b + fac_prest_2) / fac_prest;
        *pixout++ = PIX_RGB16_FROM_BYTES(tmp_r, tmp_g, tmp_b);
      }
      rowin += factor * dindq;
      rowout += wrapout;
    }
    if (qrest) {
      pixin  = rowin;
      pixout = rowout;
      for (p = 0; p < plast; p += factor) {
        tmp_r = tmp_g = tmp_b = 0;
        for (j = 0; j < qrest; j++)
          for (i = 0; i < factor; i++) {
            E 81
I 81 CASE(0 << 2) + 1 : u00      = u1;
            v00                    = v1 + sv;
            dudy                   = 1;
            dindx                  = -wrapin;
            CASE(0 << 2) + 2 : u00 = u1 + su;
            v00                    = v1 + sv;
            dvdy                   = -1;
            dindx                  = -3;
            CASE(0 << 2) + 3 : u00 = u1 + su;
            v00                    = v1;
            dudy                   = -1;
            dindx                  = wrapin;
            CASE(1 << 2) + 0 : u00 = u1 + su;
            v00                    = v1;
            dvdy                   = 1;
            dindx                  = -3;
            CASE(1 << 2) + 1 : u00 = u1 + su;
            v00                    = v1 + sv;
            dudy                   = -1;
            dindx                  = -wrapin;
            CASE(1 << 2) + 2 : u00 = u1;
            v00                    = v1 + sv;
            dvdy                   = -1;
            dindx                  = 3;
            CASE(1 << 2) + 3 : u00 = u1;
            v00                    = v1;
            dudy                   = 1;
            dindx                  = wrapin;
          DEFAULT:
            abort();
            u00 = v00 = dudy = dindx = 0;
          }

        for (u0 = u00, v0 = v00, y = newy; y < newy + ly;
             u0 += dudy, v0 += dvdy, y++) {
          bytein = bufin + u0 * 3 + v0 * wrapin;
          pixout = bufout + newx + y * wrapout;
          for (x = newx; x < newx + lx; x++) {
            *pixout++ = PIX_RGB16_FROM_BYTES(bytein[0], bytein[1], bytein[2]);
            bytein += dindx;
          }
        }
      }

      /*---------------------------------------------------------------------------*/

      static void rop_zoom_out_90_rgb_rgb16(
          RASTER * rin, RASTER * rout, int x1, int y1, int x2, int y2, int newx,
          int newy, int abs_zoom_level, int mirror, int ninety) {
        UCHAR *bufin, *rowin, *pixin, *in;
        int tmp_r, tmp_g, tmp_b;
        USHORT *bufout, *rowout, *pixout;
        int wrapin_bytes, wrapin_pixels, wrapout;
        int u1, v1, u2, v2;
        int u, v, lu, lv, startu, startv;
        int p, q, lp, lq;
        int dudp, dvdp, dudq, dvdq, dindp, dindq;
        int plast, qlast, prest, qrest, i, j;
        int factor, fac_fac_2_bits;
        int fac_fac, qrest_fac, fac_prest, qrest_prest;
        int fac_fac_2, qrest_fac_2, fac_prest_2, qrest_prest_2;
        int fac_fac_4;

        mirror &= 1;
        ninety &= 3;

        if (!ninety && !mirror) {
          rop_zoom_out(rin, rout, x1, y1, x2, y2, newx, newy, abs_zoom_level);
          return;
        }

        u1 = x1;
        v1 = y1;
        u2 = x2;
        v2 = y2;

        lu = u2 - u1 + 1;
        lv = v2 - v1 + 1;

        if (ninety & 1) {
          lp = lv;
          lq = lu;
        } else {
          lp = lu;
          lq = lv;
        }
        factor         = 1 << abs_zoom_level;
        prest          = lp & (factor - 1);
        qrest          = lq & (factor - 1);
        plast          = lp - prest;
        qlast          = lq - qrest;
        fac_fac        = factor * factor;
        fac_fac_2      = fac_fac >> 1;
        fac_fac_4      = fac_fac >> 2;
        fac_fac_2_bits = 2 * abs_zoom_level - 1;
        qrest_fac      = qrest * factor;
        qrest_fac_2    = qrest_fac >> 1;
        fac_prest      = factor * prest;
        fac_prest_2    = fac_prest >> 1;
        qrest_prest    = qrest * prest;
        qrest_prest_2  = qrest_prest >> 1;

        bufin         = rin->buffer;
        wrapin_pixels = rin->wrap;
        wrapin_bytes  = wrapin_pixels * 3;
        bufout        = rout->buffer;
        wrapout       = rout->wrap;

        switch ((mirror << 2) + ninety) {
          CASE(0 << 2) + 0 : dudp = 1;
          dvdp                    = 0;
          dudq                    = 0;
          dvdq                    = 1;
          startu                  = u1;
          startv                  = v1;
          CASE(0 << 2) + 1 : dudp = 0;
          dvdp                    = -1;
          dudq                    = 1;
          dvdq                    = 0;
          startu                  = u1;
          startv                  = v2;
          CASE(0 << 2) + 2 : dudp = -1;
          dvdp                    = 0;
          dudq                    = 0;
          dvdq                    = -1;
          startu                  = u2;
          startv                  = v2;
          CASE(0 << 2) + 3 : dudp = 0;
          dvdp                    = 1;
          dudq                    = -1;
          dvdq                    = 0;
          startu                  = u2;
          startv                  = v1;
          CASE(1 << 2) + 0 : dudp = -1;
          dvdp                    = 0;
          dudq                    = 0;
          dvdq                    = 1;
          startu                  = u2;
          startv                  = v1;
          CASE(1 << 2) + 1 : dudp = 0;
          dvdp                    = -1;
          dudq                    = -1;
          dvdq                    = 0;
          startu                  = u2;
          startv                  = v2;
          CASE(1 << 2) + 2 : dudp = 1;
          dvdp                    = 0;
          dudq                    = 0;
          dvdq                    = -1;
          startu                  = u1;
          startv                  = v2;
          CASE(1 << 2) + 3 : dudp = 0;
          dvdp                    = 1;
          dudq                    = 1;
          dvdq                    = 0;
          startu                  = u1;
          startv                  = v1;
        DEFAULT:
          abort();
          dudp = dvdp = dudq = dvdq = startu = startv = 0;
        }
        dindp = (dudp + wrapin_pixels * dvdp) * 3;
        dindq = (dudq + wrapin_pixels * dvdq) * 3;

        rowin  = (UCHAR *)rin->buffer + startu * 3 + startv * wrapin_bytes;
        rowout = (USHORT *)rout->buffer + newx + newy * wrapout;
        for (q = 0; q < qlast; q += factor) {
          pixin  = rowin;
          pixout = rowout;
          for (p = 0; p < plast; p += factor) {
            tmp_r = tmp_g = tmp_b = 0;
            in                    = pixin;
            for (j = 0; j < factor; j += 2) {
              for (i = 0; i < factor; i += 2) {
                tmp_r += in[0];
                tmp_g += in[1];
                tmp_b += in[2];
                in += dindp + dindq;
                tmp_r += in[0];
                tmp_g += in[1];
                tmp_b += in[2];
                in += dindp - dindq;
              }
              in += 2 * dindq - factor * dindp;
            }
            tmp_r     = (tmp_r + fac_fac_4) >> fac_fac_2_bits;
            tmp_g     = (tmp_g + fac_fac_4) >> fac_fac_2_bits;
            tmp_b     = (tmp_b + fac_fac_4) >> fac_fac_2_bits;
            *pixout++ = PIX_RGB16_FROM_BYTES(tmp_r, tmp_g, tmp_b);
            pixin += factor * dindp;
          }
          if (prest) {
            tmp_r = tmp_g = tmp_b = 0;
            for (j = 0; j < factor; j++)
              for (i = 0; i < prest; i++) {
                tmp_r += pixin[i * dindp + j * dindq];
                tmp_g += pixin[i * dindp + j * dindq + 1];
                tmp_b += pixin[i * dindp + j * dindq + 2];
              }
            tmp_r     = (tmp_r + fac_prest_2) / fac_prest;
            tmp_g     = (tmp_g + fac_prest_2) / fac_prest;
            tmp_b     = (tmp_b + fac_prest_2) / fac_prest;
            *pixout++ = PIX_RGB16_FROM_BYTES(tmp_r, tmp_g, tmp_b);
          }
          rowin += factor * dindq;
          rowout += wrapout;
        }
        if (qrest) {
          pixin  = rowin;
          pixout = rowout;
          for (p = 0; p < plast; p += factor) {
            tmp_r = tmp_g = tmp_b = 0;
            for (j = 0; j < qrest; j++)
              for (i = 0; i < factor; i++) {
                E 81 tmp_r += pixin[i * dindp + j * dindq];
                tmp_g += pixin[i * dindp + j * dindq + 1];
                tmp_b += pixin[i * dindp + j * dindq + 2];
              }
            tmp_r     = (tmp_r + qrest_fac_2) / qrest_fac;
            tmp_g     = (tmp_g + qrest_fac_2) / qrest_fac;
            tmp_b     = (tmp_b + qrest_fac_2) / qrest_fac;
            *pixout++ = PIX_RGB16_FROM_BYTES(tmp_r, tmp_g, tmp_b);
            pixin += factor * dindp;
          }
          if (prest) {
            tmp_r = tmp_g = tmp_b = 0;
            for (j = 0; j < qrest; j++)
              for (i = 0; i < prest; i++) {
                tmp_r += pixin[i * dindp + j * dindq];
                tmp_g += pixin[i * dindp + j * dindq + 1];
                tmp_b += pixin[i * dindp + j * dindq + 2];
              }
            tmp_r     = (tmp_r + qrest_prest_2) / qrest_prest;
            tmp_g     = (tmp_g + qrest_prest_2) / qrest_prest;
            tmp_b     = (tmp_b + qrest_prest_2) / qrest_prest;
            *pixout++ = PIX_RGB16_FROM_BYTES(tmp_r, tmp_g, tmp_b);
          }
        }
      }

      /*---------------------------------------------------------------------------*/

      E 63
E 57 static void rop_copy_90_rgb_rgbm(RASTER * rin, RASTER * rout, int x1,
                                        int y1, int x2, int y2, int newx,
                                        int newy, int mirror, int ninety) {
        UCHAR *bufin, *bytein, *appo;
        LPIXEL *bufout, *pixout, tmp;
        int wrapin, wrapout;
        int u, v, lu, lv, su, sv, u00, v00, u0, v0, dindx, dudy, dvdy;
        int x, y, lx, ly;
        int u1, v1, u2, v2;

        mirror &= 1;
        ninety &= 3;

        if (!ninety && !mirror) {
          rop_copy(rin, rout, x1, y1, x2, y2, newx, newy);
          return;
        }

        u1 = x1;
        v1 = y1;
        u2 = x2;
        v2 = y2;

        su = u2 - u1;
        sv = v2 - v1;
        lu = u2 - u1 + 1;
        lv = v2 - v1 + 1;

        if (ninety & 1) {
          lx = lv;
          ly = lu;
        } else {
          lx = lu;
          ly = lv;
        }

        bufin   = rin->buffer;
        wrapin  = rin->wrap * 3; /* mi muovo di bytes */
        bufout  = rout->buffer;
        wrapout = rout->wrap;

        dudy = 0;
        dvdy = 0;

        switch ((mirror << 2) + ninety) {
          CASE(0 << 2) + 0 : u00 = u1;
          v00                    = v1;
          dvdy                   = 1;
          dindx                  = 3;
          CASE(0 << 2) + 1 : u00 = u1;
          v00                    = v1 + sv;
          dudy                   = 1;
          dindx                  = -wrapin;
          CASE(0 << 2) + 2 : u00 = u1 + su;
          v00                    = v1 + sv;
          dvdy                   = -1;
          dindx                  = -3;
          CASE(0 << 2) + 3 : u00 = u1 + su;
          v00                    = v1;
          dudy                   = -1;
          dindx                  = wrapin;
          CASE(1 << 2) + 0 : u00 = u1 + su;
          v00                    = v1;
          dvdy                   = 1;
          dindx                  = -3;
          CASE(1 << 2) + 1 : u00 = u1 + su;
          v00                    = v1 + sv;
          dudy                   = -1;
          dindx                  = -wrapin;
          CASE(1 << 2) + 2 : u00 = u1;
          v00                    = v1 + sv;
          dvdy                   = -1;
          dindx                  = 3;
          CASE(1 << 2) + 3 : u00 = u1;
          v00                    = v1;
          dudy                   = 1;
          dindx                  = wrapin;
          D 78 DEFAULT : assert(FALSE);
          u00 = v00 = dudy = dindx = 0;
          E 78
I 78 DEFAULT : abort();
          u00 = v00 = dudy = dindx = 0;
          E 78
        }

        for (u0 = u00, v0 = v00, y = newy; y < newy + ly;
             u0 += dudy, v0 += dvdy, y++) {
          bytein = bufin + u0 * 3 + v0 * wrapin;
          pixout = bufout + newx + y * wrapout;
          for (x = newx; x < newx + lx; x++) {
            appo      = bytein;
            tmp.r     = *appo++;
            tmp.g     = *appo++;
            tmp.b     = *appo;
            tmp.m     = 0xff;
            *pixout++ = tmp;
            bytein += dindx;
          }
        }
      }

      /*---------------------------------------------------------------------------*/

      I 63 static void rop_zoom_out_90_rgb_rgbm(
          RASTER * rin, RASTER * rout, int x1, int y1, int x2, int y2, int newx,
          int newy, int abs_zoom_level, int mirror, int ninety) {
        UCHAR *bufin, *rowin, *pixin, *in;
        int tmp_r, tmp_g, tmp_b;
        LPIXEL *bufout, *rowout, *pixout, valout;
        int wrapin_bytes, wrapin_pixels, wrapout;
        int u1, v1, u2, v2;
        int u, v, lu, lv, startu, startv;
        int p, q, lp, lq;
        int dudp, dvdp, dudq, dvdq, dindp, dindq;
        int plast, qlast, prest, qrest, i, j;
        int factor, fac_fac_2_bits;
        int fac_fac, qrest_fac, fac_prest, qrest_prest;
        int fac_fac_2, qrest_fac_2, fac_prest_2, qrest_prest_2;
        int fac_fac_4;

        mirror &= 1;
        ninety &= 3;

        if (!ninety && !mirror) {
          rop_zoom_out(rin, rout, x1, y1, x2, y2, newx, newy, abs_zoom_level);
          return;
        }

        u1 = x1;
        v1 = y1;
        u2 = x2;
        v2 = y2;

        lu = u2 - u1 + 1;
        lv = v2 - v1 + 1;

        if (ninety & 1) {
          lp = lv;
          lq = lu;
        } else {
          lp = lu;
          lq = lv;
        }
        factor         = 1 << abs_zoom_level;
        prest          = lp & (factor - 1);
        qrest          = lq & (factor - 1);
        plast          = lp - prest;
        qlast          = lq - qrest;
        fac_fac        = factor * factor;
        fac_fac_2      = fac_fac >> 1;
        fac_fac_4      = fac_fac >> 2;
        fac_fac_2_bits = 2 * abs_zoom_level - 1;
        qrest_fac      = qrest * factor;
        qrest_fac_2    = qrest_fac >> 1;
        fac_prest      = factor * prest;
        fac_prest_2    = fac_prest >> 1;
        qrest_prest    = qrest * prest;
        qrest_prest_2  = qrest_prest >> 1;

        bufin         = rin->buffer;
        wrapin_pixels = rin->wrap;
        wrapin_bytes  = wrapin_pixels * 3;
        bufout        = rout->buffer;
        wrapout       = rout->wrap;

        switch ((mirror << 2) + ninety) {
          CASE(0 << 2) + 0 : dudp = 1;
          dvdp                    = 0;
          dudq                    = 0;
          dvdq                    = 1;
          startu                  = u1;
          startv                  = v1;
          CASE(0 << 2) + 1 : dudp = 0;
          dvdp                    = -1;
          dudq                    = 1;
          dvdq                    = 0;
          startu                  = u1;
          startv                  = v2;
          CASE(0 << 2) + 2 : dudp = -1;
          dvdp                    = 0;
          dudq                    = 0;
          dvdq                    = -1;
          startu                  = u2;
          startv                  = v2;
          CASE(0 << 2) + 3 : dudp = 0;
          dvdp                    = 1;
          dudq                    = -1;
          dvdq                    = 0;
          startu                  = u2;
          startv                  = v1;
          CASE(1 << 2) + 0 : dudp = -1;
          dvdp                    = 0;
          dudq                    = 0;
          dvdq                    = 1;
          startu                  = u2;
          startv                  = v1;
          CASE(1 << 2) + 1 : dudp = 0;
          dvdp                    = -1;
          dudq                    = -1;
          dvdq                    = 0;
          startu                  = u2;
          startv                  = v2;
          CASE(1 << 2) + 2 : dudp = 1;
          dvdp                    = 0;
          dudq                    = 0;
          dvdq                    = -1;
          startu                  = u1;
          startv                  = v2;
          CASE(1 << 2) + 3 : dudp = 0;
          dvdp                    = 1;
          dudq                    = 1;
          dvdq                    = 0;
          startu                  = u1;
          startv                  = v1;
        DEFAULT:
          D 78 assert(FALSE);
          dudp = dvdp = dudq = dvdq = startu = startv = 0;
          E 78
I 78 abort();
          dudp = dvdp = dudq = dvdq = startu = startv = 0;
          E 78
        }
        dindp = (dudp + wrapin_pixels * dvdp) * 3;
        dindq = (dudq + wrapin_pixels * dvdq) * 3;

        rowin    = (UCHAR *)rin->buffer + startu * 3 + startv * wrapin_bytes;
        rowout   = (LPIXEL *)rout->buffer + newx + newy * wrapout;
        valout.m = 0xff;
        for (q = 0; q < qlast; q += factor) {
          pixin  = rowin;
          pixout = rowout;
          for (p = 0; p < plast; p += factor) {
            tmp_r = tmp_g = tmp_b = 0;
            in                    = pixin;
            for (j = 0; j < factor; j += 2) {
              for (i = 0; i < factor; i += 2) {
                tmp_r += in[0];
                tmp_g += in[1];
                tmp_b += in[2];
                in += dindp + dindq;
                tmp_r += in[0];
                tmp_g += in[1];
                tmp_b += in[2];
                in += dindp - dindq;
              }
              in += 2 * dindq - factor * dindp;
            }
            valout.r  = (tmp_r + fac_fac_4) >> fac_fac_2_bits;
            valout.g  = (tmp_g + fac_fac_4) >> fac_fac_2_bits;
            valout.b  = (tmp_b + fac_fac_4) >> fac_fac_2_bits;
            *pixout++ = valout;
            pixin += factor * dindp;
          }
          if (prest) {
            tmp_r = tmp_g = tmp_b = 0;
            for (j = 0; j < factor; j++)
              for (i = 0; i < prest; i++) {
                tmp_r += pixin[i * dindp + j * dindq];
                tmp_g += pixin[i * dindp + j * dindq + 1];
                tmp_b += pixin[i * dindp + j * dindq + 2];
              }
            valout.r  = (tmp_r + fac_prest_2) / fac_prest;
            valout.g  = (tmp_g + fac_prest_2) / fac_prest;
            valout.b  = (tmp_b + fac_prest_2) / fac_prest;
            *pixout++ = valout;
          }
          rowin += factor * dindq;
          rowout += wrapout;
        }
        if (qrest) {
          pixin  = rowin;
          pixout = rowout;
          for (p = 0; p < plast; p += factor) {
            tmp_r = tmp_g = tmp_b = 0;
            for (j = 0; j < qrest; j++)
              for (i = 0; i < factor; i++) {
                tmp_r += pixin[i * dindp + j * dindq];
                tmp_g += pixin[i * dindp + j * dindq + 1];
                tmp_b += pixin[i * dindp + j * dindq + 2];
              }
            valout.r  = (tmp_r + qrest_fac_2) / qrest_fac;
            valout.g  = (tmp_g + qrest_fac_2) / qrest_fac;
            valout.b  = (tmp_b + qrest_fac_2) / qrest_fac;
            *pixout++ = valout;
            pixin += factor * dindp;
          }
          if (prest) {
            tmp_r = tmp_g = tmp_b = 0;
            for (j = 0; j < qrest; j++)
              for (i = 0; i < prest; i++) {
                tmp_r += pixin[i * dindp + j * dindq];
                tmp_g += pixin[i * dindp + j * dindq + 1];
                tmp_b += pixin[i * dindp + j * dindq + 2];
              }
            valout.r  = (tmp_r + qrest_prest_2) / qrest_prest;
            valout.g  = (tmp_g + qrest_prest_2) / qrest_prest;
            valout.b  = (tmp_b + qrest_prest_2) / qrest_prest;
            *pixout++ = valout;
          }
        }
      }

      /*---------------------------------------------------------------------------*/

      E 63 static void rop_copy_90_bw_gr8(RASTER * rin, RASTER * rout, int x1,
                                           int y1, int x2, int y2,
E 56 int newx, int newy, int mirror, int ninety) {
        UCHAR *bufin, *bufout, *bytein, *byteout;
        int bytewrapin, bitoffsin, wrapout;
        int value_for_0, value_for_1;
        int u, v, lu, lv, su, sv, u00, v00, u0, v0, dudy, dvdy, dudx, dvdx;
        int x, y, lx, ly;
        int u1, v1, u2, v2;

        mirror &= 1;
        ninety &= 3;

        if (!ninety && !mirror) {
          rop_copy(rin, rout, x1, y1, x2, y2, newx, newy);
          return;
          E 55
        }

        switch (rin->type) {
          CASE RAS_WB : value_for_0 = 255;
          value_for_1               = 0;
          CASE RAS_BW : value_for_0 = 0;
          value_for_1               = 255;
        DEFAULT:
          printf("bad raster type in rop_copy_90_bw_gr8\n");
          return;
        }

        u1 = x1;
        v1 = y1;
        u2 = x2;
        v2 = y2;

        su = u2 - u1;
        sv = v2 - v1;
        lu = u2 - u1 + 1;
        lv = v2 - v1 + 1;

        if (ninety & 1) {
          lx = lv;
          ly = lu;
        } else {
          lx = lu;
          ly = lv;
        }

        bufin            = rin->buffer;
        D 47 bytewrapin = (rin->wrap + 7) / 8;
        E 47
I 47 bytewrapin        = (rin->wrap + 7) >> 3;
        E 47 bitoffsin  = rin->bit_offs;
        bufout           = rout->buffer;
        wrapout          = rout->wrap;

        dudx = 0;
        dudy = 0;
        dvdx = 0;
        dvdy = 0;

        switch ((mirror << 2) + ninety) {
          CASE(0 << 2) + 0 : u00 = u1;
          v00                    = v1;
          dudx                   = 1;
          dvdy                   = 1;
          CASE(0 << 2) + 1 : u00 = u1;
          v00                    = v1 + sv;
          dudy                   = 1;
          dvdx                   = -1;
          CASE(0 << 2) + 2 : u00 = u1 + su;
          v00                    = v1 + sv;
          dudx                   = -1;
          dvdy                   = -1;
          CASE(0 << 2) + 3 : u00 = u1 + su;
          v00                    = v1;
          dudy                   = -1;
          dvdx                   = 1;
          CASE(1 << 2) + 0 : u00 = u1 + su;
          v00                    = v1;
          dudx                   = -1;
          dvdy                   = 1;
          CASE(1 << 2) + 1 : u00 = u1 + su;
          v00                    = v1 + sv;
          dudy                   = -1;
          dvdx                   = -1;
          CASE(1 << 2) + 2 : u00 = u1;
          v00                    = v1 + sv;
          dudx                   = 1;
          dvdy                   = -1;
          CASE(1 << 2) + 3 : u00 = u1;
          v00                    = v1;
          dudy                   = 1;
          dvdx                   = 1;
          I 50
D 78 DEFAULT : assert(FALSE);
          u00 = v00 = dudy = dvdx = 0;
          E 78
I 78 DEFAULT : abort();
          u00 = v00 = dudy = dvdx = 0;
          E 78
E 50
        }

        if (dudx)
          for (u0 = u00, v0 = v00, y = newy; y < newy + ly; v0 += dvdy, y++) {
            u       = u0;
            v       = v0;
            byteout = bufout + newx + y * wrapout;
            for (x = newx; x < newx + lx; u += dudx, x++) {
              if (GET_BIT(u, v, bufin, bytewrapin, bitoffsin))
                *byteout++ = value_for_1;
              else
                *byteout++ = value_for_0;
            }
          }
        else
          for (u0 = u00, v0 = v00, y = newy; y < newy + ly; u0 += dudy, y++) {
            u       = u0;
            v       = v0;
            byteout = bufout + newx + y * wrapout;
            for (x = newx; x < newx + lx; v += dvdx, x++) {
              if (GET_BIT(u, v, bufin, bytewrapin, bitoffsin))
                *byteout++ = value_for_1;
              else
                *byteout++ = value_for_0;
              I 21
            }
          }
        D 55

#ifdef DEBUG
            STW_STOP(0) STW_PRINT(stdout)
#endif
E 55
      }

      /*---------------------------------------------------------------------------*/

      static void rop_copy_90_bw_cm16(RASTER * rin, RASTER * rout, int x1,
                                      int y1, int x2, int y2, int newx,
                                      int newy, int mirror, int ninety) {
        UCHAR *bufin, *bytein;
        USHORT *bufout, *pixout;
        LPIXEL *cmap;
        int bytewrapin, bitoffsin, wrapout, cmap_offset;
        USHORT value_for_0, value_for_1, value_tmp;
        int u, v, lu, lv, su, sv, u00, v00, u0, v0, dudy, dvdy, dudx, dvdx;
        int x, y, lx, ly;
        int u1, v1, u2, v2;

        D 55
#ifdef DEBUG
            STW_INIT(0, "rop_copy_90_bw_gr8") STW_START(0)
#endif

E 55 mirror &= 1;
        ninety &= 3;

        if (!ninety && !mirror) {
          rop_copy(rin, rout, x1, y1, x2, y2, newx, newy);
          return;
        }
        D 84 if (!rout->cmap.buffer) {
          msg(MSG_IE, "rop_copy_90_bw_cm16: missing color map");
          return;
        }
        E 84 cmap        = rout->cmap.buffer;
        D 56 cmap_offset = rout->cmap.offset;
        E 56
I 56 cmap_offset        = rout->cmap.info.offset_mask;
        E 56 if (cmap[0].r == 0) {
          value_for_0 = cmap_offset;
          value_for_1 = cmap_offset + 255;
        }
        else {
          value_for_0 = cmap_offset + 255;
          value_for_1 = cmap_offset;
        }
        if (rin->type == RAS_WB) {
          value_tmp   = value_for_0;
          value_for_0 = value_for_1;
          value_for_1 = value_tmp;
          I 22
        }

        u1 = x1;
        v1 = y1;
        u2 = x2;
        v2 = y2;

        su = u2 - u1;
        sv = v2 - v1;
        lu = u2 - u1 + 1;
        lv = v2 - v1 + 1;

        if (ninety & 1) {
          lx = lv;
          ly = lu;
        } else {
          lx = lu;
          ly = lv;
        }

        bufin            = rin->buffer;
        D 47 bytewrapin = (rin->wrap + 7) / 8;
        E 47
I 47 bytewrapin        = (rin->wrap + 7) >> 3;
        E 47 bitoffsin  = rin->bit_offs;
        bufout           = rout->buffer;
        wrapout          = rout->wrap;

        dudx = 0;
        dudy = 0;
        dvdx = 0;
        dvdy = 0;

        switch ((mirror << 2) + ninety) {
          CASE(0 << 2) + 0 : u00 = u1;
          v00                    = v1;
          dudx                   = 1;
          dvdy                   = 1;
          CASE(0 << 2) + 1 : u00 = u1;
          v00                    = v1 + sv;
          dudy                   = 1;
          dvdx                   = -1;
          CASE(0 << 2) + 2 : u00 = u1 + su;
          v00                    = v1 + sv;
          dudx                   = -1;
          dvdy                   = -1;
          CASE(0 << 2) + 3 : u00 = u1 + su;
          v00                    = v1;
          dudy                   = -1;
          dvdx                   = 1;
          CASE(1 << 2) + 0 : u00 = u1 + su;
          v00                    = v1;
          dudx                   = -1;
          dvdy                   = 1;
          CASE(1 << 2) + 1 : u00 = u1 + su;
          v00                    = v1 + sv;
          dudy                   = -1;
          dvdx                   = -1;
          CASE(1 << 2) + 2 : u00 = u1;
          v00                    = v1 + sv;
          dudx                   = 1;
          dvdy                   = -1;
          CASE(1 << 2) + 3 : u00 = u1;
          v00                    = v1;
          dudy                   = 1;
          dvdx                   = 1;
          I 50
D 78 DEFAULT : assert(FALSE);
          u00 = v00 = dudy = dvdx = 0;
          E 78
I 78 DEFAULT : abort();
          u00 = v00 = dudy = dvdx = 0;
          E 78
E 50
        }

        if (dudx)
          for (u0 = u00, v0 = v00, y = newy; y < newy + ly; v0 += dvdy, y++) {
            u      = u0;
            v      = v0;
            pixout = bufout + newx + y * wrapout;
            for (x = newx; x < newx + lx; u += dudx, x++) {
              if (GET_BIT(u, v, bufin, bytewrapin, bitoffsin))
                *pixout++ = value_for_1;
              else
                *pixout++ = value_for_0;
            }
          }
        else
          for (u0 = u00, v0 = v00, y = newy; y < newy + ly; u0 += dudy, y++) {
            u      = u0;
            v      = v0;
            pixout = bufout + newx + y * wrapout;
            for (x = newx; x < newx + lx; v += dvdx, x++) {
              if (GET_BIT(u, v, bufin, bytewrapin, bitoffsin))
                *pixout++ = value_for_1;
              else
                *pixout++ = value_for_0;
            }
          }
        I 55
      }

      /*---------------------------------------------------------------------------*/

      I 84 static void rop_copy_90_bw_cm24(RASTER * rin, RASTER * rout, int x1,
                                            int y1, int x2, int y2, int newx,
                                            int newy, int mirror, int ninety) {
        UCHAR *bufin, *bytein;
        ULONG *bufout, *pixout;
        LPIXEL *penmap, *colmap;
        int bytewrapin, bitoffsin, wrapout;
        ULONG value_for_0, value_for_1, value_tmp;
        int u, v, lu, lv, su, sv, u00, v00, u0, v0, dudy, dvdy, dudx, dvdx;
        int x, y, lx, ly;
        int u1, v1, u2, v2;

        mirror &= 1;
        ninety &= 3;

        if (!ninety && !mirror) {
          rop_copy(rin, rout, x1, y1, x2, y2, newx, newy);
          return;
        }
        penmap = rout->cmap.penbuffer;
        colmap = rout->cmap.colbuffer;
        if (penmap[0].r + colmap[0].r == 0) {
          value_for_0 = 0;
          value_for_1 = 255;
        } else {
          value_for_0 = 255;
          value_for_1 = 0;
        }
        if (rin->type == RAS_WB) {
          value_tmp   = value_for_0;
          value_for_0 = value_for_1;
          value_for_1 = value_tmp;
        }

        u1 = x1;
        v1 = y1;
        u2 = x2;
        v2 = y2;

        su = u2 - u1;
        sv = v2 - v1;
        lu = u2 - u1 + 1;
        lv = v2 - v1 + 1;

        if (ninety & 1) {
          lx = lv;
          ly = lu;
        } else {
          lx = lu;
          ly = lv;
        }

        bufin      = rin->buffer;
        bytewrapin = (rin->wrap + 7) >> 3;
        bitoffsin  = rin->bit_offs;
        bufout     = rout->buffer;
        wrapout    = rout->wrap;

        dudx = 0;
        dudy = 0;
        dvdx = 0;
        dvdy = 0;

        switch ((mirror << 2) + ninety) {
          CASE(0 << 2) + 0 : u00 = u1;
          v00                    = v1;
          dudx                   = 1;
          dvdy                   = 1;
          CASE(0 << 2) + 1 : u00 = u1;
          v00                    = v1 + sv;
          dudy                   = 1;
          dvdx                   = -1;
          CASE(0 << 2) + 2 : u00 = u1 + su;
          v00                    = v1 + sv;
          dudx                   = -1;
          dvdy                   = -1;
          CASE(0 << 2) + 3 : u00 = u1 + su;
          v00                    = v1;
          dudy                   = -1;
          dvdx                   = 1;
          CASE(1 << 2) + 0 : u00 = u1 + su;
          v00                    = v1;
          dudx                   = -1;
          dvdy                   = 1;
          CASE(1 << 2) + 1 : u00 = u1 + su;
          v00                    = v1 + sv;
          dudy                   = -1;
          dvdx                   = -1;
          CASE(1 << 2) + 2 : u00 = u1;
          v00                    = v1 + sv;
          dudx                   = 1;
          dvdy                   = -1;
          CASE(1 << 2) + 3 : u00 = u1;
          v00                    = v1;
          dudy                   = 1;
          dvdx                   = 1;
        DEFAULT:
          abort();
          u00 = v00 = dudy = dvdx = 0;
        }

        if (dudx)
          for (u0 = u00, v0 = v00, y = newy; y < newy + ly; v0 += dvdy, y++) {
            u      = u0;
            v      = v0;
            pixout = bufout + newx + y * wrapout;
            for (x = newx; x < newx + lx; u += dudx, x++) {
              if (GET_BIT(u, v, bufin, bytewrapin, bitoffsin))
                *pixout++ = value_for_1;
              else
                *pixout++ = value_for_0;
            }
          }
        else
          for (u0 = u00, v0 = v00, y = newy; y < newy + ly; u0 += dudy, y++) {
            u      = u0;
            v      = v0;
            pixout = bufout + newx + y * wrapout;
            for (x = newx; x < newx + lx; v += dvdx, x++) {
              if (GET_BIT(u, v, bufin, bytewrapin, bitoffsin))
                *pixout++ = value_for_1;
              else
                *pixout++ = value_for_0;
            }
          }
      }

      /*---------------------------------------------------------------------------*/

      E 84 static void rop_copy_90_bw_rgb16(
          RASTER * rin, RASTER * rout, int x1, int y1, int x2, int y2, int newx,
          int newy, int mirror, int ninety) {
        UCHAR *bufin, *bytein;
        USHORT *bufout, *pixout;
        int bytewrapin, bitoffsin, wrapout;
        USHORT value_for_0, value_for_1;
        int u, v, lu, lv, su, sv, u00, v00, u0, v0, dudy, dvdy, dudx, dvdx;
        int x, y, lx, ly;
        int u1, v1, u2, v2;

        mirror &= 1;
        ninety &= 3;

        if (!ninety && !mirror) {
          rop_copy(rin, rout, x1, y1, x2, y2, newx, newy);
          return;
        }

        switch (rin->type) {
          CASE RAS_WB : value_for_0 = 0xffff;
          value_for_1               = 0x0000;
          CASE RAS_BW : value_for_0 = 0x0000;
          value_for_1               = 0xffff;
        DEFAULT:
          printf("bad raster type in rop_copy_90_bw_rgb16\n");
          return;
        }

        u1 = x1;
        v1 = y1;
        u2 = x2;
        v2 = y2;

        su = u2 - u1;
        sv = v2 - v1;
        lu = u2 - u1 + 1;
        lv = v2 - v1 + 1;

        if (ninety & 1) {
          lx = lv;
          ly = lu;
        } else {
          lx = lu;
          ly = lv;
        }

        bufin      = rin->buffer;
        bytewrapin = (rin->wrap + 7) >> 3;
        bitoffsin  = rin->bit_offs;
        bufout     = rout->buffer;
        wrapout    = rout->wrap;

        dudx = 0;
        dudy = 0;
        dvdx = 0;
        dvdy = 0;

        switch ((mirror << 2) + ninety) {
          CASE(0 << 2) + 0 : u00 = u1;
          v00                    = v1;
          dudx                   = 1;
          dvdy                   = 1;
          CASE(0 << 2) + 1 : u00 = u1;
          v00                    = v1 + sv;
          dudy                   = 1;
          dvdx                   = -1;
          CASE(0 << 2) + 2 : u00 = u1 + su;
          v00                    = v1 + sv;
          dudx                   = -1;
          dvdy                   = -1;
          CASE(0 << 2) + 3 : u00 = u1 + su;
          v00                    = v1;
          dudy                   = -1;
          dvdx                   = 1;
          CASE(1 << 2) + 0 : u00 = u1 + su;
          v00                    = v1;
          dudx                   = -1;
          dvdy                   = 1;
          CASE(1 << 2) + 1 : u00 = u1 + su;
          v00                    = v1 + sv;
          dudy                   = -1;
          dvdx                   = -1;
          CASE(1 << 2) + 2 : u00 = u1;
          v00                    = v1 + sv;
          dudx                   = 1;
          dvdy                   = -1;
          CASE(1 << 2) + 3 : u00 = u1;
          v00                    = v1;
          dudy                   = 1;
          dvdx                   = 1;
          D 78 DEFAULT : assert(FALSE);
          u00 = v00 = dudy = dvdx = 0;
          E 78
I 78 DEFAULT : abort();
          u00 = v00 = dudy = dvdx = 0;
          E 78
        }
        E 55

D 55
#ifdef DEBUG
            STW_STOP(0) STW_PRINT(stdout)
#endif
E 55
I 55 if (dudx) for (u0 = u00, v0 = v00, y = newy; y < newy + ly;
                      v0 += dvdy, y++) {
          u      = u0;
          v      = v0;
          pixout = bufout + newx + y * wrapout;
          for (x = newx; x < newx + lx; u += dudx, x++) {
            if (GET_BIT(u, v, bufin, bytewrapin, bitoffsin))
              *pixout++ = value_for_1;
            else
              *pixout++ = value_for_0;
          }
        }
        else for (u0 = u00, v0 = v00, y = newy; y < newy + ly;
                  u0 += dudy, y++) {
          u      = u0;
          v      = v0;
          pixout = bufout + newx + y * wrapout;
          for (x = newx; x < newx + lx; v += dvdx, x++) {
            if (GET_BIT(u, v, bufin, bytewrapin, bitoffsin))
              *pixout++ = value_for_1;
            else
              *pixout++ = value_for_0;
          }
        }
        E 55
      }

      /*---------------------------------------------------------------------------*/

      static void rop_copy_90_bw_rgbm(RASTER * rin, RASTER * rout, int x1,
                                      int y1, int x2, int y2, int newx,
                                      int newy,
D 52 int mirror, int ninety) {
        UCHAR *bufin, *bytein;
        LPIXEL *bufout, *pixout;
        int bytewrapin, bitoffsin, wrapout;
        LPIXEL value_for_0, value_for_1;
        int u, v, lu, lv, su, sv, u00, v00, u0, v0, dudy, dvdy, dudx, dvdx;
        int x, y, lx, ly;
        int u1, v1, u2, v2;

#ifdef DEBUG
        STW_INIT(0, "rop_copy_90_bw_gr8")
        STW_START(0)
#endif

        mirror &= 1;
        ninety &= 3;

        if (!ninety && !mirror) {
          rop_copy(rin, rout, x1, y1, x2, y2, newx, newy);
          return;
        }

        switch (rin->type) {
          CASE RAS_WB : value_for_0.r = 255;
          value_for_0.g               = 255;
          value_for_0.b               = 255;
          value_for_0.m               = 255;
          value_for_1.r               = 0;
          value_for_1.g               = 0;
          value_for_1.b               = 0;
          value_for_1.m               = 255;
          CASE RAS_BW : value_for_0.r = 0;
          value_for_0.g               = 0;
          value_for_0.b               = 0;
          value_for_0.m               = 255;
          value_for_1.r               = 255;
          value_for_1.g               = 255;
          value_for_1.b               = 255;
          value_for_1.m               = 255;
        DEFAULT:
          printf("bad raster type in rop_copy_90_bw_rgbm\n");
          return;
          E 22
        }

        u1 = x1;
        v1 = y1;
        u2 = x2;
        v2 = y2;

        su = u2 - u1;
        sv = v2 - v1;
        lu = u2 - u1 + 1;
        lv = v2 - v1 + 1;

        if (ninety & 1) {
          lx = lv;
          ly = lu;
        } else {
          lx = lu;
          ly = lv;
        }

        bufin            = rin->buffer;
        D 47 bytewrapin = (rin->wrap + 7) / 8;
        E 47
I 47 bytewrapin        = (rin->wrap + 7) >> 3;
        E 47 bitoffsin  = rin->bit_offs;
        bufout           = rout->buffer;
        wrapout          = rout->wrap;

        dudx = 0;
        dudy = 0;
        dvdx = 0;
        dvdy = 0;

        switch ((mirror << 2) + ninety) {
          CASE(0 << 2) + 0 : u00 = u1;
          v00                    = v1;
          dudx                   = 1;
          dvdy                   = 1;
          CASE(0 << 2) + 1 : u00 = u1;
          v00                    = v1 + sv;
          dudy                   = 1;
          dvdx                   = -1;
          CASE(0 << 2) + 2 : u00 = u1 + su;
          v00                    = v1 + sv;
          dudx                   = -1;
          dvdy                   = -1;
          CASE(0 << 2) + 3 : u00 = u1 + su;
          v00                    = v1;
          dudy                   = -1;
          dvdx                   = 1;
          CASE(1 << 2) + 0 : u00 = u1 + su;
          v00                    = v1;
          dudx                   = -1;
          dvdy                   = 1;
          CASE(1 << 2) + 1 : u00 = u1 + su;
          v00                    = v1 + sv;
          dudy                   = -1;
          dvdx                   = -1;
          CASE(1 << 2) + 2 : u00 = u1;
          v00                    = v1 + sv;
          dudx                   = 1;
          dvdy                   = -1;
          CASE(1 << 2) + 3 : u00 = u1;
          v00                    = v1;
          dudy                   = 1;
          dvdx                   = 1;
          I 50 DEFAULT : assert(FALSE);
          u00 = v00 = dudy = dvdx = 0;
          E 50
        }

        if (dudx)
          for (u0 = u00, v0 = v00, y = newy; y < newy + ly; v0 += dvdy, y++) {
            u      = u0;
            v      = v0;
            pixout = bufout + newx + y * wrapout;
            for (x = newx; x < newx + lx; u += dudx, x++) {
              if (GET_BIT(u, v, bufin, bytewrapin, bitoffsin))
                *pixout++ = value_for_1;
              else
                *pixout++ = value_for_0;
            }
          }
        else
          for (u0 = u00, v0 = v00, y = newy; y < newy + ly; u0 += dudy, y++) {
            u      = u0;
            v      = v0;
            pixout = bufout + newx + y * wrapout;
            for (x = newx; x < newx + lx; v += dvdx, x++) {
              if (GET_BIT(u, v, bufin, bytewrapin, bitoffsin))
                *pixout++ = value_for_1;
              else
                *pixout++ = value_for_0;
              E 21
            }
          }
        E 17

#ifdef DEBUG
            STW_STOP(0) STW_PRINT(stdout)
#endif
I 26
      }

      /*---------------------------------------------------------------------------*/

      static void rop_copy_90_rgbm(RASTER * rin, RASTER * rout, int x1, int y1,
                                   int x2, int y2, int newx, int newy,
                                   int mirror, int ninety) {
        LPIXEL *bufin, *pixin;
        LPIXEL *bufout, *pixout;
        int wrapin, wrapout;
        int u, v, lu, lv, su, sv, u00, v00, u0, v0, dindx, dudy, dvdy;
        int x, y, lx, ly;
        int u1, v1, u2, v2;

        mirror &= 1;
        ninety &= 3;

        if (!ninety && !mirror) {
          rop_copy(rin, rout, x1, y1, x2, y2, newx, newy);
          return;
        }

        u1 = x1;
        v1 = y1;
        u2 = x2;
        v2 = y2;

        su = u2 - u1;
        sv = v2 - v1;
        lu = u2 - u1 + 1;
        lv = v2 - v1 + 1;

        if (ninety & 1) {
          lx = lv;
          ly = lu;
        } else {
          lx = lu;
          ly = lv;
        }

        bufin   = rin->buffer;
        wrapin  = rin->wrap;
        bufout  = rout->buffer;
        wrapout = rout->wrap;

        dudy = 0;
        dvdy = 0;

        switch ((mirror << 2) + ninety) {
          CASE(0 << 2) + 0 : u00 = u1;
          v00                    = v1;
          dvdy                   = 1;
          dindx                  = 1;
          CASE(0 << 2) + 1 : u00 = u1;
          v00                    = v1 + sv;
          dudy                   = 1;
          dindx                  = -wrapin;
          CASE(0 << 2) + 2 : u00 = u1 + su;
          v00                    = v1 + sv;
          dvdy                   = -1;
          dindx                  = -1;
          CASE(0 << 2) + 3 : u00 = u1 + su;
          v00                    = v1;
          dudy                   = -1;
          dindx                  = wrapin;
          CASE(1 << 2) + 0 : u00 = u1 + su;
          v00                    = v1;
          dvdy                   = 1;
          dindx                  = -1;
          CASE(1 << 2) + 1 : u00 = u1 + su;
          v00                    = v1 + sv;
          dudy                   = -1;
          dindx                  = -wrapin;
          CASE(1 << 2) + 2 : u00 = u1;
          v00                    = v1 + sv;
          dvdy                   = -1;
          dindx                  = 1;
          CASE(1 << 2) + 3 : u00 = u1;
          v00                    = v1;
          dudy                   = 1;
          dindx                  = wrapin;
          I 50 DEFAULT : assert(FALSE);
          u00 = v00 = dudy = dindx = 0;
          E 50
        }

        for (u0 = u00, v0 = v00, y = newy; y < newy + ly;
             u0 += dudy, v0 += dvdy, y++) {
          pixin  = bufin + u0 + v0 * wrapin;
          pixout = bufout + newx + y * wrapout;
          for (x = newx; x < newx + lx; x++) {
            *pixout++ = *pixin;
            pixin += dindx;
          }
        }
        E 26
E 14
      }

      /*---------------------------------------------------------------------------*/

      E 6 void rop_clear(RASTER * r, int x1, int y1, int x2, int y2) {
        D 13 char *pix;
        E 13
I 13 UCHAR *pix;
        E 13 int l, x, y, tmp, rowsize, wrap, pixbits;

        if (x1 > x2) {
          tmp = x1;
          x1  = x2;
          x2  = tmp;
        }
        if (y1 > y2) {
          tmp = y1;
          y1  = y2;
          y2  = tmp;
        }

        /* controllo gli sconfinamenti */
        if (x1 < 0 || y1 < 0 || x2 - x1 >= r->lx || y2 - y1 >= r->ly) {
          printf("### INTERNAL ERROR - rop_clear; access violation\n");
          return;
        }

        pixbits = rop_pixbits(r->type);

        /* per adesso niente pixel non multipli di 8 bits */
        D 2 if (pixbits & 7)
E 2
I 2 if (rop_fillerbits(r->type))
E 2 {
          printf("### INTERNAL ERROR - rop_clear; byte fraction pixels\n");
          return;
        }

        wrap      = (r->wrap * pixbits) >> 3;
        rowsize   = ((x2 - x1 + 1) * pixbits) >> 3;
        D 10 pix = (char *)r->buffer + (((x1 + y1 * r->wrap) * pixbits) >> 3);
        E 10
I 10 pix        = (UCHAR *)r->buffer + (((x1 + y1 * r->wrap) * pixbits) >> 3);
        E 10 l   = y2 - y1 + 1;
        while (l-- > 0) {
          memset(pix, 0, rowsize);
          pix += wrap;
        }
      }

      D 6

E 6
          /*---------------------------------------------------------------------------*/
I 6

I 25 void
      rop_add_white_to_cmap(RASTER * ras) {
        int i;
        UCHAR m, white;

        for (i = 0; i < ras->cmap.size; i++) {
          m = ras->cmap.buffer[i].m;
          if (ras->cmap.buffer[i].r > m || ras->cmap.buffer[i].g > m ||
              ras->cmap.buffer[i].b > m)
            break;
          white = (UCHAR)255 - m;
          ras->cmap.buffer[i].r += white;
          ras->cmap.buffer[i].g += white;
          ras->cmap.buffer[i].b += white;
        }
        if (i < ras->cmap.size)
          fprintf(stderr, "\7add_white: cmap is not premultiplied\n");
      }

      /*---------------------------------------------------------------------------*/

      void rop_remove_white_from_cmap(RASTER * ras) {
        int i;
        UCHAR m, white;

        for (i = 0; i < ras->cmap.size; i++) {
          m     = ras->cmap.buffer[i].m;
          white = (UCHAR)255 - m;
          ras->cmap.buffer[i].r -= white;
          ras->cmap.buffer[i].g -= white;
          ras->cmap.buffer[i].b -= white;
          if (ras->cmap.buffer[i].r > m || ras->cmap.buffer[i].g > m ||
              ras->cmap.buffer[i].b > m)
            break;
        }
        if (i < ras->cmap.size)
          fprintf(stderr, "\7remove_white: cmap is not premultiplied\n");
        I 32
      }

      /*---------------------------------------------------------------------------*/

      static LPIXEL premult_lpixel(LPIXEL lpixel) {
        int m;
        LPIXEL new_lpixel;

        m = lpixel.m;
        if (m == 255)
          return lpixel;
        else if (m == 0) {
          new_lpixel.r = 0;
          new_lpixel.g = 0;
          new_lpixel.b = 0;
          new_lpixel.m = 0;
        } else {
          new_lpixel.r = (lpixel.r * m + 127) / 255;
          new_lpixel.g = (lpixel.g * m + 127) / 255;
          new_lpixel.b = (lpixel.b * m + 127) / 255;
          new_lpixel.m = m;
        }
        return new_lpixel;
      }

      /*---------------------------------------------------------------------------*/

      static LPIXEL unpremult_lpixel(LPIXEL lpixel) {
        int m, m_2;
        LPIXEL new_lpixel;

        m = lpixel.m;
        if (m == 255)
          return lpixel;
        else if (m == 0) {
          new_lpixel.r = 255;
          new_lpixel.g = 255;
          new_lpixel.b = 255;
          new_lpixel.m = 0;
          D 51
        }
        E 51
I 51
      }
      E 51 else {
        m_2          = m >> 1;
        new_lpixel.r = (lpixel.r * 255 + m_2) / m;
        new_lpixel.g = (lpixel.g * 255 + m_2) / m;
        new_lpixel.b = (lpixel.b * 255 + m_2) / m;
E 52
I 52
		                int mirror, int ninety)
{
  UCHAR *bufin, *bytein;
  LPIXEL *bufout, *pixout;
  int bytewrapin, bitoffsin, wrapout;
  LPIXEL value_for_0, value_for_1;
  int u, v, lu, lv, su, sv, u00, v00, u0, v0, dudy, dvdy, dudx, dvdx;
  int x, y, lx, ly;
  int u1, v1, u2, v2;

  D 55
#ifdef DEBUG
      STW_INIT(0, "rop_copy_90_bw_gr8") STW_START(0)
#endif

E 55 mirror &= 1;
  ninety &= 3;

  if (!ninety && !mirror) {
    rop_copy(rin, rout, x1, y1, x2, y2, newx, newy);
    return;
  }

  switch (rin->type) {
    CASE RAS_WB : value_for_0.r = 255;
    value_for_0.g               = 255;
    value_for_0.b               = 255;
    value_for_0.m               = 255;
    value_for_1.r               = 0;
    value_for_1.g               = 0;
    value_for_1.b               = 0;
    value_for_1.m               = 255;
    CASE RAS_BW : value_for_0.r = 0;
    value_for_0.g               = 0;
    value_for_0.b               = 0;
    value_for_0.m               = 255;
    value_for_1.r               = 255;
    value_for_1.g               = 255;
    value_for_1.b               = 255;
    value_for_1.m               = 255;
  DEFAULT:
    printf("bad raster type in rop_copy_90_bw_rgbm\n");
    return;
  }

  u1 = x1;
  v1 = y1;
  u2 = x2;
  v2 = y2;

  su = u2 - u1;
  sv = v2 - v1;
  lu = u2 - u1 + 1;
  lv = v2 - v1 + 1;

  if (ninety & 1) {
    lx = lv;
    ly = lu;
  } else {
    lx = lu;
    ly = lv;
  }
  D 53

      bufin  = rin->buffer;
  bytewrapin = (rin->wrap + 7) >> 3;
  bitoffsin  = rin->bit_offs;
  bufout     = rout->buffer;
  wrapout    = rout->wrap;

  dudx = 0;
  dudy = 0;
  dvdx = 0;
  dvdy = 0;

  switch ((mirror << 2) + ninety) {
    CASE(0 << 2) + 0 : u00 = u1;
    v00                    = v1;
    dudx                   = 1;
    dvdy                   = 1;
    CASE(0 << 2) + 1 : u00 = u1;
    v00                    = v1 + sv;
    dudy                   = 1;
    dvdx                   = -1;
    CASE(0 << 2) + 2 : u00 = u1 + su;
    v00                    = v1 + sv;
    dudx                   = -1;
    dvdy                   = -1;
    CASE(0 << 2) + 3 : u00 = u1 + su;
    v00                    = v1;
    dudy                   = -1;
    dvdx                   = 1;
    CASE(1 << 2) + 0 : u00 = u1 + su;
    v00                    = v1;
    dudx                   = -1;
    dvdy                   = 1;
    CASE(1 << 2) + 1 : u00 = u1 + su;
    v00                    = v1 + sv;
    dudy                   = -1;
    dvdx                   = -1;
    CASE(1 << 2) + 2 : u00 = u1;
    v00                    = v1 + sv;
    dudx                   = 1;
    dvdy                   = -1;
    CASE(1 << 2) + 3 : u00 = u1;
    v00                    = v1;
    dudy                   = 1;
    dvdx                   = 1;
  DEFAULT:
    assert(FALSE);
    u00 = v00 = dudy = dvdx = 0;
  }

  if (dudx)
    for (u0 = u00, v0 = v00, y = newy; y < newy + ly; v0 += dvdy, y++) {
      u      = u0;
      v      = v0;
      pixout = bufout + newx + y * wrapout;
      for (x = newx; x < newx + lx; u += dudx, x++) {
        if (GET_BIT(u, v, bufin, bytewrapin, bitoffsin))
          *pixout++ = value_for_1;
        else
          *pixout++ = value_for_0;
      }
    }
  else
    for (u0 = u00, v0 = v00, y = newy; y < newy + ly; u0 += dudy, y++) {
      u      = u0;
      v      = v0;
      pixout = bufout + newx + y * wrapout;
      for (x = newx; x < newx + lx; v += dvdx, x++) {
        E 53
I 53

            bufin  = rin->buffer;
        bytewrapin = (rin->wrap + 7) >> 3;
        bitoffsin  = rin->bit_offs;
        bufout     = rout->buffer;
        wrapout    = rout->wrap;

        dudx = 0;
        dudy = 0;
        dvdx = 0;
        dvdy = 0;

        switch ((mirror << 2) + ninety) {
          CASE(0 << 2) + 0 : u00 = u1;
          v00                    = v1;
          dudx                   = 1;
          dvdy                   = 1;
          CASE(0 << 2) + 1 : u00 = u1;
          v00                    = v1 + sv;
          dudy                   = 1;
          dvdx                   = -1;
          CASE(0 << 2) + 2 : u00 = u1 + su;
          v00                    = v1 + sv;
          dudx                   = -1;
          dvdy                   = -1;
          CASE(0 << 2) + 3 : u00 = u1 + su;
          v00                    = v1;
          dudy                   = -1;
          dvdx                   = 1;
          CASE(1 << 2) + 0 : u00 = u1 + su;
          v00                    = v1;
          dudx                   = -1;
          dvdy                   = 1;
          CASE(1 << 2) + 1 : u00 = u1 + su;
          v00                    = v1 + sv;
          dudy                   = -1;
          dvdx                   = -1;
          CASE(1 << 2) + 2 : u00 = u1;
          v00                    = v1 + sv;
          dudx                   = 1;
          dvdy                   = -1;
          CASE(1 << 2) + 3 : u00 = u1;
          v00                    = v1;
          dudy                   = 1;
          dvdx                   = 1;
          D 78 DEFAULT : assert(FALSE);
          u00 = v00 = dudy = dvdx = 0;
          E 78
I 78 DEFAULT : abort();
          u00 = v00 = dudy = dvdx = 0;
          E 78
        }

        if (dudx)
          for (u0 = u00, v0 = v00, y = newy; y < newy + ly; v0 += dvdy, y++) {
            u      = u0;
            v      = v0;
            pixout = bufout + newx + y * wrapout;
            for (x = newx; x < newx + lx; u += dudx, x++) {
              if (GET_BIT(u, v, bufin, bytewrapin, bitoffsin))
                *pixout++ = value_for_1;
              else
                *pixout++ = value_for_0;
            }
          }
        else
          for (u0 = u00, v0 = v00, y = newy; y < newy + ly; u0 += dudy, y++) {
            u      = u0;
            v      = v0;
            pixout = bufout + newx + y * wrapout;
            for (x = newx; x < newx + lx; v += dvdx, x++) {
              E 53 if (GET_BIT(u, v, bufin, bytewrapin, bitoffsin)) *pixout++ =
                  value_for_1;
              else *pixout++ = value_for_0;
            }
          }
        D 55

#ifdef DEBUG
            STW_STOP(0) STW_PRINT(stdout)
#endif
E 55
      }

      /*---------------------------------------------------------------------------*/

      I 62 static void rop_zoom_out_90_bw_rgbm(
          RASTER * rin, RASTER * rout, int x1, int y1, int x2, int y2, int newx,
          int newy, int abs_zoom_level, int mirror, int ninety) {
        UCHAR *bufin;
        int val_0, val_1, tmp;
        LPIXEL *bufout, *rowout, *pixout, valout;
        int bitrowin, bytewrapin, bitwrapin, wrapout, bitin, bit, bit_offs,
            startbit;
        int u1, v1, u2, v2;
        int u, v, lu, lv, startu, startv;
        int p, q, lp, lq, s, t; /* s=p+q, t=p-q */
        int dudp, dvdp, dudq, dvdq;
        int dinbitsdp, dinbitsdq, dinbitsds, dinbitsdt;
        int plast, qlast, prest, qrest, i, j;
        int factor, fac_fac_2_bits;
        int fac_fac, qrest_fac, fac_prest, qrest_prest;
        int fac_fac_2, qrest_fac_2, fac_prest_2, qrest_prest_2;
        int fac_fac_4;

        mirror &= 1;
        ninety &= 3;

        if (!ninety && !mirror) {
          rop_zoom_out(rin, rout, x1, y1, x2, y2, newx, newy, abs_zoom_level);
          return;
        }

        if (rin->type == RAS_WB) {
          val_0 = 0xff;
          val_1 = 0x00;
        } else {
          val_0 = 0x00;
          val_1 = 0xff;
        }

        u1 = x1;
        v1 = y1;
        u2 = x2;
        v2 = y2;

        lu = u2 - u1 + 1;
        lv = v2 - v1 + 1;

        if (ninety & 1) {
          lp = lv;
          lq = lu;
        } else {
          lp = lu;
          lq = lv;
        }
        factor         = 1 << abs_zoom_level;
        prest          = lp & (factor - 1);
        qrest          = lq & (factor - 1);
        plast          = lp - prest;
        qlast          = lq - qrest;
        fac_fac        = factor * factor;
        fac_fac_2      = fac_fac >> 1;
        fac_fac_4      = fac_fac >> 2;
        fac_fac_2_bits = 2 * abs_zoom_level - 1;
        qrest_fac      = qrest * factor;
        qrest_fac_2    = qrest_fac >> 1;
        fac_prest      = factor * prest;
        fac_prest_2    = fac_prest >> 1;
        qrest_prest    = qrest * prest;
        qrest_prest_2  = qrest_prest >> 1;

        bufin      = rin->buffer;
        bytewrapin = (rin->wrap + 7) >> 3;
        bitwrapin  = bytewrapin << 3;
        bit_offs   = rin->bit_offs;
        bufout     = rout->buffer;
        wrapout    = rout->wrap;

        switch ((mirror << 2) + ninety) {
          CASE(0 << 2) + 0 : dudp = 1;
          dvdp                    = 0;
          dudq                    = 0;
          dvdq                    = 1;
          startu                  = u1;
          startv                  = v1;
          CASE(0 << 2) + 1 : dudp = 0;
          dvdp                    = -1;
          dudq                    = 1;
          dvdq                    = 0;
          startu                  = u1;
          startv                  = v2;
          CASE(0 << 2) + 2 : dudp = -1;
          dvdp                    = 0;
          dudq                    = 0;
          dvdq                    = -1;
          startu                  = u2;
          startv                  = v2;
          CASE(0 << 2) + 3 : dudp = 0;
          dvdp                    = 1;
          dudq                    = -1;
          dvdq                    = 0;
          startu                  = u2;
          startv                  = v1;
          CASE(1 << 2) + 0 : dudp = -1;
          dvdp                    = 0;
          dudq                    = 0;
          dvdq                    = 1;
          startu                  = u2;
          startv                  = v1;
          CASE(1 << 2) + 1 : dudp = 0;
          dvdp                    = -1;
          dudq                    = -1;
          dvdq                    = 0;
          startu                  = u2;
          startv                  = v2;
          CASE(1 << 2) + 2 : dudp = 1;
          dvdp                    = 0;
          dudq                    = 0;
          dvdq                    = -1;
          startu                  = u1;
          startv                  = v2;
          CASE(1 << 2) + 3 : dudp = 0;
          dvdp                    = 1;
          dudq                    = 1;
          dvdq                    = 0;
          startu                  = u1;
          startv                  = v1;
        DEFAULT:
          D 78 assert(FALSE);
          dudp = dvdp = dudq = dvdq = startu = startv = 0;
          E 78
I 78 abort();
          dudp = dvdp = dudq = dvdq = startu = startv = 0;
          E 78
        }

        dinbitsdp = dudp + bitwrapin * dvdp;
        dinbitsdq = dudq + bitwrapin * dvdq;
        dinbitsds = dinbitsdp + dinbitsdq;
        dinbitsdt = dinbitsdp - dinbitsdq;

        bitrowin = bit_offs + startu + startv * bitwrapin;
        rowout   = (LPIXEL *)rout->buffer + newx + newy * wrapout;
        valout.m = 0xff;
        for (q = 0; q < qlast; q += factor) {
          bitin  = bitrowin;
          pixout = rowout;
          for (p = 0; p < plast; p += factor) {
            tmp = 0;
            bit = bitin;
            for (j = 0; j < factor; j += 2) {
              for (i = 0; i < factor; i += 2) {
                tmp += GET_BWBIT(bit, bufin);
                bit += dinbitsds;
                tmp += GET_BWBIT(bit, bufin);
                bit += dinbitsdt;
              }
              bit += 2 * dinbitsdq - factor * dinbitsdp;
            }
            tmp      = tmp * val_1 + (fac_fac_2 - tmp) * val_0;
            tmp      = (tmp + fac_fac_4) >> fac_fac_2_bits;
            valout.r = valout.g = valout.b = tmp;
            *pixout++                      = valout;
            bitin += factor * dinbitsdp;
          }
          if (prest) {
            tmp = 0;
            for (j = 0; j < factor; j++)
              for (i = 0; i < prest; i++) {
                tmp += GET_BWBIT(bitin + i * dinbitsdp + j * dinbitsdq, bufin);
              }
            tmp      = tmp * val_1 + (fac_prest - tmp) * val_0;
            tmp      = (tmp + fac_prest_2) / fac_prest;
            valout.r = valout.g = valout.b = tmp;
            *pixout++                      = valout;
          }
          bitrowin += factor * dinbitsdq;
          rowout += wrapout;
        }
        if (qrest) {
          bitin  = bitrowin;
          pixout = rowout;
          for (p = 0; p < plast; p += factor) {
            tmp = 0;
            for (j = 0; j < qrest; j++)
              for (i = 0; i < factor; i++) {
                tmp += GET_BWBIT(bitin + i * dinbitsdp + j * dinbitsdq, bufin);
              }
            tmp      = tmp * val_1 + (qrest_fac - tmp) * val_0;
            tmp      = (tmp + qrest_fac_2) / qrest_fac;
            valout.r = valout.g = valout.b = tmp;
            *pixout++                      = valout;
            bitin += factor * dinbitsdp;
          }
          if (prest) {
            tmp = 0;
            for (j = 0; j < qrest; j++)
              for (i = 0; i < prest; i++) {
                tmp += GET_BWBIT(bitin + i * dinbitsdp + j * dinbitsdq, bufin);
              }
            tmp      = tmp * val_1 + (qrest_prest - tmp) * val_0;
            tmp      = (tmp + qrest_prest_2) / qrest_prest;
            valout.r = valout.g = valout.b = tmp;
            *pixout++                      = valout;
          }
        }
      }

      /*---------------------------------------------------------------------------*/

      static void rop_zoom_out_90_bw_rgb16(
          RASTER * rin, RASTER * rout, int x1, int y1, int x2, int y2, int newx,
          int newy, int abs_zoom_level, int mirror, int ninety) {
        UCHAR *bufin;
        int val_0, val_1, tmp;
        USHORT *bufout, *rowout, *pixout;
        int bitrowin, bytewrapin, bitwrapin, wrapout, bitin, bit, bit_offs,
            startbit;
        int u1, v1, u2, v2;
        int u, v, lu, lv, startu, startv;
        int p, q, lp, lq, s, t; /* s=p+q, t=p-q */
        int dudp, dvdp, dudq, dvdq;
        int dinbitsdp, dinbitsdq, dinbitsds, dinbitsdt;
        int plast, qlast, prest, qrest, i, j;
        int factor, fac_fac_2_bits;
        int fac_fac, qrest_fac, fac_prest, qrest_prest;
        int fac_fac_2, qrest_fac_2, fac_prest_2, qrest_prest_2;
        int fac_fac_4;

        mirror &= 1;
        ninety &= 3;

        if (!ninety && !mirror) {
          rop_zoom_out(rin, rout, x1, y1, x2, y2, newx, newy, abs_zoom_level);
          return;
        }

        if (rin->type == RAS_WB) {
          val_0 = 0xff;
          val_1 = 0x00;
        } else {
          val_0 = 0x00;
          val_1 = 0xff;
        }

        u1 = x1;
        v1 = y1;
        u2 = x2;
        v2 = y2;

        lu = u2 - u1 + 1;
        lv = v2 - v1 + 1;

        if (ninety & 1) {
          lp = lv;
          lq = lu;
        } else {
          lp = lu;
          lq = lv;
        }
        factor         = 1 << abs_zoom_level;
        prest          = lp & (factor - 1);
        qrest          = lq & (factor - 1);
        plast          = lp - prest;
        qlast          = lq - qrest;
        fac_fac        = factor * factor;
        fac_fac_2      = fac_fac >> 1;
        fac_fac_4      = fac_fac >> 2;
        fac_fac_2_bits = 2 * abs_zoom_level - 1;
        qrest_fac      = qrest * factor;
        qrest_fac_2    = qrest_fac >> 1;
        fac_prest      = factor * prest;
        fac_prest_2    = fac_prest >> 1;
        qrest_prest    = qrest * prest;
        qrest_prest_2  = qrest_prest >> 1;

        bufin      = rin->buffer;
        bytewrapin = (rin->wrap + 7) >> 3;
        bitwrapin  = bytewrapin << 3;
        bit_offs   = rin->bit_offs;
        bufout     = rout->buffer;
        wrapout    = rout->wrap;

        switch ((mirror << 2) + ninety) {
          CASE(0 << 2) + 0 : dudp = 1;
          dvdp                    = 0;
          dudq                    = 0;
          dvdq                    = 1;
          startu                  = u1;
          startv                  = v1;
          CASE(0 << 2) + 1 : dudp = 0;
          dvdp                    = -1;
          dudq                    = 1;
          dvdq                    = 0;
          startu                  = u1;
          startv                  = v2;
          CASE(0 << 2) + 2 : dudp = -1;
          dvdp                    = 0;
          dudq                    = 0;
          dvdq                    = -1;
          startu                  = u2;
          startv                  = v2;
          CASE(0 << 2) + 3 : dudp = 0;
          dvdp                    = 1;
          dudq                    = -1;
          dvdq                    = 0;
          startu                  = u2;
          startv                  = v1;
          CASE(1 << 2) + 0 : dudp = -1;
          dvdp                    = 0;
          dudq                    = 0;
          dvdq                    = 1;
          startu                  = u2;
          startv                  = v1;
          CASE(1 << 2) + 1 : dudp = 0;
          dvdp                    = -1;
          dudq                    = -1;
          dvdq                    = 0;
          startu                  = u2;
          startv                  = v2;
          CASE(1 << 2) + 2 : dudp = 1;
          dvdp                    = 0;
          dudq                    = 0;
          dvdq                    = -1;
          startu                  = u1;
          startv                  = v2;
          CASE(1 << 2) + 3 : dudp = 0;
          dvdp                    = 1;
          dudq                    = 1;
          dvdq                    = 0;
          startu                  = u1;
          startv                  = v1;
        DEFAULT:
          D 78 assert(FALSE);
          dudp = dvdp = dudq = dvdq = startu = startv = 0;
          E 78
I 78 abort();
          dudp = dvdp = dudq = dvdq = startu = startv = 0;
          E 78
        }

        dinbitsdp = dudp + bitwrapin * dvdp;
        dinbitsdq = dudq + bitwrapin * dvdq;
        dinbitsds = dinbitsdp + dinbitsdq;
        dinbitsdt = dinbitsdp - dinbitsdq;

        bitrowin = bit_offs + startu + startv * bitwrapin;
        rowout   = (USHORT *)rout->buffer + newx + newy * wrapout;
        for (q = 0; q < qlast; q += factor) {
          bitin  = bitrowin;
          pixout = rowout;
          for (p = 0; p < plast; p += factor) {
            tmp = 0;
            bit = bitin;
            for (j = 0; j < factor; j += 2) {
              for (i = 0; i < factor; i += 2) {
                tmp += GET_BWBIT(bit, bufin);
                bit += dinbitsds;
                tmp += GET_BWBIT(bit, bufin);
                bit += dinbitsdt;
              }
              bit += 2 * dinbitsdq - factor * dinbitsdp;
            }
            tmp       = tmp * val_1 + (fac_fac_2 - tmp) * val_0;
            tmp       = (tmp + fac_fac_4) >> fac_fac_2_bits;
            *pixout++ = PIX_RGB16_FROM_BYTES(tmp, tmp, tmp);
            bitin += factor * dinbitsdp;
          }
          if (prest) {
            tmp = 0;
            for (j = 0; j < factor; j++)
              for (i = 0; i < prest; i++) {
                tmp += GET_BWBIT(bitin + i * dinbitsdp + j * dinbitsdq, bufin);
              }
            tmp       = tmp * val_1 + (fac_prest - tmp) * val_0;
            tmp       = (tmp + fac_prest_2) / fac_prest;
            *pixout++ = PIX_RGB16_FROM_BYTES(tmp, tmp, tmp);
          }
          bitrowin += factor * dinbitsdq;
          rowout += wrapout;
        }
        if (qrest) {
          bitin  = bitrowin;
          pixout = rowout;
          for (p = 0; p < plast; p += factor) {
            tmp = 0;
            for (j = 0; j < qrest; j++)
              for (i = 0; i < factor; i++) {
                tmp += GET_BWBIT(bitin + i * dinbitsdp + j * dinbitsdq, bufin);
              }
            tmp       = tmp * val_1 + (qrest_fac - tmp) * val_0;
            tmp       = (tmp + qrest_fac_2) / qrest_fac;
            *pixout++ = PIX_RGB16_FROM_BYTES(tmp, tmp, tmp);
            bitin += factor * dinbitsdp;
          }
          if (prest) {
            tmp = 0;
            for (j = 0; j < qrest; j++)
              for (i = 0; i < prest; i++) {
                tmp += GET_BWBIT(bitin + i * dinbitsdp + j * dinbitsdq, bufin);
              }
            tmp       = tmp * val_1 + (qrest_prest - tmp) * val_0;
            tmp       = (tmp + qrest_prest_2) / qrest_prest;
            *pixout++ = PIX_RGB16_FROM_BYTES(tmp, tmp, tmp);
          }
        }
      }

      /*---------------------------------------------------------------------------*/

      E 62 static void rop_copy_90_rgbm(RASTER * rin, RASTER * rout, int x1,
                                         int y1, int x2, int y2, int newx,
                                         int newy, int mirror, int ninety) {
        LPIXEL *bufin, *pixin;
        LPIXEL *bufout, *pixout;
        int wrapin, wrapout;
        int u, v, lu, lv, su, sv, u00, v00, u0, v0, dindx, dudy, dvdy;
        int x, y, lx, ly;
        int u1, v1, u2, v2;

        mirror &= 1;
        ninety &= 3;

        if (!ninety && !mirror) {
          rop_copy(rin, rout, x1, y1, x2, y2, newx, newy);
          return;
        }

        u1 = x1;
        v1 = y1;
        u2 = x2;
        v2 = y2;

        su = u2 - u1;
        sv = v2 - v1;
        lu = u2 - u1 + 1;
        lv = v2 - v1 + 1;

        if (ninety & 1) {
          lx = lv;
          ly = lu;
        } else {
          lx = lu;
          ly = lv;
        }

        bufin   = rin->buffer;
        wrapin  = rin->wrap;
        bufout  = rout->buffer;
        wrapout = rout->wrap;

        dudy = 0;
        dvdy = 0;

        switch ((mirror << 2) + ninety) {
          CASE(0 << 2) + 0 : u00 = u1;
          v00                    = v1;
          dvdy                   = 1;
          dindx                  = 1;
          CASE(0 << 2) + 1 : u00 = u1;
          v00                    = v1 + sv;
          dudy                   = 1;
          dindx                  = -wrapin;
          CASE(0 << 2) + 2 : u00 = u1 + su;
          v00                    = v1 + sv;
          dvdy                   = -1;
          dindx                  = -1;
          CASE(0 << 2) + 3 : u00 = u1 + su;
          v00                    = v1;
          dudy                   = -1;
          dindx                  = wrapin;
          CASE(1 << 2) + 0 : u00 = u1 + su;
          v00                    = v1;
          dvdy                   = 1;
          dindx                  = -1;
          CASE(1 << 2) + 1 : u00 = u1 + su;
          v00                    = v1 + sv;
          dudy                   = -1;
          dindx                  = -wrapin;
          CASE(1 << 2) + 2 : u00 = u1;
          v00                    = v1 + sv;
          dvdy                   = -1;
          dindx                  = 1;
          CASE(1 << 2) + 3 : u00 = u1;
          v00                    = v1;
          dudy                   = 1;
          dindx                  = wrapin;
          D 78 DEFAULT : assert(FALSE);
          u00 = v00 = dudy = dindx = 0;
          E 78
I 78 DEFAULT : abort();
          u00 = v00 = dudy = dindx = 0;
          E 78
        }

        for (u0 = u00, v0 = v00, y = newy; y < newy + ly;
             u0 += dudy, v0 += dvdy, y++) {
          pixin  = bufin + u0 + v0 * wrapin;
          pixout = bufout + newx + y * wrapout;
          for (x = newx; x < newx + lx; x++) {
            *pixout++ = *pixin;
            pixin += dindx;
          }
          I 57
        }
      }

      /*---------------------------------------------------------------------------*/

      /* vedi sia rop_zoom_out che rop_copy_90
*/
      void rop_zoom_out_90(RASTER * rin, RASTER * rout, int x1, int y1, int x2,
                           int y2, int newx, int newy, int abs_zoom_level,
                           int mirror, int ninety) {
        int tmp, newlx, newly;
        int rasras, factor, abszl;

        abszl  = abs_zoom_level;
        factor = 1 << abs_zoom_level;
        if (factor == 1)
          rop_copy_90(rin, rout, x1, y1, x2, y2, newx, newy, mirror, ninety);

        /* raddrizzo gli estremi */
        if (x1 > x2) {
          tmp = x1;
          x1  = x2;
          x2  = tmp;
        }
        if (y1 > y2) {
          tmp = y1;
          y1  = y2;
          y2  = tmp;
        }

        /* controllo gli sconfinamenti */
        if (ninety & 1) {
          newlx = (y2 - y1 + factor) / factor;
          newly = (x2 - x1 + factor) / factor;
        } else {
          newlx = (x2 - x1 + factor) / factor;
          newly = (y2 - y1 + factor) / factor;
        }
        if (x1 < 0 || y1 < 0 || x2 >= rin->lx || y2 >= rin->ly || newx < 0 ||
            newy < 0 || newx + newlx > rout->lx || newy + newly > rout->ly) {
          printf(
              "### INTERNAL ERROR - rop_zoom_out; access violation\n"
              " ((%d,%d)(%d,%d)in(%dx%d)->(%d,%d)in(%dx%d))\n",
              x1, y1, x2, y2, rin->lx, rin->ly, newx, newy, rout->lx, rout->ly);
          return;
        }

        rasras = RASRAS(rin->type, rout->type);
        switch (rasras) {
          I 62 CASE RASRAS(RAS_WB, RAS_RGB16)
              : __OR RASRAS(RAS_BW, RAS_RGB16)
              : rop_zoom_out_90_bw_rgb16(rin, rout, x1, y1, x2, y2, newx, newy,
                                         abszl, mirror, ninety);

          CASE RASRAS(RAS_WB, RAS_RGB_)
              : __OR RASRAS(RAS_WB, RAS_RGBM)
              : __OR RASRAS(RAS_BW, RAS_RGB_)
              : __OR RASRAS(RAS_BW, RAS_RGBM)
              : rop_zoom_out_90_bw_rgbm(rin, rout, x1, y1, x2, y2, newx, newy,
                                        abszl, mirror, ninety);

          E 62 CASE RASRAS(RAS_GR8, RAS_RGB16)
              : rop_zoom_out_90_gr8_rgb16(rin, rout, x1, y1, x2, y2, newx, newy,
                                          abszl, mirror, ninety);

          CASE RASRAS(RAS_GR8, RAS_RGB_)
              : __OR RASRAS(RAS_GR8, RAS_RGBM)
              : rop_zoom_out_90_gr8_rgbm(rin, rout, x1, y1, x2, y2, newx, newy,
                                         abszl,
I 63 mirror, ninety);

          CASE RASRAS(RAS_RGB, RAS_RGB16)
              : rop_zoom_out_90_rgb_rgb16(rin, rout, x1, y1, x2, y2, newx, newy,
                                          abszl, mirror, ninety);

          CASE RASRAS(RAS_RGB, RAS_RGB_)
              : __OR RASRAS(RAS_RGB, RAS_RGBM)
              : rop_zoom_out_90_rgb_rgbm(rin, rout, x1, y1, x2, y2, newx, newy,
                                         abszl,
E 63 mirror, ninety);

        DEFAULT:
          assert(!"rop_zoom_out_90; invalid raster combination");
          E 57
        }
      }

      /*---------------------------------------------------------------------------*/

      void rop_clear(RASTER * r, int x1, int y1, int x2, int y2) {
        UCHAR *pix;
        D 89 int l, x, y, tmp, rowsize, wrap, pixbits;
        E 89
I 89 ULONG *row24, *pix24;
        D 93 int lines, x, y, lx, tmp, rowsize, wrap, pixbits;
        E 93
I 93 int lines, x, y, lx, tmp, rowbytes, wrap, bytewrap, pixbits;
        E 93
E 89

            if (x1 > x2) {
          tmp = x1;
          x1  = x2;
          x2  = tmp;
        }
        if (y1 > y2) {
          tmp = y1;
          y1  = y2;
          y2  = tmp;
        }

        /* controllo gli sconfinamenti */
        if (x1 < 0 || y1 < 0 || x2 - x1 >= r->lx || y2 - y1 >= r->ly) {
          printf("### INTERNAL ERROR - rop_clear; access violation\n");
          return;
        }

        pixbits = rop_pixbits(r->type);

        /* per adesso niente pixel non multipli di 8 bits */
        if (rop_fillerbits(r->type)) {
          printf("### INTERNAL ERROR - rop_clear; byte fraction pixels\n");
          return;
        }

        D 93 wrap = (r->wrap * pixbits) >> 3;
        rowsize    = ((x2 - x1 + 1) * pixbits) >> 3;
        E 93
I 93 wrap        = r->wrap;
        bytewrap   = (wrap * pixbits + 7) >> 3;
        rowbytes   = ((x2 - x1 + 1) * pixbits) >> 3;
        E 93
D 89 pix    = (UCHAR *)r->buffer + (((x1 + y1 * r->wrap) * pixbits) >> 3);
        l     = y2 - y1 + 1;
        while (l-- > 0)
					E 89
I 89 lx     = x2 - x1 + 1;
        lines = y2 - y1 + 1;
        if (r->type == RAS_CM24) {
          row24 = (ULONG *)r->buffer + x1 + y1 * wrap;
          while (lines-- > 0) {
            pix24 = row24;
            for (x = 0; x < lx; x++) *pix24++ &= 0xff000000;
            row24 += wrap;
          }
        } else {
          D 93 pix = (UCHAR *)r->buffer + (((x1 + y1 * wrap) * pixbits) >> 3);
          E 93
I 93 pix = (UCHAR *)r->buffer + ((x1 * pixbits) >> 3) + y1 * bytewrap;
          E 93 while (lines-- > 0) {
            D 93 memset(pix, 0, rowsize);
            pix += wrap;
            E 93
I 93 memset(pix, 0, rowbytes);
            pix += bytewrap;
            E 93
          }
        }
      }

      /*---------------------------------------------------------------------------*/

      void rop_and_extra_bits(RASTER * ras, UCHAR and_mask, int x1, int y1,
                              int x2, int y2) {
        int x, y, lx, ly, wrap;
        ULONG *row24, *pix24, and_mask24;
        UCHAR *row8, *pix8;

        lx   = x2 - x1 + 1;
        ly   = y2 - y1 + 1;
        wrap = ras->wrap;
        if (ras->type == RAS_CM24) {
          and_mask24 = (and_mask << 24) | 0xffffff;
          row24      = (ULONG *)ras->buffer + x1 + y1 * wrap;
          for (y = 0; y < ly; y++, row24 += wrap) {
            pix24 = row24;
            for (x = 0; x < lx; x++, pix24++) *pix24 &= and_mask24;
          }
        } else
          E 89 {
            D 89 memset(pix, 0, rowsize);
            pix += wrap;
            E 89
I 89 if (!ras->extra) return;
            row8 = ras->extra + x1 + y1 * wrap;
            for (y = 0; y < ly; y++, row8 += wrap) {
              pix8 = row8;
              if (and_mask)
                for (x = 0; x < lx; x++, pix8++) *row8 &= and_mask;
              else
                memset(row8, 0, lx);
            }
            E 89
          }
      }

      /*---------------------------------------------------------------------------*/

      void rop_add_white_to_cmap(RASTER * ras) {
        int i;
        UCHAR m, white;
        I 82
D 84 int cmap_size;
        E 84
I 84 int cmap_size, colbuf_size, penbuf_size;
        LPIXEL *buffer, *penbuffer, *colbuffer;
        E 84
E 82

D 82 for (i = 0; i < ras->cmap.size; i++)
E 82
I 82
D 84 cmap_size = TCM_MIN_CMAP_BUFFER_SIZE(ras->cmap.info);
        for (i = 0; i < cmap_size; i++)
					E 82 {
            m = ras->cmap.buffer[i].m;
            if (ras->cmap.buffer[i].r > m || ras->cmap.buffer[i].g > m ||
                ras->cmap.buffer[i].b > m)
              break;
            white = (UCHAR)255 - m;
            ras->cmap.buffer[i].r += white;
            ras->cmap.buffer[i].g += white;
            ras->cmap.buffer[i].b += white;
            E 84
I 84 buffer         = ras->cmap.buffer;
            penbuffer = ras->cmap.penbuffer;
            colbuffer = ras->cmap.colbuffer;
            if (buffer) {
              cmap_size = TCM_MIN_CMAP_BUFFER_SIZE(ras->cmap.info);
              for (i = 0; i < cmap_size; i++) {
                m = buffer[i].m;
                if (buffer[i].r > m || buffer[i].g > m || buffer[i].b > m)
                  break;
                white = (UCHAR)255 - m;
                buffer[i].r += white;
                buffer[i].g += white;
                buffer[i].b += white;
              }
              if (i < cmap_size)
                fprintf(stderr, "\7add_white: cmap is not premultiplied\n");
            } else if (colbuffer && penbuffer) {
              colbuf_size = TCM_MIN_CMAP_COLBUFFER_SIZE(ras->cmap.info);
              penbuf_size = TCM_MIN_CMAP_PENBUFFER_SIZE(ras->cmap.info);

              for (i = 0; i < colbuf_size; i++) {
                m = colbuffer[i].m;
                if (colbuffer[i].r > m || colbuffer[i].g > m ||
                    colbuffer[i].b > m)
                  break;
                white = (UCHAR)i - m; /* i & 0xff == tone */
                colbuffer[i].r += white;
                colbuffer[i].g += white;
                colbuffer[i].b += white;
              }
              if (i < colbuf_size)
                fprintf(stderr,
                        "\7add_white: color cmap is not premultiplied\n");

              for (i = 0; i < penbuf_size; i++) {
                m = penbuffer[i].m;
                if (penbuffer[i].r > m || penbuffer[i].g > m ||
                    penbuffer[i].b > m)
                  break;
                white = (UCHAR)~i - m; /* 255 - (i & 0xff) == 255 - tone */
                penbuffer[i].r += white;
                penbuffer[i].g += white;
                penbuffer[i].b += white;
              }
              if (i < penbuf_size)
                fprintf(stderr,
                        "\7add_white: pencil cmap is not premultiplied\n");
              E 84
            }
            D 82 if (i < ras->cmap.size)
E 82
I 82
D 84 if (i < cmap_size)
E 82 fprintf(stderr, "\7add_white: cmap is not premultiplied\n");
            E 84
          }

        /*---------------------------------------------------------------------------*/

        void rop_remove_white_from_cmap(RASTER * ras) {
          int i;
          UCHAR m, white;
          I 82
D 84 int cmap_size;
          E 84
I 84 int cmap_size, colbuf_size, penbuf_size;
          LPIXEL *buffer, *penbuffer, *colbuffer;
          E 84
E 82

D 82 for (i = 0; i < ras->cmap.size; i++)
E 82
I 82
D 84 cmap_size = TCM_MIN_CMAP_BUFFER_SIZE(ras->cmap.info);
          for (i = 0; i < cmap_size; i++)
						E 84
I 84 buffer       = ras->cmap.buffer;
          penbuffer = ras->cmap.penbuffer;
          colbuffer = ras->cmap.colbuffer;
          if (buffer) {
            cmap_size = TCM_MIN_CMAP_BUFFER_SIZE(ras->cmap.info);
            for (i = 0; i < cmap_size; i++) {
              m     = buffer[i].m;
              white = (UCHAR)255 - m;
              buffer[i].r -= white;
              buffer[i].g -= white;
              buffer[i].b -= white;
              if (buffer[i].r > m || buffer[i].g > m || buffer[i].b > m) break;
            }
            if (i < cmap_size)
              fprintf(stderr, "\7remove_white: cmap is not premultiplied\n");
          } else if (colbuffer && penbuffer)
            E 84
E 82 {
              D 84 m = ras->cmap.buffer[i].m;
              white   = (UCHAR)255 - m;
              ras->cmap.buffer[i].r -= white;
              ras->cmap.buffer[i].g -= white;
              ras->cmap.buffer[i].b -= white;
              if (ras->cmap.buffer[i].r > m || ras->cmap.buffer[i].g > m ||
                  ras->cmap.buffer[i].b > m)
                break;
              E 84
I 84 colbuf_size        = TCM_MIN_CMAP_COLBUFFER_SIZE(ras->cmap.info);
              penbuf_size = TCM_MIN_CMAP_PENBUFFER_SIZE(ras->cmap.info);

              for (i = 0; i < colbuf_size; i++) {
                m     = colbuffer[i].m;
                white = (UCHAR)i - m; /* i & 0xff == tone */
                colbuffer[i].r -= white;
                colbuffer[i].g -= white;
                colbuffer[i].b -= white;
                if (colbuffer[i].r > m || colbuffer[i].g > m ||
                    colbuffer[i].b > m)
                  break;
              }
              if (i < colbuf_size)
                fprintf(stderr,
                        "\7add_white: color cmap is not premultiplied\n");

              for (i = 0; i < penbuf_size; i++) {
                m     = penbuffer[i].m;
                white = (UCHAR)~i - m; /* 255 - (i & 0xff) == 255 - tone */
                penbuffer[i].r -= white;
                penbuffer[i].g -= white;
                penbuffer[i].b -= white;
                if (penbuffer[i].r > m || penbuffer[i].g > m ||
                    penbuffer[i].b > m)
                  break;
              }
              if (i < penbuf_size)
                fprintf(stderr,
                        "\7add_white: pencil cmap is not premultiplied\n");
              E 84
            }
          D 82 if (i < ras->cmap.size)
E 82
I 82
D 84 if (i < cmap_size)
E 82 fprintf(stderr, "\7remove_white: cmap is not premultiplied\n");
          E 84
        }

        /*---------------------------------------------------------------------------*/

        D 84 static LPIXEL premult_lpixel(LPIXEL lpixel)
E 84
I 84 LPIXEL premult_lpixel(LPIXEL lpixel)
E 84 {
          D 84 int m;
          E 84
I 84 UINT m, mm;
          E 84 LPIXEL new_lpixel;

          m = lpixel.m;
          if (m == 255)
						D 84 return lpixel;
          E 84
I 84 new_lpixel = lpixel;
          E 84 else if (m == 0) {
            new_lpixel.r = 0;
            new_lpixel.g = 0;
            new_lpixel.b = 0;
            new_lpixel.m = 0;
          }
          else {
            D 84 new_lpixel.r = (lpixel.r * m + 127) / 255;
            new_lpixel.g       = (lpixel.g * m + 127) / 255;
            new_lpixel.b       = (lpixel.b * m + 127) / 255;
            E 84
I 84 mm                      = m * MAGICFAC;
            new_lpixel.r       = (lpixel.r * mm + (1U << 23)) >> 24;
            new_lpixel.g       = (lpixel.g * mm + (1U << 23)) >> 24;
            new_lpixel.b       = (lpixel.b * mm + (1U << 23)) >> 24;
            E 84 new_lpixel.m = m;
          }
          return new_lpixel;
        }

        /*---------------------------------------------------------------------------*/

        D 84 static LPIXEL unpremult_lpixel(LPIXEL lpixel)
E 84
I 84 LPIXEL unpremult_lpixel(LPIXEL lpixel)
E 84 {
          int m, m_2;
          LPIXEL new_lpixel;

          m = lpixel.m;
          if (m == 255)
            return lpixel;
          else if (m == 0) {
            new_lpixel.r = 255;
            new_lpixel.g = 255;
            new_lpixel.b = 255;
            new_lpixel.m = 0;
          } else {
            m_2                = m >> 1;
            new_lpixel.r       = (lpixel.r * 255 + m_2) / m;
            new_lpixel.g       = (lpixel.g * 255 + m_2) / m;
            new_lpixel.b       = (lpixel.b * 255 + m_2) / m;
            E 52 new_lpixel.m = m;
          }
          return new_lpixel;
        }

        /*---------------------------------------------------------------------------*/

        void rop_fill_cmap_ramp(RASTER * ras, TCM_INFO info,
D 89 LPIXEL color, LPIXEL pencil, int color_index, int pencil_index,
                                int already_premultiplied) {
          LPIXEL val, *ramp;
          int ramp_index;
          int c_r, c_g, c_b, c_m, p_r, p_g, p_b, p_m, d_r, d_g, d_b, d_m;
          int tone, tmax, tmax_2;
          int tmax_2_r, tmax_2_g, tmax_2_b, tmax_2_m;

          I 34
D 35
              /* Secondo noi (Grisu & Roberto) l'offset non ci vuole
E 34
ramp_index = color_index  << info.color_offs  |
pencil_index << info.pencil_offs | info.offset_mask;
I 34
*/
              ramp_index = color_index << info.color_offs |
                           pencil_index << info.pencil_offs;
          E 35
I 35 ramp_index =
              color_index << info.color_offs | pencil_index << info.pencil_offs;
          E 35
E 34 ramp = ras->cmap.buffer + ramp_index;
          if (!already_premultiplied) {
            color  = premult_lpixel(color);
            pencil = premult_lpixel(pencil);
          }
          c_r      = color.r;
          p_r      = pencil.r;
          d_r      = c_r - p_r;
          c_g      = color.g;
          p_g      = pencil.g;
          d_g      = c_g - p_g;
          c_b      = color.b;
          p_b      = pencil.b;
          d_b      = c_b - p_b;
          c_m      = color.m;
          p_m      = pencil.m;
          d_m      = c_m - p_m;
          tmax     = info.n_tones - 1;
          tmax_2   = tmax >> 1;
          tmax_2_r = d_r < 0 ? -tmax_2 : tmax_2;
          tmax_2_g = d_g < 0 ? -tmax_2 : tmax_2;
          tmax_2_b = d_b < 0 ? -tmax_2 : tmax_2;
          tmax_2_m = d_m < 0 ? -tmax_2 : tmax_2;
          ramp[0]  = pencil;
          for (tone = 1; tone < info.n_tones - 1; tone++) {
            val.r = p_r + (d_r * tone + tmax_2_r) / tmax;
            val.g = p_g + (d_g * tone + tmax_2_g) / tmax;
            val.b = p_b + (d_b * tone + tmax_2_b) / tmax;
            val.m = p_m + (d_m * tone + tmax_2_m) / tmax;
            D 82 ramp[tone << info.tone_offs] = val;
            E 82
I 82 ramp[tone]                              = val;
            E 82
          }
          ramp[info.n_tones - 1] = color;
        }

        /*---------------------------------------------------------------------------*/

        I 84 void rop_fill_cmap_colramp(RASTER * ras, TCM_INFO info,
                                         LPIXEL color, int color_index,
                                         int already_premultiplied) {
          LPIXEL val, *colbuffer;
          int index, tone;
          UINT magic_tone, c_r, c_g, c_b, c_m;

          if (!already_premultiplied) color = premult_lpixel(color);
          c_r                               = color.r;
          c_g                               = color.g;
          c_b                               = color.b;
          c_m                               = color.m;
          colbuffer                         = ras->cmap.colbuffer;
          index                             = color_index << info.tone_bits;
          for (tone = 0; tone < info.n_tones; tone++) {
            magic_tone         = tone * MAGICFAC;
            val.r              = (UCHAR)((c_r * magic_tone + (1 << 23)) >> 24);
            val.g              = (UCHAR)((c_g * magic_tone + (1 << 23)) >> 24);
            val.b              = (UCHAR)((c_b * magic_tone + (1 << 23)) >> 24);
            val.m              = (UCHAR)((c_m * magic_tone + (1 << 23)) >> 24);
            colbuffer[index++] = val;
          }
        }

        /*---------------------------------------------------------------------------*/

        void rop_fill_cmap_penramp(RASTER * ras, TCM_INFO info, LPIXEL pencil,
                                   int pencil_index,
                                   int already_premultiplied) {
          LPIXEL val, *penbuffer;
          int index, enot;
          UINT magic_enot, p_r, p_g, p_b, p_m;

          if (!already_premultiplied)
						D 88 pencil = premult(pencil);
          E 88
I 88 pencil       = rop_premult(pencil);
          E 88 p_r = pencil.r;
          p_g       = pencil.g;
          p_b       = pencil.b;
          p_m       = pencil.m;
          penbuffer = ras->cmap.penbuffer;
          index     = pencil_index << info.tone_bits;
          for (enot = info.n_tones - 1; enot >= 0; enot--) {
            magic_enot         = enot * MAGICFAC;
            val.r              = (UCHAR)((p_r * magic_enot + (1 << 23)) >> 24);
            val.g              = (UCHAR)((p_g * magic_enot + (1 << 23)) >> 24);
            val.b              = (UCHAR)((p_b * magic_enot + (1 << 23)) >> 24);
            val.m              = (UCHAR)((p_m * magic_enot + (1 << 23)) >> 24);
            penbuffer[index++] = val;
          }
        }

        /*---------------------------------------------------------------------------*/

        E 84
I 36 void rop_custom_fill_cmap_ramp(
            RASTER * ras, TCM_INFO info, LPIXEL color, LPIXEL pencil,
            int color_index, int pencil_index, int already_premultiplied,
            int *custom_tone) {
          LPIXEL val, *ramp;
          int ramp_index;
          int c_r, c_g, c_b, c_m, p_r, p_g, p_b, p_m, d_r, d_g, d_b, d_m;
          int tone, tmax, tmax_2;
          int tmax_2_r, tmax_2_g, tmax_2_b, tmax_2_m;

          ramp_index =
              color_index << info.color_offs | pencil_index << info.pencil_offs;
          ramp = ras->cmap.buffer + ramp_index;
          if (!already_premultiplied) {
            color  = premult_lpixel(color);
            pencil = premult_lpixel(pencil);
          }
          c_r           = color.r;
          p_r           = pencil.r;
          d_r           = c_r - p_r;
          c_g           = color.g;
          p_g           = pencil.g;
          d_g           = c_g - p_g;
          c_b           = color.b;
          p_b           = pencil.b;
          d_b           = c_b - p_b;
          c_m           = color.m;
          p_m           = pencil.m;
          d_m           = c_m - p_m;
          tmax          = info.n_tones - 1;
          tmax_2        = tmax >> 1;
          tmax_2_r      = d_r < 0 ? -tmax_2 : tmax_2;
          tmax_2_g      = d_g < 0 ? -tmax_2 : tmax_2;
          tmax_2_b      = d_b < 0 ? -tmax_2 : tmax_2;
          tmax_2_m      = d_m < 0 ? -tmax_2 : tmax_2;
          D 37 ramp[0] = pencil;
          for (tone = 1; tone < info.n_tones - 1; tone++)
            E 37
I 37 for (tone = 0; tone < info.n_tones; tone++)
E 37 {
              D 37
                  /* only the following 4 lines are different from the
                     non-custom version */
E 37 val.r        = p_r + (d_r * custom_tone[tone] + tmax_2_r) / tmax;
              val.g = p_g + (d_g * custom_tone[tone] + tmax_2_g) / tmax;
              val.b = p_b + (d_b * custom_tone[tone] + tmax_2_b) / tmax;
              val.m = p_m + (d_m * custom_tone[tone] + tmax_2_m) / tmax;
              D 82 ramp[tone << info.tone_offs] = val;
              E 82
I 82 ramp[tone]                                = val;
              E 82
            }
          D 37 ramp[info.n_tones - 1] = color;
          E 37
        }

        /*---------------------------------------------------------------------------*/

        E 36
D 84 void rop_fill_cmap_buffer(RASTER * ras, TCM_INFO info, LPIXEL * color,
                                 LPIXEL * pencil, int already_premultiplied) {
          D 82 int min_cmap_buffer_size, i, j;
          E 82
I 82 int i, j;
          E 82

D 82 min_cmap_buffer_size = TCM_MIN_CMAP_BUFFER_SIZE(info);
          D 33 assert(min_cmap_buffer_size < ras->cmap.size);
          E 33
I 33 assert(ras->cmap.size >= min_cmap_buffer_size);
          E 82
E 33 for (i = 0; i < info.n_colors; i++) for (j = 0; j < info.n_pencils; j++)
              rop_fill_cmap_ramp(ras, info, color[i], pencil[j], i, j,
                                 already_premultiplied);
          I 36
        }

        /*---------------------------------------------------------------------------*/

        void rop_custom_fill_cmap_buffer(
            RASTER * ras, TCM_INFO info, LPIXEL * color, LPIXEL * pencil,
            int already_premultiplied, int *custom_tone) {
          D 82 int min_cmap_buffer_size, i, j;
          E 82
I 82 int i, j;
          E 82

D 82 min_cmap_buffer_size = TCM_MIN_CMAP_BUFFER_SIZE(info);
          assert(ras->cmap.size >= min_cmap_buffer_size);
          E 82 for (i = 0; i < info.n_colors;
                     i++) for (j = 0; j < info.n_pencils; j++)
              rop_custom_fill_cmap_ramp(ras, info, color[i], pencil[j], i, j,
                                        already_premultiplied, custom_tone);
          E 36
E 32
        }
        I 66

            /*---------------------------------------------------------------------------*/
E 84
I 84 void
        rop_custom_fill_cmap_colramp(RASTER * ras, TCM_INFO info, LPIXEL color,
                                     int color_index, int already_premultiplied,
                                     int *custom_tone) {
          LPIXEL val, *colbuffer;
          int index, tone;
          UINT magic_tone, c_r, c_g, c_b, c_m;
          E 84

I 77
D 84 void release_raster(RASTER * raster) {
            if (!raster->native_buffer)
              E 84
I 84 if (!already_premultiplied) color = premult_lpixel(color);
            c_r                          = color.r;
            c_g                          = color.g;
            c_b                          = color.b;
            c_m                          = color.m;
            colbuffer                    = ras->cmap.colbuffer;
            index                        = color_index << info.tone_bits;
            for (tone = 0; tone < info.n_tones; tone++)
							E 84 {
                D 84 tmsg_error("release_raster,  missing buffer");
                return;
                E 84
I 84 magic_tone     = custom_tone[tone] * MAGICFAC;
                val.r = (UCHAR)((c_r * magic_tone + (1 << 23)) >> 24);
                val.g = (UCHAR)((c_g * magic_tone + (1 << 23)) >> 24);
                val.b = (UCHAR)((c_b * magic_tone + (1 << 23)) >> 24);
                val.m = (UCHAR)((c_m * magic_tone + (1 << 23)) >> 24);
                colbuffer[index++] = val;
                E 84
              }
            D 84

                release_memory_chunk(raster->native_buffer);

            if (raster->type == RAS_CM16 && raster->cmap.buffer)
              free(raster->cmap.buffer);

            memset(raster, 0, sizeof(RASTER));
            return;
          }

          /*-----------------------------------------------------------------*/

          void create_raster(RASTER * raster, int xsize, int ysize,
                             RAS_TYPE type) {
            int pixsize;

            memset(raster, 0, sizeof(RASTER));

            pixsize = rop_pixbytes(type);

            if (!(raster->native_buffer =
                      get_memory_chunk(xsize * ysize * pixsize)))
              tmsg_fatal("can't allocate %d Mbytes",
                         ((xsize / 1024) * (ysize / 1024)) * pixsize);

            raster->buffer = raster->native_buffer;
            raster->type   = type;
            raster->wrap = raster->lx = xsize;
            raster->ly                = ysize;
            return;
          }

          I 80
              /*-----------------------------------------------------------------*/

              int
              create_subraster(RASTER * rin, RASTER * rout, int x0, int y0,
                               int x1, int y1) {
            if (x1 < x0 || y1 < y0) return FALSE;

            *rout                    = *rin;
            if (x0 < 0) x0           = 0;
            if (y0 < 0) y0           = 0;
            if (x1 > rin->lx - 1) x1 = rin->lx - 1;
            if (y1 > rin->ly - 1) y1 = rin->ly - 1;

            rout->lx     = x1 - x0 + 1;
            rout->ly     = y1 - y0 + 1;
            rout->buffer = (UCHAR *)(rin->buffer) +
                           (y0 * rin->wrap + x0) * rop_pixbytes(rin->type);
            return TRUE;
            E 84
          }

          D 84
              /*-----------------------------------------------------------------*/
E 84
I 84
              /*---------------------------------------------------------------------------*/
E 84

D 84 void
          clone_raster(RASTER * rin, RASTER * rout)
E 84
I 84 void
          rop_custom_fill_cmap_penramp(
              RASTER * ras, TCM_INFO info, LPIXEL pencil, int pencil_index,
              int already_premultiplied, int *custom_tone)
E 84 {
            D 84 create_raster(rout, rin->lx, rin->ly, rin->type);
            rop_copy(rin, rout, 0, 0, rin->lx - 1, rin->ly - 1, 0, 0);
          }
          E 84
I 84 LPIXEL val, *penbuffer;
          int index, enot;
          UINT magic_enot, p_r, p_g, p_b, p_m;
          E 84

I 84 if (!already_premultiplied)
D 88 pencil       = premult(pencil);
          E 88
I 88 pencil       = rop_premult(pencil);
          E 88
E 84
E 80
E 77
E 66
E 25
E 6
E 1
I 84 p_r          = pencil.r;
          p_g       = pencil.g;
          p_b       = pencil.b;
          p_m       = pencil.m;
          penbuffer = ras->cmap.penbuffer;
          index     = pencil_index << info.tone_bits;
          for (enot = info.n_tones - 1; enot >= 0; enot--) {
            magic_enot         = custom_tone[enot] * MAGICFAC;
            val.r              = (UCHAR)((p_r * magic_enot + (1 << 23)) >> 24);
            val.g              = (UCHAR)((p_g * magic_enot + (1 << 23)) >> 24);
            val.b              = (UCHAR)((p_b * magic_enot + (1 << 23)) >> 24);
            val.m              = (UCHAR)((p_m * magic_enot + (1 << 23)) >> 24);
            penbuffer[index++] = val;
          }
        }

        /*---------------------------------------------------------------------------*/

        void rop_fill_cmap_buffer(RASTER * ras, TCM_INFO info, LPIXEL * color,
                                  LPIXEL * pencil, int already_premultiplied) {
          int i, j;

          for (i = 0; i < info.n_colors; i++)
            for (j = 0; j < info.n_pencils; j++)
              rop_fill_cmap_ramp(ras, info, color[i], pencil[j], i, j,
                                 already_premultiplied);
        }

        /*---------------------------------------------------------------------------*/

        void rop_fill_cmap_colbuffer(RASTER * ras, TCM_INFO info,
                                     LPIXEL * color,
                                     int already_premultiplied) {
          int i;

          for (i = 0; i < info.n_colors; i++)
            rop_fill_cmap_colramp(ras, info, color[i], i,
                                  already_premultiplied);
        }

E 89
I 89
                         LPIXEL color, LPIXEL pencil,
		         int color_index, int pencil_index,
		         int already_premultiplied)
{
  LPIXEL val, *ramp;
  int ramp_index;
  int c_r, c_g, c_b, c_m, p_r, p_g, p_b, p_m, d_r, d_g, d_b, d_m;
  int tone, tmax, tmax_2;
  int tmax_2_r, tmax_2_g, tmax_2_b, tmax_2_m;

  ramp_index =
      color_index << info.color_offs | pencil_index << info.pencil_offs;
  ramp = ras->cmap.buffer + ramp_index;
  if (!already_premultiplied) {
    color  = premult_lpixel(color);
    pencil = premult_lpixel(pencil);
  }
  c_r            = color.r;
  p_r            = pencil.r;
  d_r            = c_r - p_r;
  c_g            = color.g;
  p_g            = pencil.g;
  d_g            = c_g - p_g;
  c_b            = color.b;
  p_b            = pencil.b;
  d_b            = c_b - p_b;
  D 94 c_m      = color.m;
  p_m            = pencil.m;
  d_m            = c_m - p_m;
  tmax           = info.n_tones - 1;
  tmax_2         = tmax >> 1;
  tmax_2_r       = d_r < 0 ? -tmax_2 : tmax_2;
  E 94
I 94 c_m       = color.m;
  p_m            = pencil.m;
  d_m            = c_m - p_m;
  tmax           = info.n_tones - 1;
  tmax_2         = tmax >> 1;
  tmax_2_r       = d_r < 0 ? -tmax_2 : tmax_2;
  E 94 tmax_2_g = d_g < 0 ? -tmax_2 : tmax_2;
  tmax_2_b       = d_b < 0 ? -tmax_2 : tmax_2;
  tmax_2_m       = d_m < 0 ? -tmax_2 : tmax_2;
  ramp[0]        = pencil;
  for (tone = 1; tone < info.n_tones - 1; tone++) {
    val.r      = p_r + (d_r * tone + tmax_2_r) / tmax;
    val.g      = p_g + (d_g * tone + tmax_2_g) / tmax;
    val.b      = p_b + (d_b * tone + tmax_2_b) / tmax;
    val.m      = p_m + (d_m * tone + tmax_2_m) / tmax;
    ramp[tone] = val;
  }
  ramp[info.n_tones - 1] = color;
}

/*---------------------------------------------------------------------------*/

void rop_fill_cmap_colramp(RASTER * ras, TCM_INFO info, LPIXEL color,
                           int color_index, int already_premultiplied) {
  LPIXEL val, *colbuffer;
  int index, tone;
  UINT magic_tone, c_r, c_g, c_b, c_m;

  if (!already_premultiplied) color = premult_lpixel(color);
  c_r                               = color.r;
  c_g                               = color.g;
  c_b                               = color.b;
  c_m                               = color.m;
  colbuffer                         = ras->cmap.colbuffer;
  index                             = color_index << info.tone_bits;
  for (tone = 0; tone < info.n_tones; tone++) {
    magic_tone         = tone * MAGICFAC;
    val.r              = (UCHAR)((c_r * magic_tone + (1 << 23)) >> 24);
    val.g              = (UCHAR)((c_g * magic_tone + (1 << 23)) >> 24);
    val.b              = (UCHAR)((c_b * magic_tone + (1 << 23)) >> 24);
    val.m              = (UCHAR)((c_m * magic_tone + (1 << 23)) >> 24);
    colbuffer[index++] = val;
  }
}

/*---------------------------------------------------------------------------*/

void rop_fill_cmap_penramp(RASTER * ras, TCM_INFO info, LPIXEL pencil,
                           int pencil_index, int already_premultiplied) {
  LPIXEL val, *penbuffer;
  int index, enot;
  UINT magic_enot, p_r, p_g, p_b, p_m;

  if (!already_premultiplied) pencil = premult_lpixel(pencil);
  p_r                                = pencil.r;
  p_g                                = pencil.g;
  p_b                                = pencil.b;
  p_m                                = pencil.m;
  penbuffer                          = ras->cmap.penbuffer;
  index                              = pencil_index << info.tone_bits;
  for (enot = info.n_tones - 1; enot >= 0; enot--) {
    magic_enot         = enot * MAGICFAC;
    val.r              = (UCHAR)((p_r * magic_enot + (1 << 23)) >> 24);
    val.g              = (UCHAR)((p_g * magic_enot + (1 << 23)) >> 24);
    val.b              = (UCHAR)((p_b * magic_enot + (1 << 23)) >> 24);
    val.m              = (UCHAR)((p_m * magic_enot + (1 << 23)) >> 24);
    penbuffer[index++] = val;
  }
}

/*---------------------------------------------------------------------------*/

void rop_custom_fill_cmap_ramp(RASTER * ras, TCM_INFO info, LPIXEL color,
                               LPIXEL pencil, int color_index, int pencil_index,
                               int already_premultiplied, int *custom_tone) {
  LPIXEL val, *ramp;
  int ramp_index;
  int c_r, c_g, c_b, c_m, p_r, p_g, p_b, p_m, d_r, d_g, d_b, d_m;
  int tone, tmax, tmax_2;
  int tmax_2_r, tmax_2_g, tmax_2_b, tmax_2_m;

  ramp_index =
      color_index << info.color_offs | pencil_index << info.pencil_offs;
  ramp = ras->cmap.buffer + ramp_index;
  if (!already_premultiplied) {
    color  = premult_lpixel(color);
    pencil = premult_lpixel(pencil);
  }
  c_r      = color.r;
  p_r      = pencil.r;
  d_r      = c_r - p_r;
  c_g      = color.g;
  p_g      = pencil.g;
  d_g      = c_g - p_g;
  c_b      = color.b;
  p_b      = pencil.b;
  d_b      = c_b - p_b;
  c_m      = color.m;
  p_m      = pencil.m;
  d_m      = c_m - p_m;
  tmax     = info.n_tones - 1;
  tmax_2   = tmax >> 1;
  tmax_2_r = d_r < 0 ? -tmax_2 : tmax_2;
  tmax_2_g = d_g < 0 ? -tmax_2 : tmax_2;
  tmax_2_b = d_b < 0 ? -tmax_2 : tmax_2;
  tmax_2_m = d_m < 0 ? -tmax_2 : tmax_2;
  for (tone = 0; tone < info.n_tones; tone++) {
    val.r      = p_r + (d_r * custom_tone[tone] + tmax_2_r) / tmax;
    val.g      = p_g + (d_g * custom_tone[tone] + tmax_2_g) / tmax;
    val.b      = p_b + (d_b * custom_tone[tone] + tmax_2_b) / tmax;
    val.m      = p_m + (d_m * custom_tone[tone] + tmax_2_m) / tmax;
    ramp[tone] = val;
  }
}

/*---------------------------------------------------------------------------*/

void rop_custom_fill_cmap_colramp(RASTER * ras, TCM_INFO info, LPIXEL color,
                                  int color_index, int already_premultiplied,
                                  int *custom_tone) {
  LPIXEL val, *colbuffer;
  int index, tone;
  UINT magic_tone, c_r, c_g, c_b, c_m;

  if (!already_premultiplied) color = premult_lpixel(color);
  c_r                               = color.r;
  c_g                               = color.g;
  c_b                               = color.b;
  c_m                               = color.m;
  colbuffer                         = ras->cmap.colbuffer;
  index                             = color_index << info.tone_bits;
  for (tone = 0; tone < info.n_tones; tone++) {
    magic_tone         = custom_tone[tone] * MAGICFAC;
    val.r              = (UCHAR)((c_r * magic_tone + (1 << 23)) >> 24);
    val.g              = (UCHAR)((c_g * magic_tone + (1 << 23)) >> 24);
    val.b              = (UCHAR)((c_b * magic_tone + (1 << 23)) >> 24);
    val.m              = (UCHAR)((c_m * magic_tone + (1 << 23)) >> 24);
    colbuffer[index++] = val;
  }
}

/*---------------------------------------------------------------------------*/

void rop_custom_fill_cmap_penramp(RASTER * ras, TCM_INFO info, LPIXEL pencil,
                                  int pencil_index, int already_premultiplied,
                                  int *custom_tone) {
  LPIXEL val, *penbuffer;
  int index, tone, maxtone;
  UINT magic_enot, p_r, p_g, p_b, p_m;

  if (!already_premultiplied) pencil = premult_lpixel(pencil);
  p_r                                = pencil.r;
  p_g                                = pencil.g;
  p_b                                = pencil.b;
  p_m                                = pencil.m;
  penbuffer                          = ras->cmap.penbuffer;
  index                              = pencil_index << info.tone_bits;
  maxtone                            = info.n_tones - 1;
  for (tone = 0; tone < info.n_tones; tone++) {
    magic_enot         = (maxtone - custom_tone[tone]) * MAGICFAC;
    val.r              = (UCHAR)((p_r * magic_enot + (1 << 23)) >> 24);
    val.g              = (UCHAR)((p_g * magic_enot + (1 << 23)) >> 24);
    val.b              = (UCHAR)((p_b * magic_enot + (1 << 23)) >> 24);
    val.m              = (UCHAR)((p_m * magic_enot + (1 << 23)) >> 24);
    penbuffer[index++] = val;
  }
}

/*---------------------------------------------------------------------------*/

void rop_fill_cmap_buffer(RASTER * ras, TCM_INFO info, LPIXEL * color,
                          LPIXEL * pencil, int already_premultiplied) {
  int i, j;

  for (i = 0; i < info.n_colors; i++)
    for (j = 0; j < info.n_pencils; j++)
      rop_fill_cmap_ramp(ras, info, color[i], pencil[j], i, j,
                         already_premultiplied);
}

/*---------------------------------------------------------------------------*/

void rop_fill_cmap_colbuffer(RASTER * ras, TCM_INFO info, LPIXEL * color,
                             int already_premultiplied) {
  int i;

  for (i = 0; i < info.n_colors; i++)
    rop_fill_cmap_colramp(ras, info, color[i], i, already_premultiplied);
}

E 89
    /*---------------------------------------------------------------------------*/

    void
    rop_fill_cmap_penbuffer(RASTER * ras, TCM_INFO info, LPIXEL * pencil,
                            int already_premultiplied) {
  int i;

  for (i = 0; i < info.n_pencils; i++)
    rop_fill_cmap_penramp(ras, info, pencil[i], i, already_premultiplied);
}

/*---------------------------------------------------------------------------*/

void rop_custom_fill_cmap_buffer(RASTER * ras, TCM_INFO info, LPIXEL * color,
                                 LPIXEL * pencil, int already_premultiplied,
                                 int *custom_tone) {
  int i, j;

  for (i = 0; i < info.n_colors; i++)
    for (j = 0; j < info.n_pencils; j++)
      rop_custom_fill_cmap_ramp(ras, info, color[i], pencil[j], i, j,
                                already_premultiplied, custom_tone);
}

/*---------------------------------------------------------------------------*/

void rop_custom_fill_cmap_colbuffer(RASTER * ras, TCM_INFO info, LPIXEL * color,
                                    int already_premultiplied,
                                    int *custom_tone) {
  int i;

  for (i = 0; i < info.n_colors; i++)
    rop_custom_fill_cmap_colramp(ras, info, color[i], i, already_premultiplied,
                                 custom_tone);
}

/*---------------------------------------------------------------------------*/

void rop_custom_fill_cmap_penbuffer(RASTER * ras, TCM_INFO info,
                                    LPIXEL * pencil, int already_premultiplied,
                                    int *custom_tone) {
  int i;

  for (i = 0; i < info.n_pencils; i++)
    rop_custom_fill_cmap_penramp(ras, info, pencil[i], i, already_premultiplied,
                                 custom_tone);
}

/*---------------------------------------------------------------------------*/

void release_raster(RASTER * raster) {
  D 89 if (!raster->native_buffer)
E 89
I 89 rop_clear_extra(raster);

  if (!(raster->native_buffer || raster->buffer))
		E 89 {
      D 89 tmsg_error("release_raster,  missing buffer");
      return;
      E 89
I 89 tmsg_error("release_raster, missing buffer");
      return;
      E 89
    }
  I 89 if (raster->native_buffer)
I 99 {
    E 99 release_memory_chunk(raster->native_buffer);
    I 99 MEMORY_PRINTF("libero %x\n", raster->native_buffer);
  }
  E 99 else I 99 {
    E 99 release_memory_chunk(raster->buffer);
    I 99 MEMORY_PRINTF("libero %x\n", raster->buffer);
  }
  E 99
E 89

D 89 release_memory_chunk(raster->native_buffer);

  E 89 if (raster->type == RAS_CM16 && raster->cmap.buffer)
      free(raster->cmap.buffer);
  D 89

E 89
I 89 else if (raster->type == RAS_CM24 && raster->cmap.penbuffer) {
    free(raster->cmap.penbuffer);
    free(raster->cmap.colbuffer);
  }
  E 89 memset(raster, 0, sizeof(RASTER));
  return;
}

D 89
    /*-----------------------------------------------------------------*/
E 89
I 89
    /*---------------------------------------------------------------------------*/

    void
    rop_clear_extra(RASTER * raster) {
  if (raster->extra_mask && raster->type != RAS_CM24) {
    if (!(raster->native_extra || raster->extra)) {
      tmsg_error("release_extra, missing extra buffer");
      return;
    }
    if (raster->native_extra)
			I 99 {
        E 99 release_memory_chunk(raster->native_extra);
        I 99 MEMORY_PRINTF("libero extra %x\n", raster->native_extra);
      }
    E 99 else I 99 {
      E 99 release_memory_chunk(raster->extra);
      I 99 MEMORY_PRINTF("libero extra %x\n", raster->extra);
    }
    E 99
  }
  raster->extra_mask = 0;
}

/*---------------------------------------------------------------------------*/

void rop_clear_patches(RASTER * raster) {
  raster->extra_mask &= ~7;
  if (!raster->extra_mask) rop_clear_extra(raster);
}

/*---------------------------------------------------------------------------*/

void rop_clear_extra_but_not_patches(RASTER * raster) {
  raster->extra_mask &= 7;
  if (!raster->extra_mask) rop_clear_extra(raster);
}
E 89

I 89
    /*---------------------------------------------------------------------------*/

E 89 void
create_raster(RASTER * raster, int xsize, int ysize, RAS_TYPE type) {
  int pixsize;

  memset(raster, 0, sizeof(RASTER));

  pixsize = rop_pixbytes(type);

  if (!(raster->native_buffer = get_memory_chunk(xsize * ysize * pixsize)))
    D 86 tmsg_fatal("can't allocate %d Mbytes",
                     ((xsize / 1024) * (ysize / 1024)) * pixsize);
  E 86
I 86 tmsg_fatal("can't allocate %d Mbytes",
                  (xsize * ysize * pixsize + (512 * 1024)) / (1024 * 1024));
  E 86

I 99 MEMORY_PRINTF("alloco %x: %dX%dX%d=%d bytes\n", raster->native_buffer,
                     xsize, ysize, pixsize, xsize * ysize * pixsize);

  E 99 raster->buffer = raster->native_buffer;
  raster->type         = type;
  raster->wrap = raster->lx = xsize;
  raster->ly                = ysize;
  I 86 switch (type) {
    CASE RAS_CM16 : raster->cmap.info = Tcm_new_default_info;
    CASE RAS_CM24 : raster->cmap.info = Tcm_24_default_info;
  }
  E 86 return;
}

/*-----------------------------------------------------------------*/

D 89 int create_subraster(RASTER * rin, RASTER * rout, int x0, int y0, int x1,
                           int y1)
E 89
I 89 void
create_raster_with_extra(RASTER * raster, int xsize, int ysize, RAS_TYPE type,
                         UCHAR extra_mask) {
  create_raster(raster, xsize, ysize, type);

  raster->extra_mask = extra_mask;
  if (extra_mask && type != RAS_CM24) {
    if (!(raster->native_extra = get_memory_chunk(xsize * ysize)))
      tmsg_fatal("can't allocate %d Mbytes",
                 (xsize * ysize + (512 * 1024)) / (1024 * 1024));
    I 99 MEMORY_PRINTF("alloco extra %x: %dX%d=%d bytes\n",
                        raster->native_extra, xsize, ysize, xsize * ysize);
    E 99 raster->extra = raster->native_extra;
  }
}

/*-----------------------------------------------------------------*/

int create_subraster(RASTER * rin, RASTER * rout, int x0, int y0, int x1,
                     int y1)
E 89 {
  if (x1 < x0 || y1 < y0) return FALSE;

  *rout                    = *rin;
  if (x0 < 0) x0           = 0;
  if (y0 < 0) y0           = 0;
  if (x1 > rin->lx - 1) x1 = rin->lx - 1;
  if (y1 > rin->ly - 1) y1 = rin->ly - 1;

  rout->lx = x1 - x0 + 1;
  rout->ly = y1 - y0 + 1;
  rout->buffer =
      (UCHAR *)(rin->buffer) + (y0 * rin->wrap + x0) * rop_pixbytes(rin->type);
  I 89 if (rin->extra) rout->extra = rin->extra + x0 + y0 * rin->wrap;
  E 89 return TRUE;
}

/*-----------------------------------------------------------------*/

void clone_raster(RASTER * rin, RASTER * rout) {
  I 91 int size;

  E 91
D 89 create_raster(rout, rin->lx, rin->ly, rin->type);
  I 85
  
E 89
I 89
D 90 create_raster_with_extra(rout, rin->lx, rin->ly, rin->type,
                                rin->extra_mask);

  E 90
I 90
D 92 create_raster(rout, rin->lx, rin->ly, rin->type);
  E 92
I 92 create_raster_with_extra(rout, rin->lx, rin->ly, rin->type,
                                rin->extra_mask);
  E 92
  
E 90
E 89
E 85 rop_copy(rin, rout, 0, 0, rin->lx - 1, rin->ly - 1, 0, 0);
  I 85 if (rin->type == RAS_CM16 || rin->type == RAS_CM24)
I 90 {
    E 90 rout->cmap = rin->cmap;
    I 90
D 91 TCALLOC(rout->cmap.buffer, TCM_CMAP_BUFFER_SIZE(rout->cmap.info));
    memcpy(rout->cmap.buffer, rin->cmap.buffer,
           TCM_CMAP_BUFFER_SIZE(rout->cmap.info));
    if (rin->type == RAS_CM24)
      E 91
I 91

          if (rin->cmap.buffer)
E 91 {
        D 91 if (rin->cmap.penbuffer) {
          TCALLOC(rout->cmap.penbuffer,
                  TCM_CMAP_PENBUFFER_SIZE(rout->cmap.info));
          memcpy(rout->cmap.penbuffer, rin->cmap.penbuffer,
                 TCM_CMAP_PENBUFFER_SIZE(rout->cmap.info));
        }
        if (rin->cmap.colbuffer) {
          TCALLOC(rout->cmap.colbuffer,
                  TCM_CMAP_COLBUFFER_SIZE(rout->cmap.info));
          memcpy(rout->cmap.colbuffer, rin->cmap.colbuffer,
                 TCM_CMAP_COLBUFFER_SIZE(rout->cmap.info));
        }
        E 91
I 91 size = TCM_CMAP_BUFFER_SIZE(rout->cmap.info);
        TMALLOC(rout->cmap.buffer, size);
        memcpy(rout->cmap.buffer, rin->cmap.buffer, size * sizeof(LPIXEL));
      }
    if (rin->cmap.penbuffer) {
      size = TCM_CMAP_PENBUFFER_SIZE(rout->cmap.info);
      TMALLOC(rout->cmap.penbuffer, size);
      memcpy(rout->cmap.penbuffer, rin->cmap.penbuffer, size * sizeof(LPIXEL));

      size = TCM_CMAP_COLBUFFER_SIZE(rout->cmap.info);
      TMALLOC(rout->cmap.colbuffer, size);
      memcpy(rout->cmap.colbuffer, rin->cmap.colbuffer, size * sizeof(LPIXEL));
      E 91
    }
  }
  E 90
E 85
}

I 87
    /*-----------------------------------------------------------------*/

    void
    convert_raster(RASTER * r, RAS_TYPE type) {
  RASTER raux;

  if (r->type == type) return;

  D 89 create_raster(&raux, r->lx, r->ly, type);
  E 89
I 89 create_raster_with_extra(&raux, r->lx, r->ly, type, r->extra_mask);
  E 89 rop_copy(r, &raux, 0, 0, r->lx - 1, r->ly - 1, 0, 0);
  release_raster(r);
  *r = raux;
}
I 98

    /*-----------------------------------------------------------------*/

    static UCHAR Reverse_lut[256];

static void init_reverse_lut(void) {
  UCHAR meta_lut[] = {0x0, 0x8, 0x4, 0xc, 0x2, 0xa, 0x6, 0xe,
                      0x1, 0x9, 0x5, 0xd, 0x3, 0xb, 0x7, 0xf};
  static int i = 0;

  if (i == 256) return;

  for (i           = 0; i < 256; i++)
    Reverse_lut[i] = (meta_lut[i & 0xf] << 4) | (meta_lut[i >> 4]);
}

/*------------------------------------------------------------------------*/

I 105 static void rop_mirror_v_cm16_rgbm(RASTER * rin, RASTER * rout) {
  LPIXEL *rowout, *pixout, *cmap;
  USHORT *rowin, *pixin;
  int wrapin, wrapout;
  int x, lx, ly;

  cmap    = rin->cmap.buffer - rin->cmap.info.offset_mask;
  lx      = rin->lx;
  ly      = rin->ly;
  wrapin  = rin->wrap;
  wrapout = rout->wrap;

  rowin  = (USHORT *)rin->buffer;
  rowout = (LPIXEL *)rout->buffer + wrapout * (ly - 1);

  while (ly-- > 0) {
    pixin  = rowin;
    pixout = rowout;
    for (x = 0; x < lx; x++) *pixout++ = cmap[*pixin++];
    rowin += wrapin;
    rowout -= wrapout;
  }
}

/*------------------------------------------------------------------------*/

static void rop_mirror_v_cm24_rgbm(RASTER * rin, RASTER * rout) {
  LPIXEL *rowout, *pixout, valout, *penmap, *colmap;
  ULONG *rowin, *pixin, valin;
  UCHAR *exrow, *expix;
  int wrapin, wrapout;
  int x, lx, ly, lines;

  penmap  = rin->cmap.penbuffer;
  colmap  = rin->cmap.colbuffer;
  lx      = rin->lx;
  ly      = rin->ly;
  lines   = ly;
  wrapin  = rin->wrap;
  wrapout = rout->wrap;

  rowin  = (ULONG *)rin->buffer;
  rowout = (LPIXEL *)rout->buffer + wrapout * (ly - 1);

  while (lines-- > 0) {
    pixin  = rowin;
    pixout = rowout;
    for (x = 0; x < lx; x++) {
      valin = *pixin++;
      MAP24(valin, penmap, colmap, valout)
      *pixout++ = valout;
    }
    rowin += wrapin;
    rowout -= wrapout;
  }
}

/*------------------------------------------------------------------------*/

E 105 void rop_mirror(RASTER * rin, RASTER * rout, TBOOL is_vertical) {
  UCHAR *buffer1, *buffer2, *auxbuf;
  int bpp, lx, ly, wrapin, wrapout, scanline_in, scanline_out, lx_size;
  int i, j;
  lx      = rin->lx;
  ly      = rin->ly;
  wrapin  = rin->wrap;
  wrapout = rout->wrap;

  I 105 if (rin->type == RAS_CM16 && rout->type == RAS_RGBM && is_vertical) {
    rop_mirror_v_cm16_rgbm(rin, rout);
    return;
  }
  if (rin->type == RAS_CM24 && rout->type == RAS_RGBM && is_vertical) {
    rop_mirror_v_cm24_rgbm(rin, rout);
    return;
  }

  E 105 assert(rin->type == rout->type);

  bpp          = rop_pixbytes(rin->type);
  scanline_in  = wrapin * bpp;
  scanline_out = wrapout * bpp;
  lx_size      = lx * bpp;

  D 100 if (rin->type == RAS_BW)
E 100
I 100 if ((rin->type == RAS_BW) || (rin->type == RAS_WB))
E 100 {
    lx_size      = (lx_size + 7) / 8;
    scanline_in  = (scanline_in + 7) / 8;
    scanline_out = (scanline_out + 7) / 8;
    if (!is_vertical) init_reverse_lut();
  }

  buffer1 = rin->buffer;

  if (is_vertical)
		D 105 {
      E 105
I 105 {
        E 105 buffer2 = (UCHAR *)rout->buffer + (ly - 1) * scanline_out;
        TMALLOC(auxbuf, lx_size)
        for (i = 0; i < ly / 2; i++) {
          memcpy(auxbuf, buffer1, lx_size);
          memcpy(buffer1, buffer2, lx_size);
          memcpy(buffer2, auxbuf, lx_size);
          buffer1 += scanline_in;
          buffer2 -= scanline_out;
        }
      }
      else {
        buffer2 = (UCHAR *)rout->buffer + lx_size;
        TMALLOC(auxbuf, bpp)
        for (i = 0; i < ly; i++) {
          for (j = 0; j < lx_size / 2; j += bpp) {
            memcpy(auxbuf, buffer1 + j, bpp);
            memcpy(buffer1 + j, buffer1 + lx_size - j - bpp, bpp);
            memcpy(buffer2 - j - bpp, auxbuf, bpp);
            if (rin->type == RAS_BW) {
              buffer1[lx_size - j - bpp] =
                  Reverse_lut[buffer1[lx_size - j - bpp]];
              buffer1[j] = Reverse_lut[buffer1[j]];
            }
            I 102 else if (rin->type == RAS_WB) {
              buffer1[lx_size - j - bpp] =
                  ~Reverse_lut[buffer1[lx_size - j - bpp]];
              buffer1[j] = ~Reverse_lut[buffer1[j]];
            }
            E 102
          }
          buffer1 += scanline_in;
          buffer2 += scanline_out;
        }
      }
      free(auxbuf);
    }
  E 98
E 87
E 84
