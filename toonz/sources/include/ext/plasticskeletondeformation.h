#pragma once

#ifndef PLASTICSKELETONDEFORMATION_H
#define PLASTICSKELETONDEFORMATION_H

#include <memory>

// TnzCore includes
#include "tsmartpointer.h"
#include "tdoubleparam.h"
#include "tdoublekeyframe.h"

// TnzExt includes
#include "ext/plastichandle.h"
#include "ext/plasticskeleton.h"

// tcg includes
#include "tcg/tcg_any_iterator.h"

// Qt includes
#include <QString>

#undef DVAPI
#undef DVVAR
#ifdef TNZEXT_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//====================================================

//    Forward declarations

class ParamsObserver;
class ParamChange;

//====================================================

//**************************************************************************************
//    PlasticSkeletonVertexDeformation  declaration
//**************************************************************************************

//! The deformation of a plastic skeleton vertex.
typedef struct DVAPI PlasticSkeletonVertexDeformation final : public TPersist {
  PERSIST_DECLARATION(PlasticSkeletonVertexDeformation)

public:
  enum Params {
    ANGLE = 0,  //!< Distance from parent vertex (delta)
    DISTANCE,   //!< Angle with parent edge (delta)
    SO,         //!< Vertex's stacking order
    PARAMS_COUNT
  };

  struct Keyframe {
    TDoubleKeyframe m_keyframes[PARAMS_COUNT];
  };

public:
  TDoubleParamP m_params[PARAMS_COUNT];

public:
  Keyframe getKeyframe(double frame) const;

  void setKeyframe(double frame);
  bool setKeyframe(const Keyframe &values);
  bool setKeyframe(const Keyframe &values, double frame, double easeIn = -1.0,
                   double easeOut = -1.0);

  bool isKeyframe(double frame) const;
  bool isFullKeyframe(double frame) const;
  void deleteKeyframe(double frame);

  void saveData(TOStream &os) override;
  void loadData(TIStream &is) override;

} SkVD;

//**************************************************************************************
//    PlasticSkeletonDeformationKeyframe  declaration
//**************************************************************************************

//! The keyframe of a plastic skeleton vertex deformation.
/*!
  \note A deformation keyframe stores vertex deformation keyframes by vertex \a
  names.
  This is the approach we use to deal with keyframe pasting to different
  skeletons.
*/
typedef struct PlasticSkeletonDeformationKeyframe {
  std::map<QString, SkVD::Keyframe>
      m_vertexKeyframes;             //!< Keyframes by vertex \a name
  TDoubleKeyframe m_skelIdKeyframe;  //!< Skeleton id keyframe

} SkDKey;

//**************************************************************************************
//    PlasticSkeletonDeformation  declaration
//**************************************************************************************

