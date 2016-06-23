#pragma once

#ifndef PLASTIDEFORMERSTORAGE_H
#define PLASTIDEFORMERSTORAGE_H

#include <memory>

// TnzExt includes
#include "ext/plasticdeformer.h"

#undef DVAPI
#undef DVVAR
#ifdef TNZEXT_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//=======================================================================

//    Forward Declarations

struct PlasticVisualSettings;
struct DrawableMeshImage;

class PlasticSkeletonDeformation;

//=======================================================================

//***********************************************************************************************
//    PlasticDeformerData  declaration
//***********************************************************************************************

//! PlasticDeformerData contains all the data needed to perform the deformation
//! of a single mesh.
struct DVAPI PlasticDeformerData {
  PlasticDeformer m_deformer;  //!< The mesh deformer itself

  std::unique_ptr<double[]> m_so;      //!< (owned) Faces' stacking order
  std::unique_ptr<double[]> m_output;  //!< (owned) Output vertex coordinates

  std::vector<int> m_faceHints;  //!< Handles' face hints

public:
  PlasticDeformerData();
  ~PlasticDeformerData();

private:
  // Not copyable

  PlasticDeformerData(const PlasticDeformerData &);
  PlasticDeformerData &operator=(const PlasticDeformerData &);
};

//***********************************************************************************************
//    PlasticDeformerDataGroup  definition
//***********************************************************************************************

struct DVAPI PlasticDeformerDataGroup {
  std::unique_ptr<PlasticDeformerData[]>
      m_datas;  //!< (owned) The deformer datas array. One per mesh.
  std::vector<PlasticHandle>
      m_handles;  //!< Source handles (emanated from skeleton vertices).
  std::vector<TPointD>
      m_dstHandles;  //!< Corresponding destination handle positions

  int m_compiled;  //!< Whether compiled data is present about a certain
                   //! datatype.
  int m_upToDate;  //!< Whether updated  data is present about a certain
                   //! datatype.

  double m_outputFrame;  //!< The frame of current output values.
  //!< Value numeric_limits::max can be used to invalidate
  //!< deformation outputs
  TAffine m_skeletonAffine;  //!< The skeleton affine applied to each handle.
  //!< In case it changes, the deformation is automatically recompiled.

  double m_soMin,
      m_soMax;  //!< SO min and max across the whole group (useful while drawing
                //!< due to the unboundedness of the SO parameter)
  std::vector<std::pair<int, int>>
      m_sortedFaces;  //!< Pairs of face-mesh indices, by sorted stacking order.
  //!< This is the order that must be followed when drawing faces.
public:
  PlasticDeformerDataGroup();
  ~PlasticDeformerDataGroup();

private:
  // Not copyable

  PlasticDeformerDataGroup(const PlasticDeformerDataGroup &);
  PlasticDeformerDataGroup &operator=(const PlasticDeformerDataGroup &);
};

//***********************************************************************************************
//    PlasticDeformerStorage  declaration
//***********************************************************************************************

//! PlasticDeformerStorage is the global storage class for plastic deformers.
/*!
  PlasticDeformer models a mesh-deformer object that can be used together with
  texturing to create an interactive image-deformation technique. Direct use
  of PlasticDeformer instances is discouraged, as they can be considered as
  technical low-level interfaces to the deformation job.

\par Rationale

  The crucial reason behind the existance of this class lies in the
  necessity to cache PlasticDeformer instances to achieve real-time speed in
  the processing of part of its algorithms.
  \n\n
  In this context, caching is all about storing intermediate data so that they
  are recalculated only when strictly necessary.
  In PlasticDeformer's case, we can see 3 chained processing stages that need
  to be calculated before the final result is produced:

  \li <B> Initialization: <\B> A mesh is acquired
  \li <B> Compilation: <\B> The deformation's source domain data is acquired
  \li <B> Deformation: <\B> The deformation's destination domain data is
acquired

  Each step requires that the previous one has been computed. Obviously when,
  say, only the deformation's destination data change, there is no need to
process
  the first 2 steps - their cached data remains valid.

\par Interface

  This class provides the preferential high-level interface to plastic
deformers.
  \n\n
  The storage caches data about every requested deform specification, that is
kept
  until an explicit release command is issued. A data request is issued with a
call
  to the process() method, which accepts specification of a subset of
deform-related
  data to be returned, skipping unnecessary computations.
  \n\n
  The storage requires explicit content invalidation to know that is must
  recalculate subsequent requests. Invalidation is specific to changes in a
  PlasticSkeletonDeformation instance, and can be restrained to the destination
  (deformed) domain, or require more expensive invalidation.

\par Deformations and Data requests

  Cached data is uniquely associated with pairs of meshImage and deformed
skeletons,
  meaning that the same meshImage can be deformed by multiple skeletons, and
viceversa.
  \n\n
  A deformer data request needs an affine transform in input, which is intended
to
  map the skeleton deformation on top of the mesh (in mesh coordinates). The
storage
  <I> does not <\I> store a PlasticDeformer instance for each different affine
supplied,
  but rather \a invalidates the old data in case it changes.
  \n\n
  The mesh vertex coordinates resulting from a deformation will be stored in the
resulting
  PlasticDeformerData::m_output member of each entry of the output array, one
for each mesh
  of the input meshImage. The returned coordinates are intended in the mesh
reference.

\par Lifetime and Ownership

  This class <B> does not claim ownerhip <\B> of neither the mesh nor the
deformation that are
  specified in a deformer request - and <I> lifetime tracking <\I> of the
objects involved in
  plastic deforming is a burden of callers.
  \n\n
  This class requires that the destruction of either a mesh or a deformation be
signaled by a
  call to the appropriate release method in order to enforce the associated
deformers destruction.

  \warning No call to the appropriate release method when either a mesh or a
deformation which
           has requested a deformer is destroyed \b will result in a \b leak.
*/

