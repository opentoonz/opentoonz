#pragma once

#ifndef TFX_ATTRIBUTES_INCLUDED
#define TFX_ATTRIBUTES_INCLUDED

#include "tgeometry.h"
#include <QStack>
#include <QList>

#undef DVAPI
#undef DVVAR
#ifdef TFX_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

class DVAPI TFxAttributes {
  int m_id;
  TPointD m_dagNodePos;
  bool m_enabled;
  bool m_speedAware;
  bool m_isOpened;
  TPointD m_speed;
  // A stack is used to manage subgroups.
  QStack<int> m_groupId;
  QStack<std::wstring> m_groupName;
  int m_passiveCacheDataIdx;

  int m_groupSelector;

  /*-- Get object trajectory data for MotionBlur and other Fxs --*/
  QList<TPointD> m_motionPoints;
  TAffine m_motionAffine[2];
  // to maintain backward compatibility in the fx
  int m_fxVersion;

  bool m_hasGlobalControl;
  double m_globalIntensity;

public:
  TFxAttributes();
  ~TFxAttributes();

  void setId(int id) { m_id = id; }
  int getId() { return m_id; }

  void setDagNodePos(const TPointD &pos);
  TPointD getDagNodePos() const { return m_dagNodePos; }

  bool isEnabled() const { return m_enabled; }
  void enable(bool on) { m_enabled = on; }

  void setIsSpeedAware(bool yes) { m_speedAware = yes; }
  bool isSpeedAware() const { return m_speedAware; }

  void setSpeed(TPointD &speed) { m_speed = speed; }
  TPointD getSpeed() const { return m_speed; }

  void setIsOpened(bool value) { m_isOpened = value; }
  bool isOpened() { return m_isOpened; }

  int &passiveCacheDataIdx() { return m_passiveCacheDataIdx; }

  void setMotionPoints(QList<TPointD> motionPoints) {
    m_motionPoints = motionPoints;
  }
  QList<TPointD> getMotionPoints() { return m_motionPoints; }

  void setMotionAffines(TAffine aff_Before, TAffine aff_After) {
    m_motionAffine[0] = aff_Before;
    m_motionAffine[1] = aff_After;
  }
  void getMotionAffines(TAffine &aff_Before, TAffine &aff_After) {
    aff_Before = m_motionAffine[0];
    aff_After  = m_motionAffine[1];
  }

  void setFxVersion(int version) { m_fxVersion = version; }
  int getFxVersion() const { return m_fxVersion; };

  void setHasGlobalControl(bool yes) { m_hasGlobalControl = yes; }
  bool hasGlobalControl() const { return m_hasGlobalControl; }
  void setGlobalIntensity(double val) { m_globalIntensity = val; }
  double getGlobalIntensity() { return m_globalIntensity; }
  // Group management

  int setGroupId(int value);
  void setGroupId(int value, int position);
  int getGroupId();
  QStack<int> getGroupIdStack();
  void removeGroupId(int position);
  int removeGroupId();

  void removeFromAllGroup();

  bool isGrouped();
  bool isContainedInGroup(int groupId);

  void setGroupName(const std::wstring &name, int position = -1);
  std::wstring getGroupName(bool fromEditor);
  QStack<std::wstring> getGroupNameStack();
  int removeGroupName(bool fromEditor);
  void removeGroupName(int position = -1);

  bool editGroup();
  bool isGroupEditing();
  void closeEditingGroup(int groupId);
  int getEditingGroupId();
  std::wstring getEditingGroupName();
  void closeAllGroups();
};

#endif
