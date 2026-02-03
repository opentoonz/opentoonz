

#include "tspectrum.h"
//#include "tutil.h"
//#include "tpixelutils.h"

//-------------------------------------------------------------------

DVAPI TSpectrumT<TPixel64> convert(const TSpectrumT<TPixel32> &s) {
  std::vector<TSpectrum64::ColorKey> keys;
  for (int i = 0; i < s.getKeyCount(); i++) {
    TSpectrumT<TPixel32>::Key key = s.getKey(i);
    TSpectrum64::ColorKey key64(key.first, toPixel64(key.second));
    keys.push_back(key64);
  }
  return TSpectrumT<TPixel64>(keys.size(), &keys[0]);
}
