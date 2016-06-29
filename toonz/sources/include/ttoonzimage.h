#pragma once

#ifndef TTOONZIMAGE_INCLUDED
#define TTOONZIMAGE_INCLUDED

#include "trastercm.h"
#include "tthreadmessage.h"
#include "timage.h"

#undef DVAPI
#undef DVVAR
#ifdef TNZCORE_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif
class TToonzImageP;

//! An image containing a Toonz raster.

class DVAPI TToonzImage final : public TImage {
  //! dpi value for x axis
  double m_dpix,
      //! dpi value for y axis
      m_dpiy;
  int m_subsampling;
  //! The name of the image
  std::string m_name;
  //! The savebox of the image
  TRect m_savebox;
  // double m_hPos;
  //! The offset of the image
  TPoint m_offset;
  //! ColorMapped raster of the image.
  TRasterCM32P m_ras;
  TThread::Mutex m_mutex;

public:
  TToonzImage();
  TToonzImage(const TRasterCM32P &raster, const TRect &saveBox);

  ~TToonzImage();

private:
  //! Is used to clone an existing ToonzImage.
  TToonzImage(const TToonzImage &);

  //! Not implemented
  TToonzImage &operator=(const TToonzImage &);

public:
  //! Return the type of the image.
  TImage::Type getType() const override { return TImage::TOONZ_RASTER; }

  //! Return the size of the Image.
  TDimension getSize() const { return m_size; }

  //! Get the dpi values of the image for x and y axes.
  void getDpi(double &dpix, double &dpiy) const {
    dpix = m_dpix;
    dpiy = m_dpiy;
  }
  //! Set the dpi values of the image for x and y axes.
  void setDpi(double dpix, double dpiy) {
    m_dpix = dpix;
    m_dpiy = dpiy;
  }

  int getSubsampling() const { return m_subsampling; }
  void setSubsampling(int s);

  //! Return the savebox of the image
  TRect getSavebox() const { return m_savebox; }
  //! Set the savebox of the image.
  /*! The savebox setted is the intersection between \b rect and the image
   * box.*/
  void setSavebox(const TRect &rect);

  //! Return the boundin box of the image.
  TRectD getBBox() const override { return convert(m_savebox); }

  //! Return the offset point of the image.
  TPoint getOffset() const { return m_offset; }
  //! Set the offset point of the image.
  void setOffset(const TPoint &offset) { m_offset = offset; }

  //! Return raster hPos \b m_hPos
  // double gethPos() const {return m_hPos;}
  //! Return a clone of image
  // void sethPos(double hPos) {m_hPos= hPos;}

  //! Return a clone of the current image
  TImage *cloneImage() const override;

  //! Return the image's raster
  TRasterCM32P getCMapped() const;
  //! Return a copy of the image's raster.
  void getCMapped(const TRasterCM32P &ras);
  //! Set the image's raster
  void setCMapped(const TRasterCM32P &ras);

  //! Return the image's raster.
  /*! Call the getCMapped() method.*/
  TRasterCM32P getRaster() const { return getCMapped(); }

  TRasterP raster() const override { return (TRasterP)getCMapped(); }

  //! Return a clone of the current image.
  TToonzImageP clone() const;

private:
  //! Image dimension
  TDimension m_size;
};

//-------------------------------------------------------------------

#ifdef _WIN32
template class DVAPI TSmartPointerT<TToonzImage>;
template class DVAPI TDerivedSmartPointerT<TToonzImage, TImage>;
#endif

class DVAPI TToonzImageP final
    : public TDerivedSmartPointerT<TToonzImage, TImage> {
public:
  TToonzImageP() {}
  TToonzImageP(TToonzImage *image) : DerivedSmartPointer(image) {}
  TToonzImageP(TImageP image) : DerivedSmartPointer(image) {}
  TToonzImageP(const TRasterCM32P &ras, const TRect &saveBox)
      : DerivedSmartPointer(new TToonzImage(ras, saveBox)) {}
  operator TImageP() { return TImageP(m_pointer); }
};

#endif
