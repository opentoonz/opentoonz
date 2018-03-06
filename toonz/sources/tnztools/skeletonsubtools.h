#pragma once

#ifndef SKELETON_SUBTOOLS_INCLUDED
#define SKELETON_SUBTOOLS_INCLUDED

#include "tools/tool.h"
#include "toonz/txsheet.h"
#include "toonz/skeleton.h"
#include "toonz/stageobjectutil.h"
#include "toonz/ikengine.h"

#include <QObject>

class SkeletonTool;

namespace SkeletonSubtools {

//---------------------------------------------------------

class DragTool {
  SkeletonTool *m_tool;

public:
  DragTool(SkeletonTool *tool) : m_tool(tool) {}
  virtual ~DragTool() {}

  virtual void draw() {}

  virtual void leftButtonDown(const TPointD &pos, const TMouseEvent &) = 0;
  virtual void leftButtonDrag(const TPointD &pos, const TMouseEvent &) = 0;
  virtual void leftButtonUp(const TPointD &pos, const TMouseEvent &)   = 0;

  SkeletonTool *getTool() const { return m_tool; }
};

//---------------------------------------------------------

class DragCenterTool final : public DragTool {
  TStageObjectId m_objId;
  int m_frame;

  TPointD m_firstPos;
  TPointD m_oldCenter;
  TPointD m_center;
  TAffine m_affine;

public:
  DragCenterTool(SkeletonTool *tool);

  void leftButtonDown(const TPointD &pos, const TMouseEvent &) override;
  void leftButtonDrag(const TPointD &pos, const TMouseEvent &) override;
  void leftButtonUp(const TPointD &pos, const TMouseEvent &) override;
};

//---------------------------------------------------------

class DragChannelTool : public DragTool {
protected:
  TStageObjectValues m_before, m_after;
  bool m_dragged;

public:
  DragChannelTool(SkeletonTool *tool, TStageObject::Channel a0);
  DragChannelTool(SkeletonTool *tool, TStageObject::Channel a0,
                  TStageObject::Channel a1);

  void start();

  TPointD getCenter();

  double getOldValue(int index) const { return m_before.getValue(index); }
  double getValue(int index) const { return m_after.getValue(index); }
  void setValue(double v) {
    m_after.setValue(v);
    m_after.applyValues();
  }
  void setValues(double v0, double v1) {
    m_after.setValues(v0, v1);
    m_after.applyValues();
  }

  void leftButtonUp(const TPointD &pos, const TMouseEvent &) override;
};

//---------------------------------------------------------

class DragPositionTool final : public DragChannelTool {
  TPointD m_firstPos;

public:
  DragPositionTool(SkeletonTool *tool);

  void leftButtonDown(const TPointD &pos, const TMouseEvent &) override;
  void leftButtonDrag(const TPointD &pos, const TMouseEvent &e) override;
};

//---------------------------------------------------------

class DragRotationTool final : public DragChannelTool {
  TPointD m_lastPos;
  TPointD m_center;
  bool m_snapped;

public:
  DragRotationTool(SkeletonTool *tool, bool snapped);

  void leftButtonDown(const TPointD &pos, const TMouseEvent &) override;
  void leftButtonDrag(const TPointD &pos, const TMouseEvent &) override;
};

//---------------------------------------------------------

class ParentChangeTool final : public DragTool {
  TTool::Viewer *m_viewer;
  TPointD m_firstWinPos;
  TPointD m_lastPos, m_lastPos2;
  TPointD m_center;
  int m_index;
  int m_handle;
  bool m_snapped;
  int m_mode;
  TAffine m_affine;
  TStageObjectId m_objId;
  TPointD m_oldCenter, m_oldOffset;

  struct Peer {
    TPointD m_pos;
    int m_columnIndex;
    int m_handle;  // 0=centerB,-1,-2,....=H1,H2...
    int m_name;    // per glPushName()
  };

  struct Element {
    int m_columnIndex;
    TRectD m_bbox;
    TAffine m_aff;
    std::vector<Peer> m_peers;
  };
  std::map<int, Element> m_elements;

  double m_pixelSize;

public:
  ParentChangeTool(SkeletonTool *tool, TTool::Viewer *viewer);

  void leftButtonDown(const TPointD &pos, const TMouseEvent &e) override;
  void leftButtonDrag(const TPointD &pos, const TMouseEvent &e) override;
  void leftButtonUp(const TPointD &pos, const TMouseEvent &e) override;

  void draw() override;
};

//---------------------------------------------------------

class IKToolUndo;

class IKTool final : public DragTool {
  TTool::Viewer *m_viewer;
  TPointD m_pos;
  Skeleton *m_skeleton;
  int m_columnIndex;
  IKEngine m_engine;
  bool m_valid;
  TAffine m_footPlacement, m_firstFootPlacement;
  TStageObject *m_foot, *m_firstFoot;
  bool m_IHateIK;
  IKToolUndo *m_undo;

  struct Joint {
    Skeleton::Bone *m_bone, *m_prevBone;
    int m_prevColumnIndex;
    double m_angleOffset;
    double m_sign;
    bool m_active;
    TStageObjectValues m_oldValues;
    Joint()
        : m_bone(0)
        , m_prevBone(0)
        , m_angleOffset(0)
        , m_sign(1)
        , m_active(true) {}
    Joint(Skeleton::Bone *bone, Skeleton::Bone *prevBone, double sign)
        : m_bone(bone)
        , m_prevBone(prevBone)
        , m_angleOffset(0)
        , m_sign(sign)
        , m_active(true) {}
  };
  std::vector<Joint> m_joints;
  bool isParentOf(int columnIndex, int childColumnIndex) const;

public:
  IKTool(SkeletonTool *tool, TTool::Viewer *viewer, Skeleton *skeleton,
         int columnIndex);
  ~IKTool();

  void initEngine(const TPointD &pos);
  void storeOldValues();
  void computeIHateIK();
  void setAngleOffsets();
  void apply();

  void leftButtonDown(const TPointD &, const TMouseEvent &e) override;
  void leftButtonDrag(const TPointD &, const TMouseEvent &e) override;
  void leftButtonUp(const TPointD &, const TMouseEvent &e) override;

  void draw() override;
};

//---------------------------------------------------------

class ChangeDrawingTool final : public DragTool {
  int m_oldY;
  int m_dir;
  TUndo *m_undo;

public:
  ChangeDrawingTool(SkeletonTool *tool, int d);

  void leftButtonDown(const TPointD &, const TMouseEvent &e) override;
  void leftButtonDrag(const TPointD &, const TMouseEvent &e) override;
  void leftButtonUp(const TPointD &, const TMouseEvent &e) override;

  bool changeDrawing(int delta);
};

//---------------------------------------------------------

class CommandHandler final : public QObject {
  Q_OBJECT
  Skeleton *m_skeleton;
  std::set<int> *m_tempPinnedSet;

public:
  CommandHandler();
  ~CommandHandler();
  void setTempPinnedSet(std::set<int> *tempPinnedSet) {
    m_tempPinnedSet = tempPinnedSet;
  }

  void setSkeleton(Skeleton *skeleton);
public slots:
  void clearPinnedRanges();
};

}  // namespace SkeletonSubtools

#endif
