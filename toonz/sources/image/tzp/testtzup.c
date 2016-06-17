

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define _TOONZ_PROTOTYPES_
#include "toonz.h"
#include "file.h"
#include "version.h"
#include "security.h"

void main(int argc, char *argv[]) {
  char outname[512], inname[512], *boh;
  int len, i, j, total;
  IMAGE *img = 0, *newimg = 0;
  struct cmap_color *cmap;
  struct gl_color *gl_buffer;

  toonz_init(DUMMY_KEY_SLOT, (int *)&argc, argv);
  InibisciDongle();
  unprotect_lib();

  if (argc < 3) {
    printf(" %s error: missing argument\n", argv[0]);
    printf(" usage: %s infile outfile\n", argv[0]);
    exit(0);
  }

  if (*argv[1] == '-') {
    printf("bad filename <%s> \n", argv[1]);
    exit(0);
  }
  strcpy(inname, argv[1]);

  len = strlen(argv[2]);
  if (len < 4 || (STR_NE(argv[2] + len - 4, ".tzu") &&
                  STR_NE(argv[2] + len - 4, ".tzp"))) {
    printf("%s error: file %s is not tz(up)\n", argv[0], argv[2]);
    exit(0);
  }

  strcpy(outname, argv[2]);

  printf(">> Loading %s\n", inname);

  img = img_read(inname);
  if (!img) {
    printf("file %s not found\n", inname);
    exit(0);
  }

  printf(">> Immagine di dimensioni: %dx%d\n", img->pixmap.xsize,
         img->pixmap.ysize);
  printf(">Con savebox: %dx%d di dim %d,%d\n", img->pixmap.xD, img->pixmap.yD,
         img->pixmap.xSBsize, img->pixmap.ySBsize);

  printf(">> Writing %s\n", outname);
  if (!img_write_tzup(outname, img)) {
    printf("non sono stato in grado di scrivere: %s\n", outname);
    exit(0);
  }

  printf(">> Adesso che l'ho scritto lo rileggo \n");

  newimg = img_read_tzup(outname);
  if (!newimg) {
    printf("file %s not found\n", inname);
    exit(0);
  }

  printf(">> Immagine di dimensioni: %dx%d con %dx%d, %dx%d\n",
         newimg->pixmap.xsize, newimg->pixmap.ysize, newimg->pixmap.xD,
         newimg->pixmap.yD, newimg->pixmap.xSBsize, newimg->pixmap.ySBsize);
  total = newimg->pixmap.xsize * newimg->pixmap.ysize;

#ifdef QUANDO_SERVE
  gl_buffer = (struct gl_color *)calloc(total, sizeof(struct gl_color));

  for (i = 0; i < total; i++)
    if (newimg->pixmap.buffer[i] != 2063) {
      gl_buffer[i].red   = 0;
      gl_buffer[i].green = 0;
      gl_buffer[i].blue  = 0;
      gl_buffer[i].matte = 255;
    } else {
      gl_buffer[i].red   = 255;
      gl_buffer[i].green = 255;
      gl_buffer[i].blue  = 255;
      gl_buffer[i].matte = 0;
    }

  newimg->type          = RGB;
  newimg->pixmap.buffer = (unsigned short *)gl_buffer;

  /*
newimg->cmap.name = NIL;
newimg->cmap.size = 2048;
newimg->cmap.offset = 2048;
newimg->cmap.color_n  = 32;
newimg->cmap.pencil_n = 4;

newimg->cmap.buffer = (struct cmap_color*)
                malloc(sizeof(struct cmap_color)*newimg->cmap.size);

cmap = newimg->cmap.buffer;
j = 0;
for(i=0; i<newimg->cmap.size; i++)
{
cmap[i].red   = j;
cmap[i].green = j;
cmap[i].blue  = j;
j+=17;
if(j>255) j=0;
}

newimg->cmap.names = NIL;

force_to_rgb(newimg, NIL);
*/

  boh = "MAMMA.tif";
  printf(">> Writing %s\n", boh);
  if (!img_write_tzup(boh, newimg)) {
    printf("non sono stato in grado di scrivere: %s\n", boh);
    exit(0);
  }

#endif

  printf(">> Riprovami e non te ne pentirai!! <<\n");
}
