#pragma once

#ifndef PLASTICHANDLE_H
#define PLASTICHANDLE_H

// TnzCore includes
#include "tgeometry.h"

//============================================================

//    Forward declarations

class TTextureMesh;

//============================================================

//**********************************************************************************************
//    Plastic Handle  definition
//**********************************************************************************************

//! The PlasticHandle class models the geometrical information of the
//! neighbourhood of a point
//! that can be deformed with a PlasticDeformer instance.
/*!
  The PlasticDeformer class allows users to deform interactively a mesh object.
A Plastic Handle
  models a point of the mesh that can be \a manually displaced by the user to
deform the mesh object.
\n\n
  It contains the initial position of the point to be displaced; eventually,
further implementation
  may include a '<I>direction<\I>' that can be rotated by the user to force the
neighborhood into
  a specific rotational component.
*/
struct PlasticHandle {
  TPointD m_pos;  //!< Handle position
  // TPointD m_dir;                                    //!< Handle 'direction',
  // used to specify the
  //!< local rotational component

  bool m_interpolate;  //!< Whether the handle should be interpolated

  // Interpolable data (unused by PlasticDeformer)

  double m_so;  //!< Local faces stacking order

public:
  PlasticHandle() : m_interpolate(true), m_so(0.0) {}
  explicit PlasticHandle(const TPointD &pos)
      : m_pos(pos), m_interpolate(true), m_so(0.0) {}
  ~PlasticHandle() {}
};

//**********************************************************************************************
//    Plastic Handle  utility functions
//**********************************************************************************************

/*!
  Fills the distances array with the distances of mesh vertices from the
  specified pos.
  Returns false in case the specified position is not inside the mesh; in this
  case, the
  distances array remains untouched.
*/
bool buildDistances(float *distances, const TTextureMesh &mesh,
                    const TPointD &pos, int *faceHint = 0);

//---------------------------------------------------------------------------------

/*!
  Interpolates the handles' stacking order (SO) values along the specified mesh.

  The SO interpolant base function is exponential to ensure that values'
  influence decreases
  by orders of magnitude with the distance (therefore making only the closest
  interpolants
  relevant). The interpolant value falls at 1e-8 at a distance of the mesh's
  bbox.
*/
void buildSO(double *so, const TTextureMesh &mesh,
             const std::vector<PlasticHandle> &handles, int *faceHints = 0);

#endif  // PLASTICHANDLE_H
