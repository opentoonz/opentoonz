#pragma once

#ifndef TRANDOM_INCLUDED
#define TRANDOM_INCLUDED

#include "tcommon.h"

#undef DVAPI
#undef DVVAR
#ifdef TNZCORE_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

/*! Generates a pseudorandom sequence
*/
class DVAPI TRandom {
public:
  TRandom(UINT seed = 0);
  ~TRandom();

  /*! resets the pseudorandom engine to the first number of the sequence */
  void reset();

  /*! set a new seed for the pseudorandom engine and reset the sequence */
  void setSeed(UINT seed);

  /*! returns an unsigned integer number in the range [0, end[ */
  UINT getUInt(UINT end = c_maxuint);  // [0,end[

  /*! returns an integer number in the range [begin, end[ */
  int getInt(int begin, int end);

  /*! returns a float number in the range [0, 1[ */
  float getFloat();

  /*! returns a float number in the range [0, end[ */
  float getFloat(float end);

  /*! returns a float number in the range [begin, end[ */
  float getFloat(float begin, float end);

  /*! returns a random bool value */
  bool getBool();

  /*! returns a double number in the range [0, 1[ */
  double getDouble();

private:
  UINT seed;
  int idx1, idx2;
  UINT ran[56];

  enum RANDOM_FLOAT_TYPE {
    RANDOM_FLOAT_TYPE_NONE,
    RANDOM_FLOAT_TYPE_1,
    RANDOM_FLOAT_TYPE_2,
    RANDOM_FLOAT_TYPE_HOW_MANY
  };

  static RANDOM_FLOAT_TYPE RandomFloatType;

  inline UINT getNextUINT();
  inline void setRandomFloatType();
};

#endif  // TRANDOM_INCLUDED
