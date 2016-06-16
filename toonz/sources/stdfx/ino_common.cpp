#include "tenv.h"
#include "tsystem.h"

#include "ino_common.h"

/* copy and paste from
 igs_ifx_common.h */
namespace igs {
namespace image {
namespace rgba {
enum num { blu = 0, gre, red, alp, siz };
}
}
}
//------------------------------------------------------------
namespace {
// T is TPixel32 or TPixel64
// U is unsigned char or unsigned short
template <class T, class U>
void ras_to_arr_(const TRasterPT<T> ras, U *arr, const int channels) {
  using namespace igs::image::rgba;

  for (int yy = 0; yy < ras->getLy(); ++yy) {
    const T *ras_sl = ras->pixels(yy);
    for (int xx = 0; xx < ras->getLx(); ++xx, arr += channels) {
      if (red < channels) {
        arr[red] = ras_sl[xx].r;
      }
      if (gre < channels) {
        arr[gre] = ras_sl[xx].g;
      }
      if (blu < channels) {
        arr[blu] = ras_sl[xx].b;
      }
      if (alp < channels) {
        arr[alp] = ras_sl[xx].m;
      }
    }
  }
}
template <class U, class T>
void arr_to_ras_(const U *arr, const int channels, TRasterPT<T> ras,
                 const int margin  // default is 0
                 ) {
  arr +=
      (ras->getLx() + margin + margin) * margin * channels + margin * channels;

  using namespace igs::image::rgba;

  for (int yy = 0; yy < ras->getLy();
       ++yy, arr += (ras->getLx() + margin + margin) * channels) {
    const U *arrx = arr;
    T *ras_sl     = ras->pixels(yy);
    for (int xx = 0; xx < ras->getLx(); ++xx, arrx += channels) {
      if (red < channels) {
        ras_sl[xx].r = arrx[red];
      }
      if (gre < channels) {
        ras_sl[xx].g = arrx[gre];
      }
      if (blu < channels) {
        ras_sl[xx].b = arrx[blu];
      }
      if (alp < channels) {
        ras_sl[xx].m = arrx[alp];
      }
    }
  }
}
}
//--------------------
void ino::ras_to_arr(const TRasterP in_ras, const int channels,
                     unsigned char *out_arr) {
  if ((TRaster32P)in_ras) {
    ras_to_arr_<TPixel32, unsigned char>(in_ras, out_arr, channels);
  } else if ((TRaster64P)in_ras) {
    ras_to_arr_<TPixel64, unsigned short>(
        in_ras, reinterpret_cast<unsigned short *>(out_arr), channels);
  }
}
void ino::arr_to_ras(const unsigned char *in_arr, const int channels,
                     TRasterP out_ras, const int margin) {
  if ((TRaster32P)out_ras) {
    arr_to_ras_<unsigned char, TPixel32>(in_arr, channels, out_ras, margin);
  } else if ((TRaster64P)out_ras) {
    arr_to_ras_<unsigned short, TPixel64>(
        reinterpret_cast<const unsigned short *>(in_arr), channels, out_ras,
        margin);
  }
}
//--------------------
void ino::ras_to_vec(const TRasterP in_ras, const int channels,
                     std::vector<unsigned char> &out_vec) {
  out_vec.resize(
      in_ras->getLy() * in_ras->getLx() * channels *
      (((TRaster64P)in_ras) ? sizeof(unsigned short) : sizeof(unsigned char)));
  ino::ras_to_arr(in_ras, channels, &out_vec.at(0));
}
void ino::vec_to_ras(std::vector<unsigned char> &in_vec, const int channels,
                     TRasterP out_ras, const int margin  // default is 0
                     ) {
  ino::arr_to_ras(&in_vec.at(0), channels, out_ras, margin);
  in_vec.clear();
}
//--------------------
#if 0   //---
void ino::Lx_to_wrap( TRasterP ras ) {
	/*
	ras->getLx()   : 描画の幅
	ras->getWrap() : データの存在幅
	描画幅よりデータの存在幅の方が大きい場合、
	存在幅位置に置き直し、残りをゼロクリア
	*/
	if ( ras->getWrap() <= ras->getLx() ) { return; }

	const int rowSize  = ras->getLx()   * ras->getPixelSize();
	const int wrapSize = ras->getWrap() * ras->getPixelSize();
	const int restSize = wrapSize - rowSize;
	const UCHAR *rowImg  = ras->getRawData()+rowSize *(ras->getLy()-1);
	      UCHAR *wrapImg = ras->getRawData()+wrapSize*(ras->getLy()-1);
	for (int yy = 0; yy < ras->getLy(); ++yy) {
		::memcpy(wrapImg, rowImg, rowSize);
		::memset(wrapImg+rowSize, 0, restSize); /* 上下にはみ出すとここで落ちる */
		rowImg  -= rowSize;
		wrapImg -= wrapSize;
	}
}
#endif  //---

//------------------------------------------------------------
namespace {
bool enable_sw_ = true;
bool check_sw_  = true;
}
bool ino::log_enable_sw(void) {
  if (check_sw_) {
    TFileStatus file(
        // ToonzFolder::getProfileFolder()
        TEnv::getConfigDir() + "fx_ino_no_log.setup");
    if (file.doesExist()) {
      enable_sw_ = false;
    }

    check_sw_ = false;
  }
  return enable_sw_;
}
