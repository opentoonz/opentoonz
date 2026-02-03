#pragma once

#ifndef PLASTICDEFORMER_H
#define PLASTICDEFORMER_H

#include <memory>

// TnzCore includes
#include "tgeometry.h"
#include "tmeshimage.h"

// TnzExt includes
#include "ext/plastichandle.h"

// STL includes
#include <vector>

// tcg includes
#include "tcg/tcg_list.h"

#undef DVAPI
#undef DVVAR
#ifdef TNZEXT_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//**********************************************************************************************
//    Plastic Deformation  declaration
//**********************************************************************************************

/*!
  The PlasticDeformer class implements an interactive mesh deformer.

\warning Objects of this class expect that the mesh and rigidities supplied on
construction
  remain \b constant throughout the deformer's lifetime. Deforming a changed
mesh is not supported
  and will typically result in a crash. Deforming a mesh whose vertex rigidities
have been
  \a deleted will result in a crash. Altering the rigidities results in
undefined deformations
  until the deformer is recompiled against them.
*/
class DVAPI PlasticDeformer {
  class Imp;
  std::unique_ptr<Imp> m_imp;

public:
  PlasticDeformer();
  ~PlasticDeformer();

  /*!
Returns whether the last compilation procedure succeeded, or it either failed
or was never invoked after the last initialize() call.
*/
  bool compiled() const;

  /*!
Initializes a deformation on the specified mesh object.
*/
  void initialize(const TTextureMeshP &mesh);

  /*!
\brief Compiles the deformer against a group of deformation handles, and returns
whether the procedure was successful.

\note Accepts hints about the mesh face containing each handle; the hinted face
is checked before scanning the whole mesh. In case hints are supplied, they will
be
returned with the correct face indices containing each handle.

\warning Requires a previous initialize() call. The compilation may legitimately
fail to process handle configurations that \a cannot result in suitable
deformations (eg, if more than 3 handles lie in the same mesh face).
*/
  bool compile(const std::vector<PlasticHandle> &handles, int *faceHints = 0);

  /*!
Applies the deformation specified with input handles deformed positions,
returning
the deformed mesh vertices positions.

\note In case the compilation step failed or was never invoked, this function
will silently return the original, undeformed mesh vertices.

\warning Requires previous compile() invocation.
*/
  void deform(const TPointD *dstHandlePos, double *dstVerticesCoords) const;

  /*!
Releases data from the initialize() step that is unnecessary during deform().

\warning Initialization data is still necessary to invoke compile(), which will
therefore need to be preceded by a new call to initialize().
*/
  void releaseInitializedData();

private:
  // Not copyable
  PlasticDeformer(const PlasticDeformer &);
  PlasticDeformer &operator=(const PlasticDeformer &);
};

#endif  // PLASTICDEFORMER_H
