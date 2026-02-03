//------------------------------------------------------------
#include "tfxparam.h"
#include "stdfx.h"

#include "ino_common.h"
#include "igs_color_blend.h"

//------------------------------------------------------------
// Regarding computation in linear color space mode is based on the "ComposeAdd"
// plugin fx by DWANGO Co., Ltd. The major difference from the original
// "ComposeAdd" is the "Source is premultiplied" option; the semi-transparent
// pixels are un-premultiplied before converting to linear color space. Also
// modified the transfer functions to use standard gamma correction.
//-------------

/* tnzbase --> Source Files --> tfx --> binaryFx.cppを参照 */
class ino_blend_add final : public TBlendForeBackRasterFx {
  FX_PLUGIN_DECLARATION(ino_blend_add)

public:
  ino_blend_add() : TBlendForeBackRasterFx(true) {
    // expand the opacity range
    this->m_opacity->setValueRange(0, 10.0 * ino::param_range());
  }
  ~ino_blend_add() {}

  void brendKernel(double& dnr, double& dng, double& dnb, double& dna,
                   const double upr, double upg, double upb, double upa,
                   const double up_opacity,
                   const bool alpha_rendering_sw = true,
                   const bool do_clamp           = true) override {
    igs::color::add(dnr, dng, dnb, dna, upr, upg, upb, upa, up_opacity,
                    do_clamp);
  }
};
FX_PLUGIN_IDENTIFIER(ino_blend_add, "inoAddFx");