#pragma once

#ifndef TIMAGEINFO_H
#define TIMAGEINFO_H

#undef DVAPI
#undef DVVAR
#ifdef TNZCORE_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//================================================

//    Forward declarations

class TPropertyGroup;

//================================================

//*************************************************************************
//    TImageInfo  class
//*************************************************************************

/*!
  \brief    Stores description data about a generic image.

  \todo     Stores useless/redundant/improper data which should be moved or
            removed.
*/

class DVAPI TImageInfo {
public:
  // NOTE: Fields ordered by type size - minimizes padding

  double m_dpix,    //!< Horizontal image dpi.
      m_dpiy,       //!< Vertical image dpi.
      m_frameRate;  //!< Movie frame rate. \deprecated Should not be here. An
                    //! image has \a no frame rate!

  TPropertyGroup *m_properties;  //!< Format-specific image data.

  int m_lx,              //!< Image width.
      m_ly,              //!< Image height.
      m_x0,              //!< Image contents rect's left coordinate.
      m_y0,              //!< Image contents rect's bottom coordinate.
      m_x1,              //!< Image contents rect's right coordinate.
      m_y1,              //!< Image contents rect's top coordinate.
      m_samplePerPixel,  //!< Number of samples (channels) per pixel.
      m_bitsPerSample,   //!< Number of bits per sample (channel).
      m_fileSize;  //!< Total size (in bytes) of the image file. \deprecated
                   //! Possibly useless.

  bool m_valid;  //!< \a Deprecated. \deprecated Just... wrong.

public:
  TImageInfo()
      : m_dpix(0)
      , m_dpiy(0)
      , m_frameRate(12)
      , m_properties(0)
      , m_lx(0)
      , m_ly(0)
      , m_x0(0)
      , m_y0(0)
      , m_x1(-1)
      , m_y1(-1)
      , m_samplePerPixel(0)
      , m_bitsPerSample(8)
      , m_fileSize(0)
      , m_valid(false) {}

  TImageInfo(int lx, int ly)
      : m_dpix(0)
      , m_dpiy(0)
      , m_frameRate(12)
      , m_properties(0)
      , m_lx(lx)
      , m_ly(ly)
      , m_x0(0)
      , m_y0(0)
      , m_x1(-1)
      , m_y1(-1)
      , m_samplePerPixel(0)
      , m_bitsPerSample(8)
      , m_fileSize(0)
      , m_valid(false) {}
};

#endif  // TIMAGEINFO_H
