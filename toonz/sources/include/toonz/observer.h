#pragma once

#ifndef T_OBSERVER_INCLUDED
#define T_OBSERVER_INCLUDED

// OBSOLETO??

//=============================================================================
//
// TODO:
//
// separarlo in piu' file (almeno uno che contenga il solo TChangeObserver
// e che va incluso in frame.h; forse non e' necessario se tutti quelli che
// includono frame.h includono anche observer.h (probabile)
//
//=============================================================================

//#include "tlevel.h"
#include "tgeometry.h"
#include "tfilepath.h"
#include <set>

//=============================================================================

#undef DVAPI
#undef DVVAR
#ifdef TOONZLIB_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//-------------------------------------------------------------------

namespace DagViewer {
class Viewer;
}

//-------------------------------------------------------------------

class TChangeObserver {
public:
  virtual ~TChangeObserver() {}
};

template <class T>
class TChangeObserverT {
public:
  virtual ~TChangeObserverT() {}
  virtual void update(const T &) = 0;
};

//-------------------------------------------------------------------

class TObserverList {
public:
  virtual ~TObserverList() {}
  virtual void attach(TChangeObserver *observer) = 0;
  virtual void detach(TChangeObserver *observer) = 0;
};

//-------------------------------------------------------------------

template <class T>
class TObserverListT final : public TObserverList {
  friend class TNotifier;

public:
  TObserverListT() {}
  ~TObserverListT() {}
  typedef TChangeObserverT<T> Observer;

  void attach(TChangeObserver *observer) override {
    if (Observer *o = dynamic_cast<Observer *>(observer))
      m_observers.push_back(o);
  }
  void detach(TChangeObserver *observer) override {
    if (Observer *o = dynamic_cast<Observer *>(observer))
      m_observers.erase(std::remove(m_observers.begin(), m_observers.end(), o),
                        m_observers.end());
  }
  void notify(const T &change) {
    std::vector<Observer *> observers(m_observers);
    for (typename std::vector<Observer *>::iterator it = observers.begin();
         it != observers.end(); ++it)
      (*it)->update(change);
  }

private:
  std::vector<Observer *> m_observers;
};

//-------------------------------------------------------------------

class TFrameChange {
public:
  int m_frame;
  int m_oldFrame;
  TFrameId m_fid;
  TFrameChange(int frame, int oldFrame)
      : m_frame(frame), m_oldFrame(oldFrame), m_fid(0) {}
  TFrameChange(const TFrameId &fid) : m_frame(-1), m_fid(fid) {}
};
typedef TChangeObserverT<TFrameChange> TFrameObserver;

//-------------------------------------------------------------------

class TColumnChange {
public:
  int m_columnIndex;
  TColumnChange(int columnIndex) : m_columnIndex(columnIndex) {}
};
typedef TChangeObserverT<TColumnChange> TColumnObserver;

//-------------------------------------------------------------------

class TCurrentFxChange {
public:
  TCurrentFxChange() {}
};
typedef TChangeObserverT<TCurrentFxChange> TCurrentFxObserver;

//-------------------------------------------------------------------

class TGlobalChange {
  bool m_sceneChanged;

public:
  TGlobalChange(bool sceneChanged = false) : m_sceneChanged(sceneChanged) {}
  bool isSceneChanged() const { return m_sceneChanged; }
};
typedef TChangeObserverT<TGlobalChange> TGlobalObserver;

//-------------------------------------------------------------------

class TSceneNameChange {
public:
  TSceneNameChange() {}
};
typedef TChangeObserverT<TSceneNameChange> TSceneNameObserver;

//-------------------------------------------------------------------

class TDirectoryChange {
  TFilePath m_path;

public:
  TDirectoryChange(const TFilePath &path) : m_path(path) {}
  TFilePath getPath() const { return m_path; }
};
typedef TChangeObserverT<TDirectoryChange> TDirectoryObserver;

//-------------------------------------------------------------------

class TStageChange {
public:
  TRectD m_rect;

