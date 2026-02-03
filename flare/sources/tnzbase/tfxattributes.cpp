

#include "tfxattributes.h"
#include "tconst.h"

//----------------------------------------------------------------------

TFxAttributes::TFxAttributes()
    : m_id(0)
    , m_dagNodePos(TConst::nowhere)
    , m_enabled(true)
    , m_speedAware(false)
    , m_isOpened(false)
    , m_speed()
    , m_groupSelector(-1)
    , m_passiveCacheDataIdx(-1)
    , m_fxVersion(1)
    , m_hasGlobalControl(false) {}

//----------------------------------------------------------------------

TFxAttributes::~TFxAttributes() {}

//----------------------------------------------------------------------

void TFxAttributes::setDagNodePos(const TPointD &pos) { m_dagNodePos = pos; }

//----------------------------------------------------------------------

int TFxAttributes::setGroupId(int value) {
  m_groupSelector++;
  m_groupId.insert(m_groupSelector, value);
  return m_groupSelector;
}

//----------------------------------------------------------------------

void TFxAttributes::setGroupId(int value, int position) {
  assert(position >= 0 && position <= m_groupId.size());
  m_groupId.insert(position, value);
  if (m_groupSelector + 1 >= position) m_groupSelector++;
}

//----------------------------------------------------------------------

int TFxAttributes::getGroupId() {
  return m_groupId.isEmpty() || m_groupSelector < 0 ||
                 m_groupSelector >= m_groupId.size()
             ? 0
             : m_groupId[m_groupSelector];
}

//----------------------------------------------------------------------

QStack<int> TFxAttributes::getGroupIdStack() { return m_groupId; }

//----------------------------------------------------------------------

void TFxAttributes::removeGroupId(int position) {
  if (!isGrouped()) return;
  assert(position >= 0 && position <= m_groupId.size());
  m_groupId.remove(position);
  if (m_groupSelector + 1 >= position && m_groupSelector > -1)
    m_groupSelector--;
}

//----------------------------------------------------------------------

int TFxAttributes::removeGroupId() {
  m_groupId.remove(m_groupSelector);
  if (m_groupSelector > -1) m_groupSelector--;
  return m_groupSelector + 1;
}

//----------------------------------------------------------------------

bool TFxAttributes::isGrouped() { return !m_groupId.isEmpty(); }

//----------------------------------------------------------------------

bool TFxAttributes::isContainedInGroup(int groupId) {
  return m_groupId.contains(groupId);
}

//----------------------------------------------------------------------

void TFxAttributes::setGroupName(const std::wstring &name, int position) {
  int groupSelector = position < 0 ? m_groupSelector : position;
  assert(groupSelector >= 0 && groupSelector <= m_groupName.size());
  m_groupName.insert(groupSelector, name);
}

//----------------------------------------------------------------------

std::wstring TFxAttributes::getGroupName(bool fromEditor) {
  int groupSelector = fromEditor ? m_groupSelector + 1 : m_groupSelector;
  return m_groupName.isEmpty() || groupSelector < 0 ||
                 groupSelector >= m_groupName.size()
             ? L""
             : m_groupName[groupSelector];
}

//----------------------------------------------------------------------

QStack<std::wstring> TFxAttributes::getGroupNameStack() { return m_groupName; }

//----------------------------------------------------------------------

int TFxAttributes::removeGroupName(bool fromEditor) {
  int groupSelector = fromEditor ? m_groupSelector + 1 : m_groupSelector;
  if (!isGrouped()) return -1;
  assert(groupSelector >= 0 && groupSelector <= m_groupName.size());
  m_groupName.remove(groupSelector);
  return groupSelector;
}

//----------------------------------------------------------------------

void TFxAttributes::removeGroupName(int position) {
  int groupSelector = position < 0 ? m_groupSelector : position;
  assert(groupSelector >= 0 && groupSelector <= m_groupName.size());
  m_groupName.remove(groupSelector);
}

//----------------------------------------------------------------------

bool TFxAttributes::editGroup() {
  return (m_groupSelector < 0) ? false : (--m_groupSelector, true);
}

//----------------------------------------------------------------------

bool TFxAttributes::isGroupEditing() {
  return isGrouped() && (m_groupSelector == -1);
}

//----------------------------------------------------------------------

void TFxAttributes::closeEditingGroup(int groupId) {
  if (!m_groupId.contains(groupId)) return;
  m_groupSelector = 0;
  while (m_groupId[m_groupSelector] != groupId &&
         m_groupSelector < m_groupId.size())
    m_groupSelector++;
}

//----------------------------------------------------------------------

int TFxAttributes::getEditingGroupId() {
  if (!isGrouped() || m_groupSelector + 1 >= m_groupId.size()) return -1;
  return m_groupId[m_groupSelector + 1];
}

//----------------------------------------------------------------------

std::wstring TFxAttributes::getEditingGroupName() {
  if (!isGrouped() || m_groupSelector + 1 >= m_groupName.size()) return L"";
  return m_groupName[m_groupSelector + 1];
}

//-----------------------------------------------------------------------------

void TFxAttributes::removeFromAllGroup() {
  m_groupId.clear();
  m_groupName.clear();
  m_groupSelector = -1;
}

//-----------------------------------------------------------------------------

void TFxAttributes::closeAllGroups() {
  if (isGroupEditing()) m_groupSelector = m_groupId.size() - 1;
}
