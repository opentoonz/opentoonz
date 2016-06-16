#pragma once

#ifndef HOOKSET_INCLUDED
#define HOOKSET_INCLUDED

#include "tgeometry.h"

#undef DVAPI
#undef DVVAR
#ifdef TOONZLIB_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//======================================================================

//    Forward declarations

class TFrameId;
class TIStream;
class TOStream;
class TrackerObjectsSet;
class HookSet;
class TXsheet;

//======================================================================

//************************************************************************************
//    Hook  declaration
//************************************************************************************

class DVAPI Hook {
public:
  Hook();

  bool isEmpty() const;
  bool isKeyframe(const TFrameId &fid)
      const;  //!< Returns true if the frames table contains \b fid.

  TPointD getPos(const TFrameId &fid) const;
  TPointD getAPos(const TFrameId &fid) const;
  TPointD getBPos(const TFrameId &fid) const;

  /*!
Set aPos of the frame with the frame id \b fid to \b pos.
If pos is near to bPos, aPos is set to bPos, else aPos and bPos are set to pos.
Make update at the end.
\attention if aPos==bPos then  setAPos change also bPos
*/
  void setAPos(const TFrameId &fid, TPointD pos);

  /*!
Set bPos of the frame with the frame id \b fid to \b pos.
If pos is near to aPos, bPos is set to aPos, else bPos is set to pos.
Make update at the end.
*/
  void setBPos(const TFrameId &fid, TPointD pos);

  int getId() const { return m_id; }

  // TrackerRegion
  //! Returns -1 if the hook has not an area to track
  int getTrackerObjectId() { return m_trackerObjectId; }
  void setTrackerObjectId(int trackerObjectId) {
    m_trackerObjectId = trackerObjectId;
  }

  double getTrackerRegionWidth() { return m_width; }
  void setTrackerRegionWidth(double width) { m_width = width; }

  double getTrackerRegionHeight() { return m_height; }
  void setTrackerRegionHeight(double height) { m_height = height; }

  TRectD getTrackerRegion(const TFrameId &fid);

  TPointD getDelta() const { return m_delta; }

  void renumber(const std::map<TFrameId, TFrameId> &renumberTable);
  void eraseFrame(const TFrameId &fid);

  void saveData(TOStream &os);
  void loadData(TIStream &is);

private:
  struct Frame {
    TPointD m_aPos, m_bPos;
    TPointD m_pos;  // tiene conto dei delta (m_bpos-m_apos) precedenti
  };

  typedef std::map<TFrameId, Frame> Frames;

private:
  Frames m_frames;
  TPointD m_delta;
  int m_id;

  // Proprietà relative alla trackerRegion
  //! If Hook is also a Trackeregion then m_trackerObjectId>=0, else is -1
  int m_trackerObjectId;
  double m_width;   // trackerRegion width
  double m_height;  // trackerRegion height

private:
  friend class HookSet;

  Frames::const_iterator find(TFrameId fid) const;
  void update();
};

//************************************************************************************
//    HookSet  declaration
//************************************************************************************

class DVAPI HookSet {
public:
  static const int maxHooksCount = 99;  //!< Maximum size of a HookSet

public:
  HookSet();
  ~HookSet();

  HookSet(const HookSet &);
  HookSet &operator=(const HookSet &);

  int getHookCount() const;
  Hook *getHook(int index) const;

  /*!
Creates (if needed) and returns the hook with specified index.
\warning Null pointer Hooks for indices smaller than \b index can be created.
*/
  Hook *touchHook(int index);

  /*!
Adds a hook to the set.

A Hook can be added replacing a null pointer in the Hook container with a new
Hook,
getting an empty hook or adding a new Hook at the end of the container.

It is possible to istantiate at max HookSet::maxHooksCount hooks - beyond
which this function will return 0.
*/
  Hook *addHook();

  //! Finds \b hook in the container, \a deletes it, and replaces its hook cell
  //! with 0.
  void clearHook(Hook *hook);

  //! Clears the container, deleting all stored hooks.
  void clearHooks();

  //! Generates a TrackerObjectsSet containing TrackerObject hooks.
  //! \sa TrackerObject
  TrackerObjectsSet *getTrackerObjectsSet() const;

  void renumber(const std::map<TFrameId, TFrameId> &renumberTable);
  void eraseFrame(const TFrameId &fid);

  void saveData(TOStream &os);
  void loadData(TIStream &is);

private:
  std::vector<Hook *> m_hooks;  //!< (owned) The hooks container
  TrackerObjectsSet *m_trackerObjectsSet;
};

//************************************************************************************
//    TrackerObject  declaration
//************************************************************************************

class DVAPI TrackerObject {
public:
  TrackerObject(int id) : m_id(id) {}

  int getId() const { return m_id; }

  bool isEmpty() { return (getHooksCount() == 0); }
  Hook *getHook(int index);

  int getHooksCount() { return m_hooks.size(); }

  void addHook(Hook *hook);
  void removeHook(int index);

private:
  int m_id;
  std::vector<Hook *> m_hooks;  //!< (NOT owned)
};

//************************************************************************************
//    TrackerObjectSet  declaration
//************************************************************************************

class DVAPI TrackerObjectsSet {
public:
  TrackerObjectsSet() {}

  int getTrackerObjectsCount() const { return m_trackerObjects.size(); }
  TrackerObject *getObject(int objectId);
  TrackerObject *getObjectFromIndex(int index);

  int getIndexFromId(
      int objectId);  // return objectIndex, return -1 if objectId doesn't exist
  int getIdFromIndex(int index);

  int addObject();  // add new object, return new object Id
  void addObject(TrackerObject *trackerObject);
  void removeObject(int objectId);

  void clearAll();

private:
  std::map<int, TrackerObject *>
      m_trackerObjects;  // (owned) tracker id -> TrackerObject
};

//-------------------------------------------------------------------
// helper functions
//-------------------------------------------------------------------

// 0 => "B"; 1,.. => "H1",...
DVAPI std::string getHookName(int hookCode);

#endif  // HOOKSET_INCLUDED