  TStageChange() : m_rect(0, 0, 0, 0) {}
  TStageChange(const TRectD &rect) : m_rect(rect) {}
};

typedef TChangeObserverT<TStageChange> TStageObserver;

//-------------------------------------------------------------------

class TColumnHeadChange {
public:
  int m_columnIndex;
  TColumnHeadChange() : m_columnIndex(-2) {}
  TColumnHeadChange(int index) : m_columnIndex(index) {}
};

typedef TChangeObserverT<TColumnHeadChange> TColumnHeadObserver;

//-------------------------------------------------------------------

class TXshLevel;

class TDrawingChange {
public:
  TXshLevel *m_xl;
  TFrameId m_fid;
  bool m_wholeLevel;
  TDrawingChange(TXshLevel *xl, TFrameId fid)
      : m_xl(xl), m_fid(fid), m_wholeLevel(false) {}
  TDrawingChange(TXshLevel *xl) : m_xl(xl), m_wholeLevel(true) {}
  TDrawingChange() : m_xl(0), m_wholeLevel(true) {}
};

typedef TChangeObserverT<TDrawingChange> TDrawingObserver;

//-------------------------------------------------------------------

class TXsheetChange {
public:
};

typedef TChangeObserverT<TXsheetChange> TXsheetObserver;

//-------------------------------------------------------------------

class TLevelChange {
public:
  TLevelChange() {}
};

typedef TChangeObserverT<TLevelChange> TLevelObserver;

//-------------------------------------------------------------------

class TPatternStrokeStylesChange {
public:
};

typedef TChangeObserverT<TPatternStrokeStylesChange>
    TPatternStrokeStyleObserver;

//-------------------------------------------------------------------

class TStageObjectChange {
public:
  TStageObjectChange() {}
};

typedef TChangeObserverT<TStageObjectChange> TStageObjectObserver;

//-------------------------------------------------------------------

class TKeyFrameChange {
public:
  TKeyFrameChange() {}
};

typedef TChangeObserverT<TKeyFrameChange> TKeyFrameObserver;

//-------------------------------------------------------------------

class TColorStyleChange {
public:
  TColorStyleChange() {}
};

typedef TChangeObserverT<TColorStyleChange> TColorStyleObserver;

//-------------------------------------------------------------------

class TPaletteChange {
  bool m_isCleanup;

public:
  TPaletteChange(bool isCleanup = false) : m_isCleanup(isCleanup) {}
  bool isCleanup() const { return m_isCleanup; }
};

typedef TChangeObserverT<TPaletteChange> TPaletteObserver;

//-------------------------------------------------------------------

class TToolChange {
public:
  TToolChange() {}
};

typedef TChangeObserverT<TToolChange> TToolObserver;

//-------------------------------------------------------------------

class TCastChange {
public:
  TCastChange() {}
};

typedef TChangeObserverT<TCastChange> TCastObserver;

//-------------------------------------------------------------------

class TDagViewerChange {
  DagViewer::Viewer *m_viewer;

public:
  TDagViewerChange(DagViewer::Viewer *viewer) : m_viewer(viewer) {}
  const DagViewer::Viewer *getViewer() const { return m_viewer; }
};

typedef TChangeObserverT<TDagViewerChange> TDagViewerObserver;
//-------------------------------------------------------------------

class TFxDagChange {
public:
  TFxDagChange() {}
};

typedef TChangeObserverT<TFxDagChange> TFxDagObserver;

//===================================================================

class DVAPI TNotifier {
  std::vector<TObserverList *> m_obsList;
  TObserverListT<TGlobalChange> m_globalObs;
  TObserverListT<TSceneNameChange> m_sceneNameObs;
  TObserverListT<TDirectoryChange> m_directoryObs;
  TObserverListT<TStageChange> m_stageObs;
  TObserverListT<TColumnHeadChange> m_columnHeadObs;
  TObserverListT<TDrawingChange> m_drawingObs;
  TObserverListT<TLevelChange> m_levelObs;
  TObserverListT<TXsheetChange> m_xsheetObs;
  TObserverListT<TFrameChange> m_frameObs;
  TObserverListT<TCurrentFxChange> m_currentFxObs;
  TObserverListT<TStageObjectChange> m_stageObjectObs;
  TObserverListT<TPatternStrokeStylesChange> m_patternStrokeStylesObs;
  TObserverListT<TKeyFrameChange> m_keyFrameObs;
  TObserverListT<TColorStyleChange> m_colorStyleObs;
  TObserverListT<TPaletteChange> m_paletteObs;
  TObserverListT<TToolChange> m_toolObs;
  TObserverListT<TCastChange> m_castObs;

