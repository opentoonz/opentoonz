

#include "tpixel.h"
#include "tpixelgr.h"

const int TPixelRGBM32::maxChannelValue = 0xff;
const int TPixelRGBM64::maxChannelValue = 0xffff;
const int TPixelGR8::maxChannelValue    = 0xff;
const int TPixelGR16::maxChannelValue   = 0xffff;

const TPixelRGBM32 TPixelRGBM32::Red(maxChannelValue, 0, 0);
const TPixelRGBM32 TPixelRGBM32::Green(0, maxChannelValue, 0);
const TPixelRGBM32 TPixelRGBM32::Blue(0, 0, maxChannelValue);
const TPixelRGBM32 TPixelRGBM32::Yellow(maxChannelValue, maxChannelValue, 0);
const TPixelRGBM32 TPixelRGBM32::Cyan(0, maxChannelValue, maxChannelValue);
const TPixelRGBM32 TPixelRGBM32::Magenta(maxChannelValue, 0, maxChannelValue);
const TPixelRGBM32 TPixelRGBM32::White(maxChannelValue, maxChannelValue,
                                       maxChannelValue);
const TPixelRGBM32 TPixelRGBM32::Black(0, 0, 0);
const TPixelRGBM32 TPixelRGBM32::Transparent(0, 0, 0, 0);
//---------------------------------------------------

const TPixelRGBM64 TPixelRGBM64::Red(maxChannelValue, 0, 0);
const TPixelRGBM64 TPixelRGBM64::Green(0, maxChannelValue, 0);
const TPixelRGBM64 TPixelRGBM64::Blue(0, 0, maxChannelValue);
const TPixelRGBM64 TPixelRGBM64::Yellow(maxChannelValue, maxChannelValue, 0);
const TPixelRGBM64 TPixelRGBM64::Cyan(0, maxChannelValue, maxChannelValue);
const TPixelRGBM64 TPixelRGBM64::Magenta(maxChannelValue, 0, maxChannelValue);
const TPixelRGBM64 TPixelRGBM64::White(maxChannelValue, maxChannelValue,
                                       maxChannelValue);
const TPixelRGBM64 TPixelRGBM64::Black(0, 0, 0);
const TPixelRGBM64 TPixelRGBM64::Transparent(0, 0, 0, 0);
//---------------------------------------------------
const TPixelD TPixelD::Red(1, 0, 0);
const TPixelD TPixelD::Green(0, 1, 0);
const TPixelD TPixelD::Blue(0, 0, 1);
const TPixelD TPixelD::Yellow(1, 1, 0);
const TPixelD TPixelD::Cyan(0, 1, 1);
const TPixelD TPixelD::Magenta(1, 0, 1);
const TPixelD TPixelD::White(1, 1, 1);
const TPixelD TPixelD::Black(0, 0, 0);
const TPixelD TPixelD::Transparent(0, 0, 0, 0);
//---------------------------------------------------
const TPixelGR8 TPixelGR8::White(maxChannelValue);
const TPixelGR8 TPixelGR8::Black(0);

const TPixelGR16 TPixelGR16::White(maxChannelValue);
const TPixelGR16 TPixelGR16::Black(0);

static std::ostream &operator<<(std::ostream &out, const TPixel32 &pixel) {
  return out << "PixRGBM32(" << (int)pixel.r << ", " << (int)pixel.g << ", "
             << (int)pixel.b << ", " << (int)pixel.m << ")";
}

static std::ostream &operator<<(std::ostream &out, const TPixel64 &pixel) {
  return out << "PixRGBM64(" << pixel.r << ", " << pixel.g << ", " << pixel.b
             << ", " << pixel.m << ")";
}

static std::ostream &operator<<(std::ostream &out, const TPixelD &pixel) {
  return out << "PixD(" << pixel.r << ", " << pixel.g << ", " << pixel.b << ", "
             << pixel.m << ")";
}

//=============================================================================

/*
TPixel32 DVAPI TPixel32::from(const TPixelGR8 &pix)
{
   return TPixel32(pix.value, pix.value, pix.value, maxChannelValue);
}

//-----------------------------------------------------------------------------

TPixel32 DVAPI TPixel32::from(const TPixelGR16 &pix)
{
   UCHAR value = byteFromUshort(pix.value);
   return TPixel32(value, value, value, maxChannelValue);
}

//-----------------------------------------------------------------------------

TPixelD DVAPI TPixelD::from(const TPixelGR8 &pix)
{
  double v = (double)pix.value * (1.0/255.0);
  return TPixelD(v,v,v,v);
}

//-----------------------------------------------------------------------------



TPixelD DVAPI TPixelD::from(const TPixelGR16 &pix)
{
  double v = (double)pix.value * (1.0/65535.0);
  return TPixelD(v,v,v,v);
}

*/
//-----------------------------------------------------------------------------

// TPixelGR8 TPixelGR8::from(const TPixelD &pix)
//{
//  return from(TPixel32::from(pix));
//}

//-----------------------------------------------------------------------------

TPixelGR8 DVAPI TPixelGR8::from(const TPixel32 &pix) {
  return TPixelGR8((((UINT)(pix.r) * 19594 + (UINT)(pix.g) * 38472 +
                     (UINT)(pix.b) * 7470 + (UINT)(1 << 15)) >>
                    16));
}

//-----------------------------------------------------------------------------
TPixelGR16 DVAPI TPixelGR16::from(const TPixel64 &pix) {
  return TPixelGR16((((UINT)(pix.r) * 19594 + (UINT)(pix.g) * 38472 +
                      (UINT)(pix.b) * 7470 + (UINT)(1 << 15)) >>
                     16));
}

//-----------------------------------------------------------------------------
