#ifndef FIXED_INCLUDED
#define FIXED_INCLUDED

#define fixed_1 0x00010000L

// fixed 2.0
#define fixed2 0x00020000L
// fixed 0.5
#define fixedHalf 0x00008000L
#define infinity 0x7FFFFFFFL
#define negInfinity 0x80000000L
#define fixedStdErr 0x0000003FL
// fixed sqrt(2)
#define fixedSqrt2 0x00016A0AL

#define FixedRound(a) ((S16)((SFIXED)(a) + 0x8000L >> 16))
#define FixedTrunc(a) ((S16)((SFIXED)(a) >> 16))

#define FixedCeiling(a) ((S16)(((SFIXED)(a) + 0x8000L) >> 16))
#define FixedFloor(a) ((S16)((SFIXED)(a) >> 16))

#define FixedToInt(a) ((S16)((SFIXED)(a) + 0x8000L >> 16))
#define IntToFixed(a) ((SFIXED)(a) << 16)
// Fixed integer constant
#define FC(a) IntToFixed(a)

#define FixedToFloat(a) ((float)(a) / fixed_1)
#define FloatToFixed(a) ((SFIXED)((float)(a)*fixed_1))

#define FixedToDouble(a) ((double)(a) / fixed_1)
#define DoubleToFixed(a) ((SFIXED)((double)(a)*fixed_1))

#define FixedAverage(a, b) (((a) + (b)) >> 1)

#define FixedAbs(x) ((x) < 0 ? -(x) : (x))
#define FixedMin(a, b) ((a) < (b) ? (a) : (b))
#define FixedMax(a, b) ((a) > (b) ? (a) : (b))
#define FixedEqual(a, b, err) (FixedAbs((a) - (b)) <= err)

SFIXED FixedNearestMultiple(SFIXED x, SFIXED factor);

// Note that all angles are handled in Fixed point degrees to simplify rounding issues
// they are kept in the range of 0 to 360 degrees

// Generic Floating point routines for quick porting not fast enough for shipping code
SFIXED FixedMul(SFIXED, SFIXED);
SFIXED FixedDiv(SFIXED, SFIXED);
SFIXED FixedSin(SFIXED);
SFIXED FixedCos(SFIXED);
SFIXED FixedTan(SFIXED);
SFIXED FixedAtan2(SFIXED dy, SFIXED dx);

int _FPMul(int a, int b, int shift);
int _FPDiv(int a, int b, int rshift);

#endif
