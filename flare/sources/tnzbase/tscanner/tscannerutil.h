#pragma once

#include <traster.h>

namespace TScannerUtil {

/* copia un rettangolo da rin a rout,
 * specchiandolo orizzontalmente se mirror e' dispari,
 * e poi ruotandolo del multiplo di novanta gradi specificato
 * da ninety in senso antiorario
 *
 */

void copyRGBBufferToTRaster32(unsigned char *rgbBuffer, int rgbLx, int rgbLy,
                              const TRaster32P &rout, bool internal);

void copyRGBBufferToTRasterGR8(unsigned char *rgbBuffer, int rgbLx, int rgbLy,
                               int rgbWrap, const TRasterGR8P &rout);

void copyGR8BufferToTRasterGR8(unsigned char *gr8Buffer, int rgbLx, int rgbLy,
                               const TRasterGR8P &rout, bool internal);

void copyGR8BufferToTRasterBW(unsigned char *gr8Buffer, int rgbLx, int rgbLy,
                              const TRasterGR8P &rout, bool internal,
                              float thres);

void copyBWBufferToTRasterGR8(const unsigned char *buffer, int rgbLx, int rgbLy,
                              const TRasterGR8P &rout, bool isBW,
                              bool internal);

void copy90BWBufferToRasGR8(unsigned char *bwBuffer, int bwLx, int bwLy,
                            int bwWrap, bool isBW, TRasterGR8P &rout,
                            int mirror, int ninety);
};
