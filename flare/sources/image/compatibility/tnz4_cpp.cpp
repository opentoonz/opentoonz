

extern "C" {

#include "tnz4.h"
#include <memory.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifndef _WIN32
#include <unistd.h>
#endif

void free_img(IMAGE *img) {
  if (!img) return;
  delete[] img->buffer;
  delete img;
}

IMAGE *new_img() {
  IMAGE *img = new IMAGE;
  memset(img, 0, sizeof(IMAGE));
  return img;
}

int allocate_pixmap(IMAGE *img, int w, int h) {
  UCHAR *buf;
  assert(img->type == TOONZRGB);
  buf          = new UCHAR[w * h * 4];
  img->buffer  = buf;
  img->xSBsize = img->xsize = w;
  img->ySBsize = img->ysize = h;
  return TRUE;
}
}
