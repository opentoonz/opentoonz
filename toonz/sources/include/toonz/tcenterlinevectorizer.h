#pragma once

#ifndef T_CENTERLINE_VECTORIZER
#define T_CENTERLINE_VECTORIZER

#include "toonz/vectorizerparameters.h"
#include "tvectorimage.h"
#include <deque>
#include <list>

#include <QObject>

#undef DVAPI
#undef DVVAR
#ifdef TOONZLIB_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//==============================
//    Core vectorizer class
//==============================

//! Contains specific vectorization methods and deals with partial progress
//! notifications (using Qt signals).
/*!VectorizerCore class is the lowest layer of a vectorization process, it
provides vectorization of a
single input raster image by calling the \b vectorize method.

It can also deal notifications about its progress status, and is receptive to
user cancels.

\sa VectorizerPopup, Vectorizer, VectorizerConfiguration classes.*/
class DVAPI VectorizerCore final : public QObject {
  Q_OBJECT

  int m_currPartial;
  int m_totalPartials;

  bool m_isCanceled;

public:
  VectorizerCore() : m_currPartial(0), m_isCanceled(false) {}
  ~VectorizerCore() {}

  /*!Calls the appropriate technique to convert \b image to vectors depending on
\b c.
Also provides post-processing operations such as regions computing and painting
(using
colors found in \b palette).
Returns the \b TVectorImageP converted image.*/
  TVectorImageP vectorize(const TImageP &image,
                          const VectorizerConfiguration &c, TPalette *palette);

  //! Returns true if vectorization was aborted at user's request
  bool isCanceled() { return m_isCanceled; }

  //!\b (\b Internal \b use \b only) Sets the maximum number of partial
  //! notifications.
  void setOverallPartials(int total) { m_totalPartials = total; }
  //!\b (\b Internal \b use \b only) Emits partial progress signal and updates
  //! partial progresses internal count.
  void emitPartialDone(void);

private:
  /*!Converts \b image to vectors in centerline mode, depending on \b
configuration.
Returns image converted.
Note: if true==configuration.m_naaSource then it change the image, transforming
it to a ToonzImage */
  TVectorImageP centerlineVectorize(
      TImageP &image, const CenterlineConfiguration &configuration,
      TPalette *palette);

  /*!Converts \b image to vectors in outline mode, depending on \b
configuration.
Returns image converted.*/
  TVectorImageP outlineVectorize(const TImageP &image,
                                 const OutlineConfiguration &configuration,
                                 TPalette *palette);

  /*!Converts \b image to vectors in outline mode, depending on \b
configuration.
Returns image converted.*/
  TVectorImageP newOutlineVectorize(
      const TImageP &image, const NewOutlineConfiguration &configuration,
      TPalette *palette);

  //! Calculates and applies fill colors once regions of \b vi have been
  //! computed.
  void applyFillColors(TVectorImageP vi, const TImageP &img, TPalette *palette,
                       const VectorizerConfiguration &c);
  void applyFillColors(TRegion *r, const TRasterP &ras, TPalette *palette,
                       const CenterlineConfiguration &c, int regionCount);
  // void applyFillColors(TRegion *r, const TRasterP &ras, TPalette *palette,
  //                     const OutlineConfiguration &c, int regionCount);

  //! Traduces the input VectorizerConfiguration into an edible form.
  VectorizerConfiguration traduceConfiguration(
      const VectorizerConfiguration &configuration);

  bool isInkRegionEdge(TStroke *stroke);
  bool isInkRegionEdgeReversed(TStroke *stroke);
  void clearInkRegionFlags(TVectorImageP vi);

signals:

  //! Partial progress \b par1 of overall \b par2 is notified.
  void partialDone(int, int);

protected slots:

  //! Receives a user cancel signal and attempts an early exit from
  //! vectorization process.
  void onCancel() { m_isCanceled = true; }
};

#endif  // T_CENTERLINE_VECTORIZER
