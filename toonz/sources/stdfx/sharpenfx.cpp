

#include "stdfx.h"
#include "tfxparam.h"

//===================================================================

template <class T>
void ropSharpen(const TRasterPT<T> &rin, TRasterPT<T> &rout,
                int sharpen_max_corr) {
  T *bufin, *east, *northeast, *southeast;
  T *bufout, *pixout;
  int lx, ly, wrapin, wrapout, x, y, count;
  int cntr_r, east_r, col_west_r, col_cntr_r, col_east_r;
  int cntr_g, east_g, col_west_g, col_cntr_g, col_east_g;
  int cntr_b, east_b, col_west_b, col_cntr_b, col_east_b;
  int cntr_m, east_m, col_west_m, col_cntr_m, col_east_m;

  int lapl, out;

#define SET_PIXOUT(X)                                                          \
  {                                                                            \
    lapl = (cntr_##X << 3) + cntr_##X -                                        \
           (col_west_##X + col_cntr_##X + col_east_##X);                       \
    if (lapl < 0) {                                                            \
      out       = cntr_##X - ((256 * 4 - lapl * sharpen_max_corr) >> (8 + 3)); \
      pixout->X = (out <= 0) ? 0 : out;                                        \
    } else {                                                                   \
      out       = cntr_##X + ((256 * 4 + lapl * sharpen_max_corr) >> (8 + 3)); \
      pixout->X = (out >= maxChanVal) ? maxChanVal : out;                      \
    }                                                                          \
  }
  rin->lock();
  rout->lock();
  bufin          = (T *)rin->getRawData();
  bufout         = (T *)rout->getRawData();
  lx             = std::min(rin->getLx(), rout->getLx());
  ly             = std::min(rin->getLy(), rout->getLy());
  wrapin         = rin->getWrap();
  wrapout        = rout->getWrap();
  int maxChanVal = T::maxChannelValue;

  if (lx <= 1 || ly <= 1) {
    for (y = 0; y < ly; y++)
      for (x = 0; x < lx; x++) bufout[x + y * wrapout] = bufin[x + y * wrapin];
    return;
  }
  east       = bufin;
  northeast  = east + wrapin;
  east_r     = east->r;
  east_g     = east->g;
  east_b     = east->b;
  east_m     = east->m;
  col_east_r = 2 * east_r + northeast->r;
  col_east_g = 2 * east_g + northeast->g;
  col_east_b = 2 * east_b + northeast->b;
  col_east_m = 2 * east_m + northeast->m;
  col_cntr_r = col_east_r;
  col_cntr_g = col_east_g;
  col_cntr_b = col_east_b;
  col_cntr_m = col_east_m;
  east++;
  northeast++;
  pixout = bufout;
  for (count = lx - 1; count > 0; count--, east++, northeast++, pixout++) {
    cntr_r     = east_r;
    east_r     = east->r;
    col_west_r = col_cntr_r;
    col_cntr_r = col_east_r;
    col_east_r = 2 * east_r + northeast->r;
    SET_PIXOUT(r)
    cntr_g     = east_g;
    east_g     = east->g;
    col_west_g = col_cntr_g;
    col_cntr_g = col_east_g;
    col_east_g = 2 * east_g + northeast->g;
    SET_PIXOUT(g)
    cntr_b     = east_b;
    east_b     = east->b;
    col_west_b = col_cntr_b;
    col_cntr_b = col_east_b;
    col_east_b = 2 * east_b + northeast->b;
    SET_PIXOUT(b)
    cntr_m     = east_m;
    east_m     = east->m;
    col_west_m = col_cntr_m;
    col_cntr_m = col_east_m;
    col_east_m = 2 * east_m + northeast->m;
    SET_PIXOUT(m)
  }
  cntr_r     = east_r;
  col_west_r = col_cntr_r;
  col_cntr_r = col_east_r;
  SET_PIXOUT(r)
  cntr_g     = east_g;
  col_west_g = col_cntr_g;
  col_cntr_g = col_east_g;
  SET_PIXOUT(g)
  cntr_b     = east_b;
  col_west_b = col_cntr_b;
  col_cntr_b = col_east_b;
  SET_PIXOUT(b)
  cntr_m     = east_m;
  col_west_m = col_cntr_m;
  col_cntr_m = col_east_m;
  SET_PIXOUT(m)
  for (y = 1; y < ly - 1; y++) {
    east       = bufin + y * wrapin;
    northeast  = east + wrapin;
    southeast  = east - wrapin;
    east_r     = east->r;
    east_g     = east->g;
    east_b     = east->b;
    east_m     = east->m;
    col_east_r = east_r + northeast->r + southeast->r;
    col_east_g = east_g + northeast->g + southeast->g;
    col_east_b = east_b + northeast->b + southeast->b;
    col_east_m = east_m + northeast->m + southeast->m;
    col_cntr_r = col_east_r;
    col_cntr_g = col_east_g;
    col_cntr_b = col_east_b;
    col_cntr_m = col_east_m;
    east++;
    northeast++;
    southeast++;
    pixout = bufout + y * wrapout;
    for (count = lx - 1; count > 0;
         count--, east++, northeast++, southeast++, pixout++) {
      cntr_r     = east_r;
      east_r     = east->r;
      col_west_r = col_cntr_r;
      col_cntr_r = col_east_r;
      col_east_r = east_r + northeast->r + southeast->r;
      SET_PIXOUT(r)
      cntr_g     = east_g;
      east_g     = east->g;
      col_west_g = col_cntr_g;
      col_cntr_g = col_east_g;
      col_east_g = east_g + northeast->g + southeast->g;
      SET_PIXOUT(g)
      cntr_b     = east_b;
      east_b     = east->b;
      col_west_b = col_cntr_b;
      col_cntr_b = col_east_b;
      col_east_b = east_b + northeast->b + southeast->b;
      SET_PIXOUT(b)
      cntr_m     = east_m;
      east_m     = east->m;
      col_west_m = col_cntr_m;
      col_cntr_m = col_east_m;
      col_east_m = east_m + northeast->m + southeast->m;
      SET_PIXOUT(m)
    }
    cntr_r     = east_r;
    col_west_r = col_cntr_r;
    col_cntr_r = col_east_r;
    SET_PIXOUT(r)
    cntr_g     = east_g;
    col_west_g = col_cntr_g;
    col_cntr_g = col_east_g;
    SET_PIXOUT(g)
    cntr_b     = east_b;
    col_west_b = col_cntr_b;
    col_cntr_b = col_east_b;
    SET_PIXOUT(b)
    cntr_m     = east_m;
    col_west_m = col_cntr_m;
    col_cntr_m = col_east_m;
    SET_PIXOUT(m)
  }
  east       = bufin + y * wrapin;
  southeast  = east - wrapin;
  east_r     = east->r;
  east_g     = east->g;
  east_b     = east->b;
  east_m     = east->m;
  col_east_r = 2 * east_r + southeast->r;
  col_east_g = 2 * east_g + southeast->g;
  col_east_b = 2 * east_b + southeast->b;
  col_east_m = 2 * east_m + southeast->m;
  col_cntr_r = col_east_r;
  col_cntr_g = col_east_g;
  col_cntr_b = col_east_b;
  col_cntr_m = col_east_m;
  east++;
  southeast++;
  pixout = bufout + y * wrapout;
  for (count = lx - 1; count > 0; count--, east++, southeast++, pixout++) {
    cntr_r     = east_r;
    east_r     = east->r;
    col_west_r = col_cntr_r;
    col_cntr_r = col_east_r;
    col_east_r = 2 * east_r + southeast->r;
    SET_PIXOUT(r)
    cntr_g     = east_g;
    east_g     = east->g;
    col_west_g = col_cntr_g;
    col_cntr_g = col_east_g;
    col_east_g = 2 * east_g + southeast->g;
    SET_PIXOUT(g)
    cntr_b     = east_b;
    east_b     = east->b;
    col_west_b = col_cntr_b;
    col_cntr_b = col_east_b;
    col_east_b = 2 * east_b + southeast->b;
    SET_PIXOUT(b)
    cntr_m     = east_m;
    east_m     = east->m;
    col_west_m = col_cntr_m;
    col_cntr_m = col_east_m;
    col_east_m = 2 * east_m + southeast->m;
    SET_PIXOUT(m)
  }
  cntr_r     = east_r;
  col_west_r = col_cntr_r;
  col_cntr_r = col_east_r;
  SET_PIXOUT(r)
  cntr_g     = east_g;
  col_west_g = col_cntr_g;
  col_cntr_g = col_east_g;
  SET_PIXOUT(g)
  cntr_b     = east_b;
  col_west_b = col_cntr_b;
  col_cntr_b = col_east_b;
  SET_PIXOUT(b)
  cntr_m     = east_m;
  col_west_m = col_cntr_m;
  col_cntr_m = col_east_m;
  SET_PIXOUT(m)

  rin->unlock();
  rout->unlock();
}

/*---------------------------------------------------------------------------*/

class SharpenFx final : public TStandardRasterFx {
  FX_PLUGIN_DECLARATION(SharpenFx)

  TRasterFxPort m_input;
  TDoubleParamP m_intensity;

public:
  SharpenFx()
      : m_intensity(50)

  {
    bindParam(this, "intensity", m_intensity);
    addInputPort("Source", m_input);
    m_intensity->setValueRange(0.0, 999999, 1);
  }

  ~SharpenFx(){};

  bool doGetBBox(double frame, TRectD &bBox,
                 const TRenderSettings &info) override {
    if (m_input.isConnected()) {
      bool ret = m_input->doGetBBox(frame, bBox, info);
      return ret;
    } else {
      bBox = TRectD();
      return false;
    }
  }

  void doCompute(TTile &tile, double frame, const TRenderSettings &ri) override;

  bool canHandle(const TRenderSettings &info, double frame) override {
    return true;
  }
};

//-------------------------------------------------------------------

void SharpenFx::doCompute(TTile &tile, double frame,
                          const TRenderSettings &ri) {
  if (!m_input.isConnected()) return;

  int intensity = troundp(m_intensity->getValue(frame));

  TRasterP srcRas = tile.getRaster()->create(tile.getRaster()->getLx(),
                                             tile.getRaster()->getLy());
  // TRaster32P srcRas(tile.getRaster()->getLx() + border*2,
  // tile.getRaster()->getLy() + border*2);
  TTile srcTile(srcRas, tile.m_pos);

  m_input->compute(srcTile, frame, ri);
  TRaster32P raster32    = tile.getRaster();
  TRaster32P srcraster32 = srcTile.getRaster();

  if (raster32) ropSharpen<TPixel32>(srcraster32, raster32, intensity);
  // doEmboss<TPixel32, TPixelGR8, UCHAR>(raster32, srcraster32, azimuth,
  // elevation, intensity, border);
  else {
    TRaster64P raster64    = tile.getRaster();
    TRaster64P srcraster64 = srcTile.getRaster();
    if (raster64)
      ropSharpen<TPixel64>(srcraster64, raster64, intensity);
    else
      throw TException("sharpen: unsupported Pixel Type");
  }
}

FX_PLUGIN_IDENTIFIER(SharpenFx, "sharpenFx");
