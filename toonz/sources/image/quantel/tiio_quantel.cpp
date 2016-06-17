

#include "tiio_quantel.h"

//#include "texception.h"
#include "tfilepath.h"
#include "filequantel.h"

/*

//------------------------------------------------------------
namespace {
  int getTypeFromExt(const std::string &ext)
    {
    if (ext == "qnt")
      return QNT_FORMAT;
    if (ext == "qtl")
      return QTL_FORMAT;
    if (ext == "yuv")
      return YUV_FORMAT;
    if (ext == "sdl")
      return SDL_FORMAT;
    if (ext == "vpb")
      return VPB_FORMAT;
    return QNT_FORMAT;
    }
};  //namespace

//------------------------------------------------------------

TImageP TImageReaderQuantel::load()
{
int w = 0, h = 0;

int type = getTypeFromExt(getFilePath().getType());

void *buffer = img_read_quantel(getFilePath().getWideString().c_str(), &w, &h,
type);
if (buffer)
  {
  TRaster32P raster(w, h, w, (TPixel32*)buffer);
  TRasterImageP rasImage(raster);
  return TImageP(rasImage);
  }
throw TImageException(getFilePath(), "unable to load file");
}

//------------------------------------------------------------

TDimension TImageReaderQuantel::getSize() const
{
int w, h;

img_read_quantel_info(getFilePath().getWideString().c_str(), &w, &h,
getTypeFromExt(getFilePath().getType()));
return TDimension(w, h);
}

//------------------------------------------------------------

TRect TImageReaderQuantel::getBBox() const
{
int w, h;
img_read_quantel_info(getFilePath().getWideString().c_str(), &w, &h,
getTypeFromExt(getFilePath().getType()));
return TRect(0,0,w-1, h-1);
}

//============================================================

void TImageWriterQuantel::save(const TImageP &image)
{
TRasterImageP rasImage(image);
TRaster32P raster;
raster = rasImage->getRaster();
if (!raster)
  throw TException("not a rasterimage!");

if (raster->getLx() != raster->getWrap())
  throw TException ("TImageWriterQuantel::save lx != wrap");

int type = getTypeFromExt(getFilePath().getType());

if (!img_write_quantel(getFilePath().getWideString().c_str(),
raster->getRawData(), raster->getLx(), raster->getLy(), type))
  throw TException("error writing file");
}


*/
