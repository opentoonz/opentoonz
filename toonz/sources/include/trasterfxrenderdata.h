#pragma once

#ifndef TRASTERFXRENDERDATA_H
#define TRASTERFXRENDERDATA_H

// TnzCore includes
#include "tcommon.h"

#undef DVAPI
#undef DVVAR
#ifdef TFX_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//******************************************************************************
//    TRasterFxRenderData  class
//******************************************************************************

//! Base class for custom data that can added by an fx to be processed in a
//! downstrem fx.
class DVAPI TRasterFxRenderData : public TSmartObject {
public:
  virtual ~TRasterFxRenderData() {}

  virtual bool operator==(const TRasterFxRenderData &data) const = 0;
  virtual std::string toString() const                           = 0;

  //! Returns a value respresenting the type of a renderData instance. The type
  //! is used to sort renderData entries for both retrieval and processing.
  //! The floating return type specification is used to allow insertions between
  //! two close-up values, if needed.
  virtual float typeIndex() const = 0;

  //! Currently used by TColumnFx to find out whether a renderData instance
  //! will enlarge the rendering of the specified bounding box.
  virtual TRectD getBBoxEnlargement(const TRectD &bbox) { return bbox; }
};

//===================================================================

#ifdef WIN32
template class DVAPI TSmartPointerT<TRasterFxRenderData>;
#endif
typedef TSmartPointerT<TRasterFxRenderData> TRasterFxRenderDataP;

#endif  // TRASTERFXRENDERDATA_H
