

#include "toonz/hook.h"
#include "tstream.h"
#include "tfilepath.h"

#include "tconvert.h"
#include "toonz/txsheet.h"
#include "toonz/txshcell.h"

//---------------------------------------------------------

Hook::Hook() : m_id(0), m_trackerObjectId(-1) {}

//---------------------------------------------------------

bool Hook::isEmpty() const { return m_frames.empty(); }

//---------------------------------------------------------

bool Hook::isKeyframe(const TFrameId &fid) const {
  return m_frames.count(fid) > 0;
}

//---------------------------------------------------------

Hook::Frames::const_iterator Hook::find(TFrameId fid) const {
  if (m_frames.empty()) return m_frames.end();
  Frames::const_iterator it;
  it = m_frames.lower_bound(fid);
  if (it == m_frames.end()) {
    // dopo l'ultimo
    assert(it != m_frames.begin());
    --it;
  } else if (it->first == fid || it == m_frames.begin()) {
    // giusto o prima del primo
  } else
    --it;
  return it;
}

//---------------------------------------------------------

TPointD Hook::getPos(const TFrameId &fid) const {
  // return TPointD(0,0);
  Frames::const_iterator it = find(fid);
  if (it == m_frames.end())
    return TPointD();
  else
    return it->second.m_pos;
}

//---------------------------------------------------------

TPointD Hook::getAPos(const TFrameId &fid) const {
  Frames::const_iterator it = find(fid);
  if (it == m_frames.end())
    return TPointD();
  else if (it->first == fid)
    return it->second.m_aPos;
  else
    return it->second.m_bPos;
}

//---------------------------------------------------------

TPointD Hook::getBPos(const TFrameId &fid) const {
  Frames::const_iterator it = find(fid);
  if (it == m_frames.end())
    return TPointD();
  else
    return it->second.m_bPos;
}

//---------------------------------------------------------

void Hook::setAPos(const TFrameId &fid, TPointD pos) {
  Frames::iterator it = m_frames.lower_bound(fid);
  Frame f;
  if (it != m_frames.end() && it->first == fid) {
    f = it->second;
    if (f.m_aPos == f.m_bPos)
      f.m_aPos = f.m_bPos = pos;
    else if (tdistance2(pos, f.m_bPos) <= 1)
      f.m_aPos = f.m_bPos;
    else
      f.m_aPos = pos;
  } else {
    f.m_aPos = f.m_bPos = pos;
  }
  m_frames[fid] = f;
  update();
}

//---------------------------------------------------------

void Hook::setBPos(const TFrameId &fid, TPointD pos) {
  Frames::iterator it = m_frames.lower_bound(fid);
  Frame f;
  if (it != m_frames.end() && it->first == fid) {
    f = it->second;
    if (tdistance2(pos, f.m_aPos) <= 1)
      f.m_bPos = f.m_aPos;
    else
      f.m_bPos = pos;
  } else {
    f.m_aPos = getAPos(fid);
    f.m_bPos = pos;
  }
  m_frames[fid] = f;
  update();
}

//---------------------------------------------------------
TRectD Hook::getTrackerRegion(const TFrameId &fid) {
  TRectD rect;
  rect.x0 = getPos(fid).x - (getTrackerRegionWidth() * 0.5);
  rect.y0 = getPos(fid).y - (getTrackerRegionHeight() * 0.5);
  rect.x1 = getPos(fid).x + (getTrackerRegionWidth() * 0.5);
  rect.y1 = getPos(fid).y + (getTrackerRegionHeight() * 0.5);
  return rect;
}

//---------------------------------------------------------

void Hook::update() {
  TPointD delta;
  for (Frames::iterator it = m_frames.begin(); it != m_frames.end(); ++it) {
    it->second.m_pos = it->second.m_aPos + delta;
    delta -= it->second.m_bPos - it->second.m_aPos;
  }
  m_delta = delta;
}

//---------------------------------------------------------

void Hook::renumber(const std::map<TFrameId, TFrameId> &renumberTable) {
  Frames oldFrames = m_frames;
  m_frames.clear();
  for (Frames::iterator it = oldFrames.begin(); it != oldFrames.end(); ++it) {
    std::map<TFrameId, TFrameId>::const_iterator j =
        renumberTable.find(it->first);
    assert(j != renumberTable.end());
    if (j == renumberTable.end()) continue;
    m_frames[j->second] = it->second;
  }
}

//---------------------------------------------------------

void Hook::eraseFrame(const TFrameId &fid) { m_frames.erase(fid); }

//---------------------------------------------------------

