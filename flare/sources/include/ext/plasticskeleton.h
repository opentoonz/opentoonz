#pragma once

#ifndef PLASTICSKELETON_H
#define PLASTICSKELETON_H

#include <memory>

// TnzCore includes
#include "tsmartpointer.h"
#include "tpersist.h"
#include "tmeshimage.h"  // add imported template
                         // instance tcg::Vertex<TPointD> (for Windows dll)

// TnzExt includes
#include "plastichandle.h"

// Boost includes
#include <boost/config.hpp>

// Qt includes
#include <QString>

#if (defined TNZEXT_EXPORTS && !defined INCLUDE_HPP)
#define REMOVE_INCLUDE_HPP
#define INCLUDE_HPP  // Exporting templates needs full instantiation
#endif

#include "tcg_wrap.h"
#include "tcg/tcg_vertex.h"
#include "tcg/tcg_edge.h"
#include "tcg/tcg_face.h"
#include "tcg/tcg_mesh.h"

#ifdef REMOVE_INCLUDE_HPP
#undef INCLUDE_HPP
#endif

#undef DVAPI
#undef DVVAR
#ifdef TNZEXT_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//==========================================================================

//    Forward declarations

class PlasticSkeletonDeformation;

//==========================================================================

//************************************************************************************
//    PlasticSkeletonVertex  declaration
//************************************************************************************

//! PlasticSkeletonVertex is the vertex of a plastic skeleton object.
struct DVAPI PlasticSkeletonVertex final : public tcg::Vertex<TPointD>,
                                           public TPersist {
  PERSIST_DECLARATION(PlasticSkeletonVertex)

private:
  friend class PlasticSkeleton;

  QString m_name;  //!< Vertex name
  int m_number;    //!< Vertex \a number - assigned persistently upon addition
  //!< in a skeleton
  int m_parent;  //!< Index of the parent vertex in the skeleton

public:
  // Tool data

  double m_minAngle, m_maxAngle;  //!< Minimum and maximum accepted angles when
                                  //! updating the vertex
  //!< position with the mouse. In degrees.
public:
  // Handle data

  bool m_interpolate;  //!< Whether the vertex needs to be interpolated (see
                       //! plasticdeformer.h)

public:
  PlasticSkeletonVertex();
  explicit PlasticSkeletonVertex(const TPointD &pos);

  operator PlasticHandle() const;
  operator TPointD() const { return P(); }

  const QString &name() const { return m_name; }
  int number() const { return m_number; }  // Should be removed

  int parent() const { return m_parent; }

  void saveData(TOStream &os) override;
  void loadData(TIStream &is) override;
};

//************************************************************************************
//    PlasticSkeleton  declaration
//************************************************************************************

class DVAPI PlasticSkeleton final
    : public TSmartObject,
      public tcg::Mesh<PlasticSkeletonVertex, tcg::Edge, tcg::FaceN<3>>,
      public TPersist {
  DECLARE_CLASS_CODE
  PERSIST_DECLARATION(PlasticSkeleton)

private:
  class Imp;
  std::unique_ptr<Imp> m_imp;

public:
  typedef tcg::Mesh<PlasticSkeletonVertex, tcg::Edge, tcg::FaceN<3>> mesh_type;

public:
  PlasticSkeleton();
  PlasticSkeleton(const PlasticSkeleton &other);
  ~PlasticSkeleton();

  PlasticSkeleton &operator=(const PlasticSkeleton &other);

  int parentVertex(int v) { return vertex(v).m_parent; }

  void moveVertex(int v, const TPointD &pos);  //!< Moves a vertex to the
                                               //! specified position, informing
  //! associated deformers
  int addVertex(const PlasticSkeletonVertex &vx,
                int parent);  //!< Adds a vertex to the skeleton
  int insertVertex(const PlasticSkeletonVertex &vx,
                   int e);  //!< Inserts a vertex splitting an existing edge
  //!< \note Inserted Vertex will connect to parent first
  int insertVertex(
      const PlasticSkeletonVertex &vx,
      int parent,  //!< Generalization of vertex addition/insertion,
      const std::vector<int>
          &children);  //!< it is useful as inverse of vertex removal.
  void removeVertex(
      int v);  //!< Removes a vertex, reattaching all its children to parent
  void clear();
  void squeeze();

  bool setVertexName(int v, const QString &name);

  void saveData(TOStream &os) override;
  void loadData(TIStream &is) override;

  // Utility functions

  int closestVertex(const TPointD &pos, double *distance = 0) const;
  int closestEdge(const TPointD &pos, double *distance = 0) const;

  std::vector<PlasticHandle> verticesToHandles() const;

public:
  // RValues-related functions
  PlasticSkeleton(PlasticSkeleton &&other);
  PlasticSkeleton &operator=(PlasticSkeleton &&other);

private:
  friend class PlasticSkeletonDeformation;  // Skeleton deformations can
                                            // register to be notified

  void addListener(PlasticSkeletonDeformation *deformation);
  void removeListener(PlasticSkeletonDeformation *deformation);
};

//===============================================================================

#ifdef _WIN32
#ifndef TFX_EXPORTS
template class DVAPI TSmartPointerT<PlasticSkeleton>;
#endif
#endif

typedef TSmartPointerT<PlasticSkeleton> PlasticSkeletonP;

#endif  // PLASTICSKELETON_H
