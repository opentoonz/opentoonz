#pragma once

#ifndef TVECTORIMAGE_INCLUDED
#define TVECTORIMAGE_INCLUDED

#include <memory>

#include "timage.h"

// da togliere spostando cose in altri file!!
#include "traster.h"
#include "tstream.h"

#include <set>

#undef DVAPI
#undef DVVAR
#ifdef TVECTORIMAGE_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//#define NEW_REGION_FILL
#define DISEGNO_OUTLINE 0

//=============================================================================
// Forward declarations
class TVectorImageP;
class TStroke;
class TRegion;
class TRegionId;
class TColorStyle;
class TVectorRenderData;
class TRegionId;
class TFilledRegionInf;
namespace TThread {
class Mutex;
}

//=============================================================================

#define REGION_COMPUTING_PRECISION 128.0

const double c_newAutocloseTolerance = 1.15;

//-----------------------------------------------------------------------------

class VIStroke;

/*!
  TVectorImage: describe a vector image.
    A vector image is a set of strokes and regions.
  \relates  TImage
*/
class DVAPI TVectorImage final : public TImage {
  class Imp;
  int pickGroup(const TPointD &pos, bool onEnteredGroup) const;

public:
  std::unique_ptr<Imp> m_imp;

  struct IntersectionBranch {
    int m_strokeIndex;
    int m_style;
    double m_w;
    UINT m_currInter;
    UINT m_nextBranch;
    bool m_gettingOut;
  };
  //! if the vectorimage is loaded from disc, loaded=true
  TVectorImage(bool loaded = false);
  virtual ~TVectorImage();

  TImage::Type getType() const override { return VECTOR; }

  /*!Set valid regions flags
      call validateRegions() after region/stroke changes
*/
  void validateRegions(bool state = false);
  //! Get valid regions flags
  /*! Call validateRegions() after region/stroke changes
   */
  bool areValidRegions();

  //! Return a clone of image
  TVectorImageP clone() const;

  //! Create a new \b TImage
  TImage *cloneImage() const override;

  //! Transform a stroke using an affine \b TAffine
  void transform(const TAffine &aff, bool doChangeThickness = false);

  // usato solo in pli
  //! Put a region in the regions' container
  void putRegion(TRegion *region);

  //! Return the regions' count
  UINT getRegionCount() const;
  //! Get a region at index position
  TRegion *getRegion(UINT index) const;
  //! Get a region at \b TRegionId id
  TRegion *getRegion(TRegionId) const;

  //! returns the region equivalent to region, 0 if does not exists.
  TRegion *findRegion(const TRegion &region) const;
  // calcola tutte le regioni che intersecano  rect(e qualcuna in piu' a
  // volte...)
  void findRegions(const TRectD &rect);

  //! Return the strokes' count
  UINT getStrokeCount() const;
  //! Get a \b TStroke stroke at index position
  TStroke *getStroke(UINT index) const;
  //! Get a \b VIStroke stroke at index position
  VIStroke *getVIStroke(UINT index) const;
  //! Get a \b VIStroke stroke at id
  VIStroke *getStrokeById(int id) const;

  //! Get the stroke index by id
  int getStrokeIndexById(int id) const;

  //! Get the stroke index by id
  int getStrokeIndex(TStroke *stroke) const;

  //! Group strokes in the \b fromIndex - \b toIndex range
  /*! Only adjacent strokes can be grouped*/
  void group(int fromIndex, int count);

  //! Ungroup all the strokes in the group of index \b index.
  /*! Return the number of ungroped stroke.*/
  int ungroup(int index);

  bool isStrokeGrouped(UINT index) const { return getGroupDepth(index) > 0; }
  //! Return a value grater of zero if stroke of index \b index is contained in
  //! a group
  /*! The returned number is the depth of the nested group containing the
   * stroke.*/
  int getGroupDepth(UINT index) const;

  //! it says if two strokes are in the same group, even qhen the image is
  //! entered in a group.

  bool sameSubGroup(int strokeIndex0, int strokeIndex1) const;

  int getCommonGroupDepth(int strokeIndex0, int strokeIndex1) const;

  //! Return -1 if two object are contained in the same group.
  /*! Objects can be strokes or regions: they are identified by \b index1 and \b
     index2.
          If objects aren't in the same group, the method return the number of
     equal level into the nested groups.*/
  int areDifferentGroup(UINT index1, bool isRegion1, UINT index2,
                        bool isRegion2) const;

