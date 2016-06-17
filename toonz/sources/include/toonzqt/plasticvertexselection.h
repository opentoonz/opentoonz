#pragma once

#ifndef PLASTICVERTEXSELECTION_H
#define PLASTICVERTEXSELECTION_H

// TnzQt includes
#include "toonzqt/multipleselection.h"

#undef DVAPI
#undef DVVAR
#ifdef TOONZQT_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//**********************************************************************
//    PlasticVertexSelection  definition
//**********************************************************************

/*!
  \brief    Represents a selection of vertices from the Plastic tool.

  \details  A Plastic vertex selection is a particular case of
            IndexesSelection that also stores the id of the
            skeleton containing the indexed vertexes.
*/

class DVAPI PlasticVertexSelection : public MultipleSelection<int> {
  typedef MultipleSelection<int> base_type;

  int m_skelId;  //!< Skeleton Id containing the vertex

public:
  PlasticVertexSelection(int vIdx = -1, int skelId = -1) : m_skelId(skelId) {
    if (vIdx >= 0) m_objects.push_back(vIdx);
  }

  //! Constructs from a list of vertex indices
  //! \warning The user is responsible for ensuring that the list #does not
  //! contain negative indices#
  PlasticVertexSelection(const std::vector<int> &vIdxs, int skelId = -1)
      : base_type(vIdxs), m_skelId(skelId) {}

  void selectNone() {
    m_skelId = -1;
    base_type::selectNone();
  }

  operator int() const {
    return (objects().size() == 1) ? objects().front() : -1;
  }

  int skeletonId() const { return m_skelId; }
  int &skeletonId() { return m_skelId; }
};

#endif  // PLASTICVERTEXSELECTION_H
