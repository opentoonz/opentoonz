#pragma once

#ifndef TRASTERIMAGE_INCLUDED
#define TRASTERIMAGE_INCLUDED

#include "timage.h"
#include "traster.h"

#undef DVAPI
#undef DVVAR
#ifdef TRASTERIMAGE_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//-------------------------------------------------------------------

//! An image containing a raster.

/*!
  Some examples:
 \include rasterImage_ex.cpp
*/

class DVAPI TRasterImage final : public TImage {
  TRasterP m_mainRaster, m_patchRaster, m_iconRaster;

  //! dpi value for x axis
  double m_dpix,
      //! dpi value for y axis
      m_dpiy;
  //! The name of the image
  std::string m_name;
  //! The savebox of the image
  TRect m_savebox;
  // double m_hPos;
  //! Specify if the image is an opaque image.
  bool m_isOpaque;
  //! Specify if the image is a BW Scan image.
  bool m_isScanBW;

  //! The offset of the image
  TPoint m_offset;

  int m_subsampling;

public:
  TRasterImage();
  TRasterImage(const TRasterP &raster);
  ~TRasterImage();

private:
  //! Is used to clone an existing ToonzImage.
  TRasterImage(const TRasterImage &);

  //! not implemented
  TRasterImage &operator=(const TRasterImage &);

public:
  //! Return the image type
  TImage::Type getType() const override { return TImage::RASTER; }

  // image info
  //! Return the name of the image
  std::string getName() const { return m_name; }
  //! Set the name of the image
  void setName(std::string name) { m_name = name; }

  //! Get the \b dpi image
  void getDpi(double &dpix, double &dpiy) const {
    dpix = m_dpix;
    dpiy = m_dpiy;
  }
  //! Set the \b dpi image
  void setDpi(double dpix, double dpiy) {
    m_dpix = dpix;
    m_dpiy = dpiy;
  }

  int getSubsampling() const { return m_subsampling; }
  void setSubsampling(int s);

  //! Return the save box of the image
  TRect getSavebox() const { return m_savebox; }
  //! Set the save box of the image
  void setSavebox(const TRect &rect) { m_savebox = rect; }

  //! Return the bbox of the image
  TRectD getBBox() const override { return convert(m_savebox); }

  //! Return raster image offset \b m_offset
  TPoint getOffset() const { return m_offset; }
  //! Set raster image offset \b m_offset
  void setOffset(const TPoint &offset) { m_offset = offset; }

  //! Return raster hPos \b m_hPos
  // double gethPos() const {return m_hPos;}
  //! Set raster hPos \b m_hPos
  // void sethPos(double hPos) {m_hPos= hPos;}

  //! Return a clone of image
  TImage *cloneImage() const override;

  //! Return \b TRasterP
  const TRasterP &getRaster() const { return m_mainRaster; }
  //! Return \b TRasterP
  TRasterP &getRaster() { return m_mainRaster; }

  TRasterP raster() const override { return m_mainRaster; }

  //! Set the \b TRasterP \b raster
  void setRaster(const TRasterP &raster);

  //! Return a \b TRasterP contained the icon of the image \b m_iconRaster
  const TRasterP &getIconRaster() const { return m_iconRaster; }
  //! Return a \b TRasterP contained the icon of the image \b m_iconRaster
  TRasterP &getIconRaster() { return m_iconRaster; }
  //! Set a \b TRasterP contained the icon of the image \b m_iconRaster
  void setIconRaster(const TRasterP &raster) { m_iconRaster = raster; }

  //! Return true if the raster is empty
  bool isEmpty() const { return !m_mainRaster; }

  //! Return true if the raster comes from BW Scan
  bool isScanBW() const { return m_isScanBW; }
  void setScanBWFlag(bool isScanBW) { m_isScanBW = isScanBW; }

  //! Return true if the raster is opaque

  bool isOpaque() const { return m_isOpaque; }
  void setOpaqueFlag(bool isOpaque) { m_isOpaque = isOpaque; }

  //! Make the icon of the raster.
  void makeIcon(const TRaster32P &raster);
};

//-------------------------------------------------------------------

#ifdef _WIN32
template class DVAPI TSmartPointerT<TRasterImage>;
template class DVAPI TDerivedSmartPointerT<TRasterImage, TImage>;
#endif

class DVAPI TRasterImageP final
    : public TDerivedSmartPointerT<TRasterImage, TImage> {
public:
  TRasterImageP() {}
  TRasterImageP(TRasterImage *image) : DerivedSmartPointer(image) {}
  TRasterImageP(TImageP image) : DerivedSmartPointer(image) {}
  TRasterImageP(const TRasterP &ras)
      : DerivedSmartPointer(new TRasterImage(ras)) {}
  operator TImageP() { return TImageP(m_pointer); }
};

#endif