void Hook::saveData(TOStream &os) {
  Frames::iterator j;
  for (j = m_frames.begin(); j != m_frames.end(); ++j) {
    os.openChild("frame");
    os << j->first.getNumber();
    os << j->second.m_aPos.x << j->second.m_aPos.y;
    os << j->second.m_bPos.x << j->second.m_bPos.y;
    os.closeChild();
  }
  if (m_trackerObjectId >= 0) {
    os.openChild("tracker");
    os << m_trackerObjectId << m_width << m_height;
    os.closeChild();
  }
}

//---------------------------------------------------------

void Hook::loadData(TIStream &is) {
  m_frames.clear();
  std::string tagName;
  while (is.matchTag(tagName)) {
    if (tagName == "frame") {
      Frame f;
      int frameNumber = 0;
      is >> frameNumber;
      is >> f.m_aPos.x >> f.m_aPos.y;
      is >> f.m_bPos.x >> f.m_bPos.y;
      m_frames[TFrameId(frameNumber)] = f;
      m_trackerObjectId               = -1;
    } else if (tagName == "tracker") {
      is >> m_trackerObjectId;
      is >> m_width;
      is >> m_height;
    } else
      throw TException("expected <frame>");
    is.matchEndTag();
  }
  update();
}

//=========================================================

HookSet::HookSet() : m_trackerObjectsSet(new TrackerObjectsSet) {}

//---------------------------------------------------------

HookSet::~HookSet() {
  clearPointerContainer(m_hooks);
  delete m_trackerObjectsSet;
}

//---------------------------------------------------------

HookSet::HookSet(const HookSet &other)
    : m_hooks(other.m_hooks), m_trackerObjectsSet(new TrackerObjectsSet) {
  int h, hCount = m_hooks.size();
  for (h                       = 0; h != hCount; ++h)
    if (m_hooks[h]) m_hooks[h] = new Hook(*m_hooks[h]);
}

//---------------------------------------------------------

HookSet &HookSet::operator=(const HookSet &other) {
  clearPointerContainer(m_hooks);
  m_hooks = other.m_hooks;

  int h, hCount = m_hooks.size();
  for (h                       = 0; h != hCount; ++h)
    if (m_hooks[h]) m_hooks[h] = new Hook(*m_hooks[h]);

  return *this;
}

//---------------------------------------------------------

int HookSet::getHookCount() const { return m_hooks.size(); }

//---------------------------------------------------------

Hook *HookSet::getHook(int index) const {
  return 0 <= index && index < getHookCount() ? m_hooks[index] : 0;
}

//---------------------------------------------------------

Hook *HookSet::touchHook(int index) {
  assert(0 <= index && index < maxHooksCount);
  if (index < 0 || index >= maxHooksCount) return 0;

  while (index >= (int)m_hooks.size()) m_hooks.push_back(0);

  if (m_hooks[index] == 0) {
    Hook *hook     = new Hook();
    m_hooks[index] = hook;
    hook->m_id     = index;
  }

  return m_hooks[index];
}

//---------------------------------------------------------

Hook *HookSet::addHook() {
  int h, hCount = m_hooks.size();
  for (h = 0; h != hCount; ++h) {
    if (m_hooks[h] == 0) {
      Hook *hook = new Hook();
      m_hooks[h] = hook;
      hook->m_id = h;

      return hook;
    } else if (m_hooks[h]->isEmpty())
      return m_hooks[h];
  }

  if (m_hooks.size() >= maxHooksCount) return 0;

  Hook *hook = new Hook();
  hook->m_id = m_hooks.size();
  m_hooks.push_back(hook);

  return hook;
}

//---------------------------------------------------------

void HookSet::clearHook(Hook *hook) {
  for (int i                           = 0; i < (int)m_hooks.size(); i++)
    if (m_hooks[i] == hook) m_hooks[i] = 0;
  delete hook;
}

//---------------------------------------------------------

void HookSet::clearHooks() {
  for (int i = 0; i < (int)m_hooks.size(); i++) delete m_hooks[i];
  m_hooks.clear();
}

//---------------------------------------------------------

TrackerObjectsSet *HookSet::getTrackerObjectsSet() const {
  // Ripulisco l'insieme dei tracker objects
  m_trackerObjectsSet->clearAll();

  // costruisco il tracker object in base agli Hook
  for (int i = 0; i < getHookCount(); ++i) {
    Hook *hook = getHook(i);
    if (!hook || hook->isEmpty()) continue;

    int trackerObjectId = hook->getTrackerObjectId();
    if (trackerObjectId >= 0)  // se l'Hook ha anche una regione
    {
      TrackerObject *trackerObject =
          m_trackerObjectsSet->getObject(trackerObjectId);
      if (trackerObject == NULL) {
        trackerObject = new TrackerObject(trackerObjectId);
        m_trackerObjectsSet->addObject(trackerObject);
      }
      trackerObject = m_trackerObjectsSet->getObject(trackerObjectId);
      assert(trackerObject != NULL);
      trackerObject->addHook(hook);
    }
  }
  return m_trackerObjectsSet;
}