  // utility functions
  //! Return true if two strokes has common parent groups.
  bool sameParentGroupStrokes(UINT index1, UINT index2) const {
    int ret = areDifferentGroup(index1, false, index2, false);
    return (ret == -1 || ret >= 1);
  }
  //! Return true if two objects are contained into the same group.
  bool sameGroup(UINT index1, UINT index2) const {
    return areDifferentGroup(index1, false, index2, false) == -1;
  }
  //! Return true if a stroke and a region have common parent groups.
  bool sameGroupStrokeAndRegion(UINT strokeIndex, UINT regionIndex) const {
    return areDifferentGroup(strokeIndex, false, regionIndex, true) == -1;
  }

  // entering and exiting groups
  bool inCurrentGroup(int strokeIndex) const;
  bool canEnterGroup(int strokeIndex) const;
  bool selectable(int strokeIndex) const;
  bool enterGroup(int index);

  // return -1 if no possible to exit, otherwise a stroke index  which has same
  // id of the exiting group
  int exitGroup();
  bool isEnteredGroupStroke(int index) const;
  bool canMoveStrokes(int strokeIndex, int count, int moveBefore) const;
  // returns depth of group inside.
  int isInsideGroup() const;

  int pickGroup(const TPointD &pos) const;
  int getGroupByStroke(UINT index) const;
  int getGroupByRegion(UINT index) const;

  /*!
get the stroke nearest at point
\note outw is stroke parameter w in [0,1]
\par p [input] is point nearest stroke
\par outw [output] is parameter of minimum in stroke
\par strokeIndex [output] is index of stroke in vector image
\par dist2 [output] is the square value of distance
\ret true if a value is found
*/
  bool getNearestStroke(const TPointD &p,

                        // output
                        double &outw, UINT &strokeIndex, double &dist2,
                        bool inCurrentGroup = true) const;

  //! Enable or disable the style of a stroke according to the \b enable param.
  static void enableStrokeStyle(int index, bool enable);
  //! Return true if the style of the stroke identified by \b index is enabled.
  static bool isStrokeStyleEnabled(int index);

  //! Remove the stroke of index \b index and if \b doComputeRegions is true
  //! recompute the regions
  TStroke *removeStroke(int index, bool doComputeRegions = true);
  //! Remove the strokes identified by indexes in the vector \b toBeRemoved
  /*! If \b deleteThem is true strokes are really delete;
  if \b doComputeRegions is true recompute the regions*/
  void removeStrokes(const std::vector<int> &tobeRemoved, bool deleteThem,
                     bool recomputeRegions);

  //! Delete the \b TStroke stroke of index \b index
  void deleteStroke(int index);
  //! Delete the \b VIStroke stroke
  void deleteStroke(VIStroke *stroke);

  //! Add a stroke at the end of the vector; returns position of the stroke (or
  //! -1 if not added)
  int addStroke(TStroke *, bool discardPoints = true);
  int addStrokeToGroup(TStroke *stroke, int strokeIndex);
  //! Replace the stroke at index \b index with \b newStroke
  void replaceStroke(int index, TStroke *newStroke);

  //! Insert a \b VIStroke \b vs at index \b strokeIndex
  void insertStrokeAt(VIStroke *vs, int strokeIndex,
                      bool recomputeRegions = true);  //! Move \b count strokes
                                                      //! starting from \b
                                                      //! fromIndex before the
                                                      //! stroke identified by
                                                      //! the \b moveBefore
                                                      //! index.
  void moveStrokes(int fromIndex, int count, int moveBefore);
  //! Find regions of a \b TVectorImage
  void findRegions();

// Gmt. VA TOLTO IL PRIMA POSSIBILE.
// E' una pessima cosa rendere platform dependent l'interfaccia pubblica di una
// classe cosi' importante come la VectorImage
#if defined(LINUX) || defined(FREEBSD) || defined(MACOSX)
  void render(const TVectorRenderData &rd, TRaster32P &ras);
#endif

  //! Make the rendering of the vector image, return a \b TRaster32P with the
  //! image rendered
  TRaster32P render(bool onlyStrokes);
  //! Return the region which contains \b p, if none regions contains \b p
  //! return 0
  TRegion *getRegion(const TPointD &p);

