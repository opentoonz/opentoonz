

#include "toonz/skeleton.h"
#include "toonz/tstageobject.h"
#include "toonz/tpinnedrangeset.h"
#include "toonz/tstageobjectkeyframe.h"
#include "toonz/txsheet.h"
#include "toonz/stage.h"

namespace {

TStageObjectId getAncestor(TXsheet *xsh, TStageObjectId id) {
  assert(id.isColumn());
  TStageObjectId parentId;
  while (parentId = xsh->getStageObjectParent(id), parentId.isColumn())
    id = parentId;
  return id;
}

}  // namespace

//=============================================================================

int Skeleton::Bone::getColumnIndex() const {
  return m_stageObject->getId().getIndex();
}

//-----------------------------------------------------------------------------

void Skeleton::Bone::setParent(Bone *parent) {
  if (m_parent != parent) {
    m_parent = parent;
    parent->m_children.push_back(this);
  }
}

//=============================================================================

Skeleton::Skeleton() : m_rootBone(0) {}

//-----------------------------------------------------------------------------

Skeleton::~Skeleton() { clearPointerContainer(m_bones); }

//-----------------------------------------------------------------------------

void Skeleton::clear() {
  clearPointerContainer(m_bones);
  m_rootBone = 0;
}

//-----------------------------------------------------------------------------

void Skeleton::build(TXsheet *xsh, int row, int col,
                     const std::set<int> &tempPinnedColumns) {
  // clear
  m_rootBone = 0;
  clearPointerContainer(m_bones);

  // antenato (colonna) della colonna corrente
  TStageObjectId ancestorId = getAncestor(xsh, TStageObjectId::ColumnId(col));

  // costruisco le "ossa"
  std::map<TStageObjectId, Bone *> boneTable;
  int columnCount = xsh->getColumnCount();
  int pinnedCount = 0;
  for (int i = 0; i < columnCount; i++) {
    TStageObjectId columnId(TStageObjectId::ColumnId(i));
    if (getAncestor(xsh, columnId) != ancestorId) continue;

    TAffine aff         = xsh->getPlacement(columnId, row);
    TPointD center      = Stage::inch * xsh->getCenter(columnId, row);
    TPointD p           = aff * center;
    TStageObject *obj   = xsh->getStageObject(columnId);
    Bone *bone          = new Bone(obj, p);
    boneTable[columnId] = bone;
    m_bones.push_back(bone);
    if (columnId == ancestorId) m_rootBone = bone;

    if (obj->getPinnedRangeSet()->isPinned(row)) {
      pinnedCount++;
      bone->setPinnedStatus(Bone::PINNED);
    } else if (tempPinnedColumns.count(i) > 0)
      bone->setPinnedStatus(Bone::TEMP_PINNED);
  }

  // if no bone is pinned then the root is considered pinned
  if (pinnedCount == 0 && m_rootBone) m_rootBone->setPinnedStatus(Bone::PINNED);

  // The skeleton could possibly be empty
  if (boneTable.empty()) {
    assert(!m_rootBone);
    return;
  }

  // assign parents
  std::map<TStageObjectId, Bone *>::iterator it, sit;
  for (it = boneTable.begin(); it != boneTable.end(); ++it) {
    sit = boneTable.find(xsh->getStageObjectParent(it->first));
    if (sit != boneTable.end()) it->second->setParent(sit->second);
  }

  // select the "active chain", i.e. the starting bone (columnIndex=col) and the
  // ancestors
  it = boneTable.find(TStageObjectId::ColumnId(col));
  if (it != boneTable.end()) {
    Bone *bone = it->second;
    while (bone) {
      bone->select();
      bone = bone->getParent();
    }
  }
}

//-----------------------------------------------------------------------------

Skeleton::Bone *Skeleton::getBone(int index) const {
  assert(0 <= index && index < (int)m_bones.size());
  return m_bones[index];
}

//-----------------------------------------------------------------------------

Skeleton::Bone *Skeleton::getBoneByColumnIndex(int columnIndex) const {
  for (int i = 0; i < (int)m_bones.size(); i++)
    if (m_bones[i]->getColumnIndex() == columnIndex) return m_bones[i];
  return 0;
}

//-----------------------------------------------------------------------------

bool Skeleton::isIKEnabled() const {
  return m_rootBone &&
         m_rootBone->getStageObject()->getStatus() == TStageObject::IK;
}

//-----------------------------------------------------------------------------

bool Skeleton::hasPinnedRanges() const {
  for (int i = 0; i < getBoneCount(); i++) {
    TStageObject *obj = getBone(i)->getStageObject();
    if (obj->getPinnedRangeSet()->getRangeCount() > 0) return true;
  }
  return false;
}

//-----------------------------------------------------------------------------

void Skeleton::clearAllPinnedRanges() {
  for (int i = 0; i < getBoneCount(); i++) {
    TStageObject *obj = getBone(i)->getStageObject();
    obj->getPinnedRangeSet()->removeAllRanges();
    obj->invalidate();
  }
}