class DVAPI PlasticDeformerStorage {
  class Imp;
  std::unique_ptr<Imp> m_imp;

public:
  enum DataType {
    NONE    = 0x0,
    HANDLES = 0x1,  // Handles data
    SO      = 0x4,  // Stacking Order data
    MESH    = 0x8,  // The deformed Mesh

    ALL = HANDLES | SO | MESH
  };

public:
  PlasticDeformerStorage();
  ~PlasticDeformerStorage();

  static PlasticDeformerStorage *instance();

  //! This function processes the specified meshImage-deformation pair,
  //! returning a DataGroup
  //! with the required data.
  /*!
\note This function \b caches all required data, so that subsequent requests
about the same
triplet can be sped up considerably in case of cache match.
*/
  const PlasticDeformerDataGroup *process(
      double frame, const TMeshImage *meshImage,
      const PlasticSkeletonDeformation *deformation, int skeletonId,
      const TAffine &deformationToMeshAffine, DataType dataType = ALL);

  //! Performs the specified deformation once, \b without caching data.
  /*!
This method allows the user to perform a single-shot Platic deformation, without
dealing with
caching issues.

\note Since caching is disabled, this method is comparably \a slower than its
cached counterpart,
    in case the same deformation is repeatedly invoked.
    It is meant to be used only in absence of user interaction.

\warning The returned pointer is owned by the \b caller, and must be manually
deleted
       when no longer needed.
*/
  static const PlasticDeformerDataGroup *processOnce(
      double frame, const TMeshImage *meshImage,
      const PlasticSkeletonDeformation *deformation, int skeletonId,
      const TAffine &deformationToMeshAffine, DataType dataType = ALL);

  //! Similarly to invalidateSkeleton(), for every deformer attached to the
  //! specified mesh.
  void invalidateMeshImage(const TMeshImage *meshImage,
                           int recompiledDataType = NONE);

  //! Schedules all stored deformers associated to the specified deformation for
  //! either re-deformation (stage 3 invalidation) or re-compilation (stage 2
  //! invalidation).
  /*!
Recompilation should be selected whenever the deformation has sustained source
domain
changes, such as a vertex addition or removal, or when the \a source skeletal
configuration has changed.

\note Recompilation is typically a slower process than the mere deformers
update.
Select it explicitly only when it truly needs to be done.
*/
  void invalidateSkeleton(const PlasticSkeletonDeformation *deformation,
                          int skeletonId, int recompiledDataType = NONE);

  //! Similarly to invalidateSkeleton(), for everything attached to the
  //! specified deformation.
  void invalidateDeformation(const PlasticSkeletonDeformation *deformation,
                             int recompiledDataType = NONE);

  //! Releases all deformers associated to the specified mesh image.
  void releaseMeshData(const TMeshImage *meshImage);

  //! Releases all deformers associated to the specified skeletal deformation.
  void releaseSkeletonData(const PlasticSkeletonDeformation *deformation,
                           int skeletonId);

  //! Releases all deformers associated to the specified skeletal deformation.
  void releaseDeformationData(const PlasticSkeletonDeformation *deformation);

  //! Releases all deformers, effectively returning the storage to its empty
  //! state.
  void clear();

private:
  //! Retrieves the group of deformers (one per mesh in the image) associated to
  //! the input
  //! pair, eventually creating one if none did exist.
  PlasticDeformerDataGroup *deformerData(
      const TMeshImage *meshImage,
      const PlasticSkeletonDeformation *deformation, int skeletonId);
};

#endif  // PLASTIDEFORMERSTORAGE_H