  //! Fill the region which contains \b p with \b styleId. it returns the  style
  //! of the region before filling or -1 if not filled.
  int fill(const TPointD &p, int styleId, bool onlyEmpty = false);
  //! Fill all the regions and strokes contained in \b selectArea or contained
  //! into the regions formed by the stroke \b s with the style \b styleId.
  /*! If \b onlyUnfilled is true, only regions filled with the style 0 are
     filled with the new stile.
          If \b fillAreas is true regions are filled.
          If \b fillLines is true stroke are filled.*/
  bool selectFill(const TRectD &selectArea, TStroke *s, int styleId,
                  bool onlyUnfilled, bool fillAreas, bool fillLines);

  //! Fill all regions contained in the \b stroke area with \b styleIndex
  void areaFill(TStroke *stroke, int styleIndex, bool onlyUnfilled);
  //! Fill the stroke which contains \b p with \b newStyleId; it returns the
  //! style of stroke before filling or -1 if not filled.
  int fillStrokes(const TPointD &p, int newStyleId);

  //! Return true if \b computeRegion method was called
  bool isComputedRegionAlmostOnce() const;

  //! Return the image bounding box in the image coordinate system
  TRectD getBBox() const override;

  //! Call the following method after stroke modification
  //! \note you must specify, using the second argument, whether the
  //! modification was a reflection
  void notifyChangedStrokes(const std::vector<int> &strokeIndexArray,
                            const std::vector<TStroke *> &oldStrokeArray,
                            bool areFlipped = false);

  //! Call the following method after stroke modification
  void notifyChangedStrokes(int strokeIndex, TStroke *oldStroke = 0,
                            bool isFlipped = false);

  UINT getFillData(std::unique_ptr<IntersectionBranch[]> &v);
  void setFillData(std::unique_ptr<IntersectionBranch[]> const &v, UINT size,
                   bool doComputeRegions = true);

  void drawAutocloses(const TVectorRenderData &rd) const;  // debug method

  /*! Includes a (transformed) copy of imgs in this. If setSelected==true then
     selects imported strokes.
          It also includes the color information.
          Try to assign the same stroke ids (if unused)
  */
  void enableRegionComputing(bool enabled, bool notIntersectingStrokes);

  /*! if enabled, region edges are joined together when possible. for flash
   * render, should be disabled!
   */
  void enableMinimizeEdges(bool enabled);
  /*! Creates a new Image using the selected strokes. If removeFlag==true then
     removes selected strokes
      It includes (in the new image) the color information too.
          It maintains stroke ids.
  */
  TVectorImageP splitSelected(bool removeFlag);

  //! Merge the image with the \b img.
  int mergeImage(const TVectorImageP &img, const TAffine &affine,
                 bool sameStrokeId = true);

  int mergeImage(const TVectorImageP &img, const TAffine &affine,
                 const std::map<int, int> &styleTable,
                 bool sameStrokeId = true);
  //! Merge the image with the vector of image \b images.
  void mergeImage(const std::vector<const TVectorImage *> &images);

  //! Insert the \b TVectorImageP \b img
  void insertImage(const TVectorImageP &img,
                   const std::vector<int> &dstIndices);
  TVectorImageP splitImage(const std::vector<int> &indices, bool removeFlag);

  //! Return the used styles in the image
  void getUsedStyles(std::set<int> &styles) const;
  //! Reassign all stroke's style
  void reassignStyles(std::map<int, int> &table);

  //! Transfer the stroke style of the \b sourceStroke in the \b sourceImage to
  //! the style
  static void transferStrokeColors(TVectorImageP sourceImage, int sourceStroke,
                                   TVectorImageP destinationImage,
                                   int destinationStroke);
  //! Set the edges of the stroke identified by \b strokeIndex with the styles
  //! \b leftColorIndex and \brightColorIndex.
  void setEdgeColors(int strokeIndex, int leftColorIndex, int rightColorIndex);

  /*! This functions splits stroke with index 'strokeIndex' in n+1 strokes,
     where n is the size of vector sortedW,
      cutting it in points w specified in sortedW. SortedW must be sorted in
     ascending order.
          Resulting strokes are put in VectorImage in position
     strokeIndex,strokeIndex+1, ... strokeIndex+n.
          Information on fill colors are maintened, as much as possible. */
  void splitStroke(int strokeIndex,
                   const std::vector<DoublePair> &sortedWRanges);
  VIStroke *joinStroke(int index1, int index2, int cpIndex1, int cpIndex2,
                       bool isSmooth);
  VIStroke *extendStroke(int index, const TThickPoint &p, int cpIndex,
                         bool isSmooth);
  /*! this method removes the parts  of the stroke that are not bounds of
regions. only ending parts are removed.
If the entire stroke is not bounding any region, it is kept entitely.
it returns the original stroke (for undo)*/
  TStroke *removeEndpoints(int strokeIndex);

