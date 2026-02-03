

// TnzLib includes
#include "toonz/txsheet.h"
#include "toonz/txshcell.h"
#include "toonz/hook.h"
#include "toonz/dpiscale.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/stage.h"

#include "xshhandlemanager.h"

//*******************************************************************************
//    Local namespace
//*******************************************************************************

namespace {

// Returns the sum of each pass hook between fid and precFid.
TPointD forwardPass(const TFrameId &fid, const TFrameId &precFid,
                    TXshSimpleLevel *sl, Hook *hook) {
  TPointD delta;
  std::vector<TFrameId> levelFids;
  sl->getFids(levelFids);

  int f, fCount = levelFids.size();
  for (f = 0; f != fCount; ++f) {
    const TFrameId &levelFid = levelFids[f];

    if (levelFid < precFid) continue;

    if (levelFid >= fid) break;

    if (hook->isKeyframe(levelFid))
      delta += hook->getAPos(levelFid) - hook->getBPos(levelFid);
  }

  return delta;
}

//-----------------------------------------------------------------------------
// Returns subtraction of each pass hook between fid and precFid.
TPointD backwardPass(const TFrameId &fid, const TFrameId &precFid,
                     TXshSimpleLevel *sl, Hook *hook) {
  TPointD delta;
  std::vector<TFrameId> levelFids;
  sl->getFids(levelFids);

  int f, fCount = levelFids.size();
  for (f = 0; f != fCount; ++f) {
    const TFrameId &levelFid = levelFids[f];

    if (levelFid < fid) continue;

    if (levelFid >= precFid) break;

    if (hook->isKeyframe(levelFid))
      delta -= hook->getAPos(levelFid) - hook->getBPos(levelFid);
  }

  return delta;
}

//-----------------------------------------------------------------------------
// Compute pass hook between fid and precFid
TPointD computePassHook(const TFrameId &fid, const TFrameId &precFid,
                        TXshSimpleLevel *sl, Hook *hook) {
  if (fid < precFid && (precFid.getNumber() - fid.getNumber()) <= 2)
    return backwardPass(fid, precFid, sl, hook);

  return forwardPass(fid, precFid, sl, hook);
}

}  // namespace

//*******************************************************************************
//    XshHandleManager  implementation
//*******************************************************************************

TPointD XshHandleManager::getHandlePos(const TStageObjectId &id,
                                       const std::string &handle,
                                       int row) const {
  static const double unit = 8.0;

  assert(m_xsh->getScene());

  if (handle == "")
    return TPointD();

  else if (handle[0] == 'H' && handle.length() > 1) {
    // Hook port case

    if (!id.isColumn()) return TPointD();

    int hookIndex = 0;
    {
      int h, hLength = handle.length();
      for (h = 1; h != hLength; ++h)
        hookIndex = hookIndex * 10 + (handle[h] - '0');
    }

    TStageObject *obj = m_xsh->getStageObject(id);
    if (const PlasticSkeletonDeformationP &def =
            obj->getPlasticSkeletonDeformation()) {
      int skelId = def->skeletonId(row);

      PlasticSkeleton skel;
      def->storeDeformedSkeleton(skelId, row, skel);

      int v = def->vertexIndex(hookIndex, skelId);
      return (v >= 0) ? TScale(1.0 / Stage::inch) * skel.vertex(v).P()
                      : TPointD();
    }

    --hookIndex;

    int col = id.getIndex();
    TXshCell cell(m_xsh->getCell(row, col));

    TXshLevel *xl = cell.m_level.getPointer();
    if (!xl) return TPointD();

    TXshSimpleLevel *sl = xl->getSimpleLevel();
    if (!sl) return TPointD();

    Hook *hook = sl->getHookSet()->getHook(hookIndex);
    if (!hook) return TPointD();

    TFrameId fid(cell.m_frameId);

    TPointD pos = hook->getAPos(fid) * (1.0 / Stage::inch);
    TPointD delta;

    for (int r = row - 1; r >= 0; --r) {
      cell = m_xsh->getCell(r, col);
      if (cell.m_level.getPointer() != xl) break;

      const TFrameId &precFid = cell.m_frameId;
      delta += computePassHook(fid, precFid, sl, hook);
      fid = precFid;
    }

    pos += delta * (1.0 / Stage::inch);
    pos = getDpiAffine(sl, cell.m_frameId, true) * pos;

    return pos;
  } else if ('A' <= handle[0] && handle[0] <= 'Z')
    return TPointD(unit * (handle[0] - 'B'), 0);
  else if ('a' <= handle[0] && handle[0] <= 'z')
    return TPointD(0.5 * unit * (handle[0] - 'b'), 0);
  else
    return TPointD();
}
