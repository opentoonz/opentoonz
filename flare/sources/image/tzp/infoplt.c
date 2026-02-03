

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gl/gl.h>
#include <gl/device.h>

#define _TOONZ_PROTOTYPES_
#include "toonz.h"
#include "file.h"
#include "tim.h"
#include "version.h"
#include "security.h"
#include "history.h"

void main(int argc, char *argv[]) {
  char outname[512], inname[512], *boh;
  int len, i, j, total;
  IMAGE *img = NIL, *newimg = 0;
  struct cmap_color *cmap;
  struct gl_color *gl_buffer;

  toonz_init(DUMMY_KEY_SLOT, (int *)&argc, argv);
  InibisciDongle();
  unprotect_lib();

  if (argc < 2) {
    printf("### %s error: missing argument\n", argv[0]);
    printf(" usage: %s infile \n", argv[0]);
    exit(0);
  }

  if (*argv[1] == '-') {
    printf("bad filename <%s> \n", argv[1]);
    exit(0);
  }

  printf("\n\n");

  for (i = 1; i < argc; i++) {
    strcpy(inname, argv[i]);

    len = strlen(inname);
    if (len < 4 || STR_NE(inname + len - 4, ".plt")) {
      printf("### %s error: file %s is not plt\n", argv[0], inname);
      continue;
    }

    /*   printf(">> Loading %s\n", inname); */

    img = img_read_plt_info(inname);
    if (!img) {
      printf("### %s error: file %s not found\n", argv[0], inname);
      continue;
    }

    printf(" > IMAGE: %s \n\n", inname);
    printf("   > Dimension:    xsize=%d\t\tysize=%d\n", img->pixmap.xsize,
           img->pixmap.ysize);
    printf("   > Savebox:\n");
    printf("   >   Start       x0=%d\t\ty0=%d \n", img->pixmap.xD,
           img->pixmap.yD);
    printf("   >   Dimensions  xsize=%d\t\tysize=%d \n", img->pixmap.xSBsize,
           img->pixmap.ySBsize);
    printf("   > Resolution:   x_dpi=%g\ty_dpi=%g \n", img->pixmap.x_dpi,
           img->pixmap.y_dpi);

    printf("\n");
    if (img->history) print_history(img->history);

    printf("\n\n");
    free_img(img);
    img = NIL;
  }

  printf(" Bye!!\n");
}

/* ---------------------- */