  /*! this method replaces  the stroke at index with oldstroke. Oldstroke is
supposed to contain
existing stroke. this method is used for undoing removeEndpoints . */
  void restoreEndpoints(int index, TStroke *oldStroke);

  //! Set the autoclose tolerance to the specified value.
  void setAutocloseTolerance(double val);
  //! Return the autoclose tolerance.
  double getAutocloseTolerance() const;

  //! forces the recomputing of all regions (it is actually done only after
  //! having loaded the vectorimage)
  void recomputeRegionsIfNeeded();

  /*! Remove all image strokes and all image regions with style index contained
   * in \b styleIds vector.*/
  void eraseStyleIds(const std::vector<int> styleIds);

  TThread::Mutex *getMutex() const;

#ifdef _DEBUG
  void checkIntersections();
#endif

  void computeRegion(const TPointD &p, int styleId);

#ifdef NEW_REGION_FILL
  void resetRegionFinder();
#endif

private:  // not implemented
  TVectorImage(const TVectorImage &);
  TVectorImage &operator=(const TVectorImage &);
};

DVAPI VIStroke *cloneVIStroke(VIStroke *vs);
DVAPI void deleteVIStroke(VIStroke *vs);

DVAPI void getClosingPoints(const TRectD &rect, double fac,
                            const TVectorImageP &vi,
                            std::vector<std::pair<int, double>> &startPoints,
                            std::vector<std::pair<int, double>> &endPoints);

//-----------------------------------------------------------------------------

#ifdef _WIN32
template class DVAPI TSmartPointerT<TVectorImage>;
template class DVAPI TDerivedSmartPointerT<TVectorImage, TImage>;
#endif

class DVAPI TVectorImageP final
    : public TDerivedSmartPointerT<TVectorImage, TImage> {
public:
  TVectorImageP() {}
  TVectorImageP(TVectorImage *image) : DerivedSmartPointer(image) {}
  TVectorImageP(TImageP image) : DerivedSmartPointer(image) {}
#if !defined(_WIN32)
  TVectorImageP(TImage *image) : DerivedSmartPointer(TImageP(image)) {}
#endif
  operator TImageP() { return TImageP(m_pointer); }
};

//-----------------------------------------------------------------------------

// GMT: DA TOGLIERE. vedi sopra
#if defined(LINUX) || defined(FREEBSD)
DVAPI void hardRenderVectorImage(const TVectorRenderData &rd, TRaster32P &r,
                                 const TVectorImageP &vimg);
#endif
//-----------------------------------------------------------------------------

//=============================================================================

class DVAPI TInputStreamInterface {
public:
  TInputStreamInterface() {}
  virtual ~TInputStreamInterface() {}

  virtual TInputStreamInterface &operator>>(double &)      = 0;
  virtual TInputStreamInterface &operator>>(int &)         = 0;
  virtual TInputStreamInterface &operator>>(std::string &) = 0;
  virtual TInputStreamInterface &operator>>(UCHAR &)       = 0;
  virtual TInputStreamInterface &operator>>(USHORT &)      = 0;
  virtual TInputStreamInterface &operator>>(TRaster32P &)  = 0;
  virtual TInputStreamInterface &operator>>(TPixel32 &pixel);

  virtual VersionNumber versionNumber() const { return VersionNumber(); }
};

//-----------------------------------------------------------------------------

class DVAPI TOutputStreamInterface {
public:
  TOutputStreamInterface() {}
  virtual ~TOutputStreamInterface() {}

  virtual TOutputStreamInterface &operator<<(double)             = 0;
  virtual TOutputStreamInterface &operator<<(int)                = 0;
  virtual TOutputStreamInterface &operator<<(std::string)        = 0;
  virtual TOutputStreamInterface &operator<<(UCHAR)              = 0;
  virtual TOutputStreamInterface &operator<<(USHORT)             = 0;
  virtual TOutputStreamInterface &operator<<(const TRaster32P &) = 0;
  virtual TOutputStreamInterface &operator<<(const TPixel32 &pixel);
};

#if DISEGNO_OUTLINE == 1
extern int CurrStrokeIndex;
#include "tvectorimage.h"
extern const TVectorImage *CurrVimg;
#else
#ifndef DISEGNO_OUTLINE
   *&^&%^^$%&^%$(^(
#endif
#endif

#endif
