

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define _TOONZ_PROTOTYPES_

#include "toonz.h"
#include "tiff.h"
#include "tiffio.h"

void main(int argc, char *argv[]) {
  TIFF *tfp = NIL;

  if (argc != 2) {
    printf("tiff_dir error: missing parameters\n");
    exit(0);
  }
  Tiff_ignore_missing_internal_colormap = 1;
  Silent_tiff_print_error               = 1;
  tfp                                   = TIFFOpen(argv[1], "r");
  if (!tfp) {
    printf("unable to open %s\n", argv[1]);
    exit(0);
  }
  TIFFPrintDirectory(tfp, stderr, TRUE);
  while (TIFFReadDirectory(tfp)) TIFFPrintDirectory(tfp, stderr, TRUE);
  TIFFClose(tfp);
}