/*!
  PlasticSkeletonDeformation models the deformation of a group of
PlasticSkeleton instances.

\par Description

  A PlasticSkeleton instance is typically used to act as a deformable object -
in other
  words, it defines the 'original' form of a hierarchy of vertices that can be
manipulated
  interactively to obtain a 'deformed' configuration.
  \n\n
  PlasticSkeletonDeformation represents a deformation of PlasticSkeleton
objects,
  therefore acting primarily as a collection of PlasticSkeletonVertexDeformation
  instances - one per skeleton vertex. The collection is an associative
container mapping a
  <I> vertex name <\I> to its deformation; using names as keys is a useful
abstraction
  that allows vertex deformations data (eg keyframes) to be copied to skeleton
deformations
  whose skeletons have a different internal configuration (ie vertex indices and
such).
  \n\n
  Each vertex deformation also stores a unique <I> hook number <\I> that can be
used during
  xsheet animation to link stage objects to a skeleton vertex.

\par The Skeletons group

  The PlasticSkeletonDeformation implementation has been extended to work on
multiple
  skeletons for one class instance. This class stores a map associating skeleton
indices
  to the skeleton instances, that can be used to select a skeleton to be
deformed with
  the deformation's data.
  \n\n
  Vertices in different skeleton instances share the same animation if their
name is the same.
  \n\n
  This class provides an animatable parameter that is intended to choose the \a
active
  skeleton along an xsheet timeline. It is retrievable through the
skeletonIdsParam() method.

\par Notable implementation details

  In current implementation, a PlasticSkeletonDeformation keeps shared ownership
of the
  skeletons it is attached to. It is therefore intended to be a \a container of
said skeletons.
*/
class DVAPI PlasticSkeletonDeformation final : public TSmartObject,
                                               public TPersist {
  DECLARE_CLASS_CODE
  PERSIST_DECLARATION(PlasticSkeletonDeformation)

private:
  class Imp;
  std::unique_ptr<Imp> m_imp;

public:
  typedef tcg::any_it<int, int, void *>::bidirectional skelId_iterator;
  typedef tcg::any_it<std::pair<const QString *, SkVD *>,
                      std::pair<const QString *, SkVD *>, void *>::bidirectional
      vd_iterator;
  typedef tcg::any_it<std::pair<int, int>, std::pair<int, int>,
                      void *>::bidirectional vx_iterator;

public:
  PlasticSkeletonDeformation();  //!< Constructs an empty deformation
  PlasticSkeletonDeformation(
      const PlasticSkeletonDeformation
          &other);  //!< Constructs a deformation \a cloning other's skeletons
  ~PlasticSkeletonDeformation();

  PlasticSkeletonDeformation &operator=(
      const PlasticSkeletonDeformation &other);

  // Skeleton-related methods

  bool empty() const;
  int skeletonsCount() const;

  //! Acquires <I> shared ownership <\I> of the specified skeleton, under given
  //! skeletonId
  void attach(int skeletonId, PlasticSkeleton *skeleton);

  //! Releases the skeleton associated to specified skeletonId
  void detach(int skeletonId);

  PlasticSkeletonP skeleton(int skeletonId) const;
  int skeletonId(PlasticSkeleton *skeleton) const;

  //! Returns the ordered range containing the skeleton ids
  void skeletonIds(skelId_iterator &begin, skelId_iterator &end) const;

  TDoubleParamP skeletonIdsParam()
      const;  //!< Returns the skeleton id by frame animatable parameter

  PlasticSkeletonP skeleton(
      double frame) const;  //!< Returns the \a active skeleton by xsheet frame
  int skeletonId(double frame)
      const;  //!< Returns the \a active skeleton id by xsheet frame

  // Vertex deformations-related methods

  int vertexDeformationsCount() const;

  SkVD *vertexDeformation(const QString &vertexName)
      const;  //!< Returns the vertex deformation associated to given
              //!< vertex name. The returned pointer is <I> owned by
              //!< the deformation - it must \b not be deleted <\I>
  SkVD *vertexDeformation(int skelId, int v) const;

  void vertexDeformations(vd_iterator &begin, vd_iterator &end)
      const;  //!< Returns the ordered range of vertex deformations

  void vdSkeletonVertices(const QString &vertexName, vx_iterator &begin,
                          vx_iterator &end)
      const;  //!< Returns the ordered range of skeleton vertices
              //!< (at max one per skeleton id) associated to a
              //!< vertex name
  // Hook number-related methods

  int hookNumber(const QString &name) const;
  int hookNumber(int skelId, int v) const;

  QString vertexName(int hookNumber) const;
  int vertexIndex(int hookNumber, int skelId) const;

  // Parameters-related methods

  void addObserver(TParamObserver *observer);
  void removeObserver(TParamObserver *observer);

  void setGrammar(TSyntax::Grammar *grammar);

  // Keyframes-related methods

  void getKeyframeAt(double frame, SkDKey &keysMap)
      const;  //!< \note keysMap returned by argument to avoid map
              //!< copies in case move semantics is not available
  void setKeyframe(double frame);
  bool setKeyframe(const SkDKey &keyframe);
  bool setKeyframe(const SkDKey &keyframe, double frame, double easeIn = -1.0,
                   double easeOut = -1.0);

  bool isKeyframe(double frame) const;
  bool isFullKeyframe(double frame) const;
  void deleteKeyframe(double frame);

  // Interface methods using a deformed copy of the original skeleton (which is
  // owned by this class)

  void storeDeformedSkeleton(int skeletonId, double frame,
                             PlasticSkeleton &skeleton) const;

  void updatePosition(const PlasticSkeleton &originalSkeleton,
                      PlasticSkeleton &deformedSkeleton, double frame, int v,
                      const TPointD &pos);
  void updateAngle(const PlasticSkeleton &originalSkeleton,
                   PlasticSkeleton &deformedSkeleton, double frame, int v,
                   const TPointD &pos);

protected:
  void saveData(TOStream &os) override;
  void loadData(TIStream &is) override;

private:
  friend class PlasticSkeleton;

  void addVertex(
      PlasticSkeleton *sk,
      int v);  //!< Deals with vertex deformations when v has been added
  void insertVertex(PlasticSkeleton *sk, int v);  //!< Deals with vertex
                                                  //! deformations when v has
  //! been inserted in an edge
  void deleteVertex(
      PlasticSkeleton *sk,
      int v);  //!< Removes vertex deformation for v, \a before it is deleted
  void vertexNameChange(
      PlasticSkeleton *sk, int v,
      const QString &newName);      //!< Rebinds a vertex deformation name
  void clear(PlasticSkeleton *sk);  //!< Clears all vertex deformations

  void loadData_prerelease(
      TIStream &is);  // Toonz 7.0 pre-release loading function. Will be deleted
                      // in the next minor release.
};

typedef PlasticSkeletonDeformation SkD;

//===============================================================================

#ifdef _WIN32
#ifndef TFX_EXPORTS
template class DVAPI TSmartPointerT<PlasticSkeletonDeformation>;
#endif
#endif

typedef TSmartPointerT<PlasticSkeletonDeformation> PlasticSkeletonDeformationP;
typedef PlasticSkeletonDeformationP SkDP;

#endif  // PLASTICSKELETONDEFORMATION_H
