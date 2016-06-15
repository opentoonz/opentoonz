#pragma once

#ifndef SKELETON_INCLUDED
#define SKELETON_INCLUDED

#include "tgeometry.h"
#include <set>

#undef DVAPI
#undef DVVAR
#ifdef TOONZLIB_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

class TXsheet;
class TStageObject;

//! The Skeleton is the data structure used by the Skeleton Tool

class DVAPI Skeleton {
public:
  class DVAPI Bone {
  public:
    enum PinnedStatus { FREE, PINNED, TEMP_PINNED };

    Bone(TStageObject *pegbar, const TPointD &center)
        : m_parent(0)
        , m_stageObject(pegbar)
        , m_center(center)
        , m_selected(false)
        , m_pinnedStatus(FREE) {}

    void setParent(Bone *parent);
    Bone *getParent() const { return m_parent; }

    int getChildCount() const { return (int)m_children.size(); }
    Bone *getChild(int index) const {
      return 0 <= index && index < getChildCount() ? m_children[index] : 0;
    }

    TStageObject *getStageObject() const { return m_stageObject; }
    int getColumnIndex() const;
    const TPointD &getCenter() const { return m_center; }

    void select(bool selected = true) { m_selected = selected; }
    bool isSelected() const { return m_selected; }

    PinnedStatus getPinnedStatus() const { return m_pinnedStatus; }
    void setPinnedStatus(PinnedStatus pinnedStatus) {
      m_pinnedStatus = pinnedStatus;
    }

  private:
    Bone *m_parent;
    std::vector<Bone *> m_children;
    TStageObject *m_stageObject;
    TPointD m_center;  // position of the rotation center of the stageobject,
                       // i.e. the "joint"
    bool m_selected;   // true if the bone belongs to the active chain (i.e.
                       // handle <-> pinned ponts)
    PinnedStatus m_pinnedStatus;

  };  // class Bone

private:
  std::vector<Bone *> m_bones;
  Bone *m_rootBone;

public:
  Skeleton();
  ~Skeleton();

  //! create a skeleton containing 'col'.
  //! tempPinnedColumns contains the column indices of the temporarly pinned
  //! bones
  void build(TXsheet *xsh, int row, int col,
             const std::set<int> &tempPinnedColumns = std::set<int>());

  //! reset the skeleton
  void clear();

  int getBoneCount() const { return m_bones.size(); }
  Bone *getBone(int index) const;
  Bone *getBoneByColumnIndex(int columnIndex) const;

  //! the root is the ancestor of each bone in the skeleton
  Bone *getRootBone() const { return m_rootBone; }

  //! helper functions. see TPinnedRangeSet
  //! the root status is TStageObject::IK
  bool isIKEnabled() const;

  //! Some bone has a not empty pinned range
  bool hasPinnedRanges() const;

  void clearAllPinnedRanges();
};

#endif
