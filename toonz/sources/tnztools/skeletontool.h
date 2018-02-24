#pragma once

#ifndef SKELETON_TOOL_INCLUDED
#define SKELETON_TOOL_INCLUDED

#include "tools/tool.h"
#include "tgeometry.h"
#include "tproperty.h"
#include "toonz/hook.h"
#include "toonz/skeleton.h"

#include <QCoreApplication>

namespace SkeletonSubtools {

class DragTool;
class CommandHandler;

class HookData {
public:
  int m_columnIndex;
  int m_hookId;  // 0=center (e.g. B,C,...), 1,2... = hook
  TPointD m_pos;
  std::string m_name;  // name visualized in the balloons
  bool m_isPivot;  // true if this is the level "pivot", i.e. the current handle
  HookData(TXsheet *xsh, int columnIndex, int hookId, const TPointD &pos);
  std::string getHandle() const { return m_hookId > 0 ? "H" + m_name : m_name; }
};

class MagicLink {
public:
  HookData m_h0, m_h1;
  double m_dist2;
  MagicLink(const HookData &h0, const HookData &h1, double dist2)
      : m_h0(h0), m_h1(h1), m_dist2(dist2) {}
  bool operator<(const MagicLink &link) const { return m_dist2 < link.m_dist2; }
};
}

class SkeletonTool : public TTool {
  Q_DECLARE_TR_FUNCTIONS(SkeletonTool)

  SkeletonSubtools::DragTool *m_dragTool;

  TPointD m_lastPos;
  TPointD m_curPos;
  TPointD m_firstPos;
  TPointD m_curCenter;
  TPointD m_parentProbe;
  bool m_parentProbeEnabled;

  bool m_active;
  bool m_firstTime;
  int m_device;

  TEnumProperty m_mode;
  TBoolProperty m_showOnlyActiveSkeleton;
  TBoolProperty m_globalKeyframes;

  TPropertyGroup m_prop;

  std::vector<SkeletonSubtools::MagicLink> m_magicLinks;

  std::set<int> m_temporaryPinnedColumns;

  int m_currentFrame;

  SkeletonSubtools::CommandHandler *m_commandHandler;

  // column currently selected, during a click&drag operation
  int m_otherColumn;
  TRectD m_otherColumnBBox;
  TAffine m_otherColumnBBoxAff;

  TPointD m_labelPos;
  std::string m_label;

public:
  SkeletonTool();
  ~SkeletonTool();

  ToolType getToolType() const { return TTool::ColumnTool; }

  void updateTranslation();  // QString localization stuff

  bool doesApply() const;  // ritorna vero se posso deformare l'oggetto corrente

  void onEnter() {}
  void leftButtonDown(const TPointD &pos, const TMouseEvent &);
  void leftButtonDrag(const TPointD &pos, const TMouseEvent &);
  void leftButtonUp(const TPointD &pos, const TMouseEvent &);
  void mouseMove(const TPointD &, const TMouseEvent &e);

  void onImageChanged() { invalidate(); }

  void reset() { m_temporaryPinnedColumns.clear(); }

  bool onPropertyChanged(std::string propertyName);

  void draw();

  void drawSkeleton(const Skeleton &skeleton, int row);
  void drawLevelBoundingBox(int frame, int columnIndex);
  void getImageBoundingBox(TRectD &bbox, TAffine &aff, int frame,
                           int columnIndex);
  void drawIKJoint(const Skeleton::Bone *bone);
  void drawJoint(const TPointD &p, bool current);
  void drawBone(const TPointD &a, const TPointD &b, bool selected);
  void drawIKBone(const TPointD &a, const TPointD &b);
  void drawMainGadget(const TPointD &center);
  void drawDrawingBrowser(const TXshCell &cell, const TPointD &center);

  void setParentProbe(const TPointD &parentProbe) {
    m_parentProbe        = parentProbe;
    m_parentProbeEnabled = true;
  }
  void resetParentProbe() { m_parentProbeEnabled = false; }

  void drawHooks();

  void computeMagicLinks();

  bool keyDown(QKeyEvent *event) override;

  void onActivate();
  void onDeactivate();

  int getMagicLinkCount() const { return (int)m_magicLinks.size(); }
  SkeletonSubtools::MagicLink getMagicLink(int index) const;

  void magicLink(int index);

  int getCursorId() const;

  // TRaster32P getToolIcon() const {return Pixmaps::arrow;}
  TPropertyGroup *getProperties(int targetType) { return &m_prop; }

  void updateMatrix() { setMatrix(getCurrentObjectParentMatrix()); }
  void addContextMenuItems(QMenu *menu);
  bool select(const TSelection *) { return false; }

  void togglePinnedStatus(int columnIndex, int frame, bool shiftPressed);

  void buildSkeleton(Skeleton &skeleton, int columnIndex);

  void setTemporaryPinnedColumns(const std::set<int> &tmp) {
    m_temporaryPinnedColumns = tmp;
  }
  bool isGlobalKeyframesEnabled() const;
};

#endif