//---------------------------------------------------------

void HookSet::renumber(const std::map<TFrameId, TFrameId> &renumberTable) {
  for (int i = 0; i < getHookCount(); i++)
    if (getHook(i)) getHook(i)->renumber(renumberTable);
}

//---------------------------------------------------------

void HookSet::eraseFrame(const TFrameId &fid) {
  for (int i = 0; i < getHookCount(); i++)
    if (getHook(i)) getHook(i)->eraseFrame(fid);
}

//---------------------------------------------------------

void HookSet::saveData(TOStream &os) {
  for (int i = 0; i < getHookCount(); ++i) {
    os.openChild("hook");
    Hook *hook = getHook(i);
    // Pay attention: it is necessary that empty hook will be however saved
    // in the xml file.
    if (hook) hook->saveData(os);
    os.closeChild();
  }
}

//---------------------------------------------------------

void HookSet::loadData(TIStream &is) {
  std::string tagName;
  while (is.matchTag(tagName)) {
    if (tagName == "hook") {
      Hook *hook = new Hook();
      hook->m_id = m_hooks.size();
      hook->loadData(is);
      is.matchEndTag();
      m_hooks.push_back(hook);
    } else
      return;

    is.matchEndTag();
  }
}

//=========================================================
//
//	TRACKER
//

Hook *TrackerObject::getHook(int index) {
  assert(index >= 0 && index < getHooksCount());
  return m_hooks.at(index);
}
void TrackerObject::addHook(Hook *hook) { m_hooks.push_back(hook); }

void TrackerObject::removeHook(int index) {
  assert(index >= 0 && index < (int)m_hooks.size());
  m_hooks.erase(m_hooks.begin() + index);
}

TrackerObject *TrackerObjectsSet::getObject(int objectId) {
  assert(objectId >= 0);
  std::map<int, TrackerObject *>::iterator it = m_trackerObjects.find(objectId);
  if (it != m_trackerObjects.end())
    return it->second;
  else
    return NULL;
}

TrackerObject *TrackerObjectsSet::getObjectFromIndex(int index) {
  assert(index >= 0 && index < (int)m_trackerObjects.size());
  return m_trackerObjects[index];
}

// return objectIndex, return -1 if objectId doesn't exist
int TrackerObjectsSet::getIndexFromId(int objectId) {
  int index = -1;
  int i     = 0;
  for (i = 0; i < (int)m_trackerObjects.size(); i++) {
    int id = m_trackerObjects[i]->getId();
    if (id == objectId) {
      index = i;
      break;
    }
  }
  return index;
}

int TrackerObjectsSet::getIdFromIndex(int index) {
  assert(index >= 0 && index < (int)m_trackerObjects.size());
  return m_trackerObjects[index]->getId();
}

// add new object, return new object Id
int TrackerObjectsSet::addObject() {
  // assegno l'id
  int id;
  if (m_trackerObjects.size() > 0) {
    std::map<int, TrackerObject *>::iterator it = m_trackerObjects.end();
    --it;
    id = it->first + 1;
  } else
    id = 0;

  TrackerObject *trackerObject = new TrackerObject(id);
  m_trackerObjects[id]         = trackerObject;
  return id;
}

void TrackerObjectsSet::addObject(TrackerObject *trackerObject) {
  assert(trackerObject);
  int id = trackerObject->getId();
  assert(id >= 0);

  m_trackerObjects[id] = trackerObject;
}

void TrackerObjectsSet::removeObject(int objectId) {
  assert(objectId >= 0);
  std::map<int, TrackerObject *>::iterator tt = m_trackerObjects.find(objectId);
  if (tt != m_trackerObjects.end()) {
    delete tt->second;
    m_trackerObjects.erase(tt);
  }
}

void TrackerObjectsSet::clearAll() {
  std::map<int, TrackerObject *>::iterator tt, tEnd(m_trackerObjects.end());
  for (tt = m_trackerObjects.begin(); tt != tEnd; ++tt) delete tt->second;

  m_trackerObjects.clear();
}

//=========================================================

std::string getHookName(int code) {
  assert(0 <= code && code < 10);
  if (code == 0)
    return "B";
  else
    return "H" + std::to_string(code);
}