  TObserverListT<TDagViewerChange> m_dagViewerObs;
  TObserverListT<TFxDagChange> m_fxDagObs;

  bool m_dirtyFlag;

  std::set<TGlobalObserver *> m_newSceneNotifiedObs;

  TNotifier() : m_dirtyFlag(false) {
    m_obsList.push_back(&m_globalObs);
    m_obsList.push_back(&m_sceneNameObs);
    m_obsList.push_back(&m_directoryObs);
    m_obsList.push_back(&m_stageObs);
    m_obsList.push_back(&m_columnHeadObs);
    m_obsList.push_back(&m_drawingObs);
    m_obsList.push_back(&m_levelObs);
    m_obsList.push_back(&m_xsheetObs);
    m_obsList.push_back(&m_patternStrokeStylesObs);
    m_obsList.push_back(&m_frameObs);
    m_obsList.push_back(&m_stageObjectObs);
    m_obsList.push_back(&m_keyFrameObs);
    m_obsList.push_back(&m_colorStyleObs);
    m_obsList.push_back(&m_paletteObs);
    m_obsList.push_back(&m_toolObs);
    m_obsList.push_back(&m_castObs);
    m_obsList.push_back(&m_dagViewerObs);
    m_obsList.push_back(&m_currentFxObs);
    m_obsList.push_back(&m_fxDagObs);
  }

public:
  static TNotifier *instance();

  // void attach(TChangeObserver*observer);
  // void detach(TChangeObserver*observer);

  void notify(const TGlobalChange &c);

  void notify(const TStageChange &c) { m_stageObs.notify(c); }
  void notify(const TSceneNameChange &c) { m_sceneNameObs.notify(c); }
  void notify(const TDirectoryChange &c) { m_directoryObs.notify(c); }
  void notify(const TColumnHeadChange &c) {
    m_columnHeadObs.notify(c);
    m_dirtyFlag = true;
  }
  void notify(const TDrawingChange &c) {
    m_drawingObs.notify(c);
    m_dirtyFlag = true;
  }
  void notify(const TLevelChange &c) {
    m_levelObs.notify(c);
    m_dirtyFlag = true;
  }
  void notify(const TXsheetChange &c) {
    m_xsheetObs.notify(c);
    m_dirtyFlag = true;
  }
  void notify(const TPatternStrokeStylesChange &c) {
    m_patternStrokeStylesObs.notify(c);
  }
  void notify(const TFrameChange &c) { m_frameObs.notify(c); }
  void notify(const TCurrentFxChange &c) { m_currentFxObs.notify(c); }
  void notify(const TStageObjectChange &c) { m_stageObjectObs.notify(c); }
  void notify(const TKeyFrameChange &c) {
    m_keyFrameObs.notify(c);
    m_dirtyFlag = true;
  }
  void notify(const TColorStyleChange &c) { m_colorStyleObs.notify(c); }
  void notify(const TPaletteChange &c) {
    m_paletteObs.notify(c);
    m_dirtyFlag = true;
  }
  void notify(const TToolChange &c) { m_toolObs.notify(c); }
  void notify(const TCastChange &c) { m_castObs.notify(c); }

  void notify(const TDagViewerChange &c) { m_dagViewerObs.notify(c); }
  void notify(const TFxDagChange &c) { m_fxDagObs.notify(c); }

  bool getDirtyFlag() const { return m_dirtyFlag; }
  void setDirtyFlag(bool on) { m_dirtyFlag = on; }
};

//-------------------------------------------------------------------

#endif
