

#include "trandom.h"

TRandom::RANDOM_FLOAT_TYPE TRandom::RandomFloatType =
    TRandom::RANDOM_FLOAT_TYPE_NONE;

TRandom::TRandom(UINT _seed) : seed(_seed) {
  if (RandomFloatType == TRandom::RANDOM_FLOAT_TYPE_NONE) setRandomFloatType();
  reset();
}

//--------------------------------------------------------------------------

TRandom::~TRandom() {}

//--------------------------------------------------------------------------

void TRandom::setSeed(UINT s) {
  seed = s;
  reset();
}
//--------------------------------------------------------------------------

void TRandom::reset() {
  UINT uj, uk;
  int i, ii, k;

  assert(sizeof(UINT) == 32 / 8);

  uj      = 161803398 - seed;
  ran[55] = uj;
  uk      = 1;
  for (i = 1; i <= 54; i++) {
    ii      = (21 * i) % 55;
    ran[ii] = uk;
    uk      = uj - uk;
    uj      = ran[ii];
  }
  for (k = 1; k <= 4; k++)
    for (i = 1; i <= 55; i++) ran[i] -= ran[1 + (i + 30) % 55];
  idx1     = 55;
  idx2     = 31;
}

//--------------------------------------------------------------------------

inline void TRandom::setRandomFloatType() {
  UINT u;

  assert(sizeof(float) == sizeof(UINT));
  u = 0x3f800000;
  if ((*(float *)&u) == 1.0F)
    RandomFloatType = RANDOM_FLOAT_TYPE_1;
  else {
    u = 0x0000803f;
    if ((*(float *)&u) == 1.0F)
      RandomFloatType = RANDOM_FLOAT_TYPE_2;
    else
      assert(0);
  }
}

//--------------------------------------------------------------------------

inline UINT TRandom::getNextUINT() {
  idx1++;
  if (idx1 == 56) idx1 = 1;
  idx2++;
  if (idx2 == 56) idx2 = 1;
  ran[idx1]            = ran[idx1] - ran[idx2];
  return ran[idx1];
}

//--------------------------------------------------------------------------

UINT TRandom::getUInt(UINT end)  // [0,end[
{
  if (end == 0) return 0;
  UINT u = getNextUINT();
  if (end == c_maxuint) return u;

  return u % end;
}

//--------------------------------------------------------------------------

int TRandom::getInt(int begin, int end)  // [begin, end[
{
  assert(end >= begin);
  int limit = end - begin;
  if (limit == 0)  // end == begin
    return begin;
  UINT u = getNextUINT();
  return u % limit + begin;
}

//--------------------------------------------------------------------------

float TRandom::getFloat()  // [0,1[
{
  UINT u = getNextUINT();

  switch (RandomFloatType) {
  case RANDOM_FLOAT_TYPE_1:
    u = ((u >> 5) & 0x007fffff) | 0x3f800000;
    break;
  case RANDOM_FLOAT_TYPE_2:
    u = (u & 0xffff7f00) | 0x0000803f;
    break;
  default:
    assert(0);
    u = 0;
    break;
  }
  return (*(float *)&u) - 1.0F;
}

//--------------------------------------------------------------------------

float TRandom::getFloat(float end)  // [0, end[
{
  return getFloat() * end;
}

//--------------------------------------------------------------------------

float TRandom::getFloat(float begin, float end)  // [begin, end[
{
  assert(end >= begin);
  return (getFloat() * (end - begin)) + begin;
}

//--------------------------------------------------------------------------

bool TRandom::getBool() {
  UINT u = getNextUINT();
  return u & 1;
}

//--------------------------------------------------------------------------

double TRandom::getDouble()  // [0,1[
{
  return getFloat();
#ifdef DA_RIVEDERE_O_PROPRIO_IMPLEMENTARE
  UINT low     = getNextUINT();
  UINT high    = getNextUINT();
  double value = 0;

  void *ptr  = &value;
  UINT *retL = (UINT *)ptr;
  UINT *retH = retL + 1;

/*
// questa parte e' stata commentata perche' su tutte le piattaforme il tipo
//di float e' RANDOM_FLOAT_TYPE_1ma su irix c'e' bisogno di eseguire le
//istruzioni sotto RANDOM_FLOAT_TYPE_2
switch (RandomFloatType)
  {
  case RANDOM_FLOAT_TYPE_1:
                             *retH = high;
                             *retH &= 0x00efffff;
                             *retH |= 0x3f000000;

                             *retL = low ;
// vecchi commenti
//                             *retH = high;
//                             *retL = (low >> 5) & 0x007fffff | 0x3ff00000;

                             return value;
                             break;
   case RANDOM_FLOAT_TYPE_2: *retH = low;
                             *retL = (high >> 5) & 0x007fffff | 0x3ff00000;
                             break;
                                                                                //!!!!!occhio andrebbe sopra il break
                             return value-1.0;
  default: assert (0);
  }
return -1;
*/
#ifndef __sgi
  *retH = high;
  *retH &= 0x007fffff;
  *retH |= 0x3ff00000;
  *retL = low;
  return value - 1.0;
#else
  *retH = low;
  *retL = (high >> 5) & 0x007fffff | 0x3ff00000;
  return value - 1.0;
#endif

#endif
}
