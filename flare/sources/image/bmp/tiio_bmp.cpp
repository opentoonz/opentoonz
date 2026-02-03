

#include "texception.h"
#include "tfilepath.h"
#include "tiio_bmp.h"
#include "filebmp.h"

namespace {

string buildBMPExceptionString(int retCode) {
  switch (retCode) {
  case UNSUPPORTED_BMP_FORMAT:
    return " : unsupported bmp format";
  case OUT_OF_MEMORY:
    return " : out of memory";
  case UNEXPECTED_EOF:
    return " : unexptected EOF";
  case CANT_OPEN_FILE:
    return " : can't open file";
  case WRITE_ERROR:
    return " : writing error";
  default:
    return " : I/O error";
  }
}

}  // namespace
//------------------------------------------------------------

TImageP TImageReaderBmp::load() {
  TImageP image;
  void *buff;
  int retCode =
      readbmp(getFilePath().getWideString().c_str(), &m_lx, &m_ly, &buff);
  if (retCode != OK) {
    throw TImageException(getFilePath(), buildBMPExceptionString(retCode));
  }

  TRaster32P raster;
  raster.create(m_lx, m_ly);
  raster->lock();
  memcpy(raster->pixels(), buff, m_lx * m_ly * sizeof(TPixel32));
  raster->unlock();
  TRasterImageP rasImage(raster);
  // image->setRaster(raster);
  delete[] buff;
  return TImageP(rasImage);
}

//------------------------------------------------------------

/*
void TImageReaderBmp::load(const TRasterP &ras, const TPoint &pos,
                              int shrinkX, int shrinkY)
{
void *buff;
int x0 = pos.x;
int y0 = pos.y;
int x1 = x0 + ras->getLx() * shrinkX - 1;
int y1 = y0 + ras->getLy() * shrinkY - 1;

int retCode = readbmpregion(m_path.getFullPath().c_str(), &buff, x0, y0, x1, y1,
shrinkX);
if (retCode!= OK)
  throw TImageException(getFilePath().getFullPath(),
buildBMPExceptionString(retCode));
memcpy(ras->getRawData(), buff, ras->getLx() * ras->getLy() * 4);
delete [] buff;
}
*/

//------------------------------------------------------------

TDimension TImageReaderBmp::getSize() const {
  TDimension d(0, 0);
  int retCode =
      readbmp_size(getFilePath().getWideString().c_str(), &(d.lx), &(d.ly));
  if (retCode != OK)
    throw TImageException(getFilePath(), buildBMPExceptionString(retCode));
  return d;
}

//------------------------------------------------------------

TRect TImageReaderBmp::getBBox() const {
  TRect r;
  int retCode = readbmp_bbox(getFilePath().getWideString().c_str(), &(r.x0),
                             &(r.y0), &(r.x1), &(r.y1));
  if (retCode != OK)
    throw TImageException(getFilePath(), buildBMPExceptionString(retCode));
  return r;
}

//============================================================

void TImageWriterBmp::save(const TImageP &image) {
  TRasterImageP rasImage(image);

  TRaster32P ras32;
  ras32 = rasImage->getRaster();

  TRasterGR8P ras8;
  ras8       = rasImage->getRaster();
  void *buff = 0;
  int pixSize;

  if (!ras32 && !ras8))
    throw TImageException(m_path, buildBMPExceptionString(UNSUPPORTED_BMP_FORMAT));

  rasImage->getRaster()->lock();
  if (ras32) {
    buff    = ras32->getRawData();
    m_lx    = ras32->getLx();
    m_ly    = ras32->getLy();
    pixSize = 32;
  } else {
    buff    = ras8->getRawData();
    m_lx    = ras8->getLx();
    m_ly    = ras8->getLy();
    pixSize = 8;
  }

  int retCode = writebmp(getFilePath().getWideString().c_str(), m_lx, m_ly,
                         buff, pixSize);
  rasImage->getRaster()->unlock();
  if (retCode != OK) {
    throw TImageException(m_path, buildBMPExceptionString(retCode));
  }
}
