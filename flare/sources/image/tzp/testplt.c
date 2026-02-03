

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

  if (argc > 2)
    strcpy(outname, argv[2]);
  else
    strcpy(outname, argv[1]);

  printf(">> Loading %s\n", inname);

  img = img_read(inname);
  if (!img) {
    printf("file %s not found\n", inname);
    exit(0);
  }

  printf(">> Writing %s\n", outname);
  if (!img_write_plt(outname, img)) {
    printf("non sono stato in grado di scrivere: %s\n", outname);
    exit(0);
  }

  newimg = img_read_plt(outname);
  if (!newimg) {
    printf("file %s not found\n", outname);
    exit(0);
  }

  boh = "MAMMA.cmap";
  printf(">> Writing %s\n", boh);

  img_write_ciak(boh, newimg);
  printf(">> Riprovami e non te ne pentirai!! <<\n");
}
