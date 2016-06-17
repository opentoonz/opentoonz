

#include "tnz4.h"

short swap_short(short s) {
  s = ((s >> 8) & 0x00ff) | (s << 8);
  return s;
}

TINT32 swap_TINT32(TINT32 s) {
  TINT32 appo, aux, aux1;

  appo = 0xff0000ff;
  aux  = (s & appo);
  aux1 = (aux >> 24) & 0x000000ff;
  aux  = (aux << 24) | aux1;
  appo = 0x00ffff00;
  aux1 = (s & appo);
  aux1 = ((aux1 >> 8) | (aux1 << 8)) & appo;
  aux  = (aux | aux1);

  return aux;
}

USHORT swap_ushort(USHORT x) { return x >> 8 | x << 8; }

/*
ULONG swap_uTINT32 (ULONG x)
{ return x >> 24 | (x & 0xff0000) >> 8 | (x & 0xff00) << 8 | x << 24; }
*/
