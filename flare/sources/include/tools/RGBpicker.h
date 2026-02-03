#pragma once

#ifndef RGB_PICKER_H
#define RGB_PICKER_H

#include "tpixel.h"

/* (by Daniele)

NOTE: This stuff should be moved to ToonzQt's StyleEditor class.

      It was previously in the RGBPickerTool's local namespace - but its scope
      should be wider...
*/

namespace RGBPicker {

//*************************************************************************************
//    RGB Picker open functions
//*************************************************************************************

void setCurrentColor(const TPixel32 &color);
void setCurrentColorWithUndo(const TPixel32 &color);

}  // namespace RGBPicker

#endif  // RGB_PICKER_H
