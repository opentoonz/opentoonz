#pragma once

#ifndef T_STAGE_OBJECT_UTIL_INCLUDED
#define T_STAGE_OBJECT_UTIL_INCLUDED

// TnzCore includes
#include "tundo.h"

// TnzExt includes
#include "ext/plasticskeletondeformation.h"

// TnzLib includes
#include "toonz/txsheet.h"
#include "toonz/tstageobjectid.h"
#include "toonz/tstageobject.h"
#include "toonz/tstageobjectkeyframe.h"

#include "historytypes.h"

#undef DVAPI
#undef DVVAR
#ifdef TOONZLIB_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

class TDoubleParam;
class TXsheetHandle;
class TObjectHandle;
class TFrameHandle;

//=============================================================================
// TStageObjectValues
//-----------------------------------------------------------------------------

class DVAPI TStageObjectValues {
  class Channel {
    double m_value;

  public:
    TStageObject::Channel m_actionId;
    //    bool m_isKeyframe;
    Channel(TStageObject::Channel actionId);
    void setValue(double value);
    double getValue() const { return m_value; }
  };

  TXsheetHandle *m_xsheetHandle;  // Sbagliato: viene usato per l'update ma
                                  // anche per prendere l'xsheet corrente in
                                  // applyValues e in setGlobalKeyFrame
  TObjectHandle *m_objectHandle;  // Viene usato per l'update dei parametri.
  TFrameHandle *m_frameHandle;    // Viene usato per l'update dei parametri.
  TStageObjectId m_objectId;
  int m_frame;
  std::vector<Channel> m_channels;

public:
  TStageObjectValues();
  TStageObjectValues(TStageObjectId id, TStageObject::Channel a0);
  TStageObjectValues(TStageObjectId id, TStageObject::Channel a0,
                     TStageObject::Channel a1);

  void setXsheetHandle(TXsheetHandle *xsheetHandle) {
    m_xsheetHandle = xsheetHandle;
  }
  void setObjectHandle(TObjectHandle *objectHandle) {
    m_objectHandle = objectHandle;
  }
  void setFrameHandle(TFrameHandle *frameHandle) {
    m_frameHandle = frameHandle;
  }

  void add(TStageObject::Channel actionId);

  void updateValues();                              // this <- current values
  void applyValues(bool undoEnabled = true) const;  // this -> current values;
  // n.b. set undoEnabled=false if you call this method from undo()/redo()

  // change parameters; (doesn't applyValues())
  void setValue(double v);
  void setValues(double v0, double v1);

  // get parameter
  double getValue(int index) const;

  void setGlobalKeyframe();

  /*--
   * HistoryPanel表示のために、動かしたObject/Channel名・フレーム番号の文字列を返す
   * --*/
  QString getStringForHistory();
};

//=============================================================================
// UndoSetKeyFrame
//-----------------------------------------------------------------------------
//
// Per inserire un keyframe con undo su una pegbar si deve richiamare
// pegbar->setKeyframeWithoutUndo(int frame) e poi si deve aggiungere
// a TUndoManager l'undo UndoSetKeyFrame(..).
//
//-----------------------------------------------------------------------------

class DVAPI UndoSetKeyFrame final : public TUndo {
  TStageObjectId m_objId;
  int m_frame;

  TXsheetHandle *m_xsheetHandle;
  TObjectHandle *m_objectHandle;

  TStageObject::Keyframe m_key;

public:
  UndoSetKeyFrame(TStageObjectId objectId, int frame,
                  TXsheetHandle *xsheetHandle);

  void setXsheetHandle(TXsheetHandle *xsheetHandle) {
    m_xsheetHandle = xsheetHandle;
  }
  void setObjectHandle(TObjectHandle *objectHandle) {
    m_objectHandle = objectHandle;
  }

  void undo() const override;
  void redo() const override;

  int getSize() const override;

  QString getHistoryString() override {
    return QObject::tr("Set Keyframe   %1 at frame %2")
        .arg(QString::fromStdString(m_objId.toString()))
        .arg(m_frame + 1);
  }
};

//=============================================================================
// UndoRemoveKeyFrame
//-----------------------------------------------------------------------------
//
// Per rimuovere con undo un keyframe su una pegbar si deve richiamare
// pegbar->removeframeWithoutUndo(int frame) e poi si deve aggiungere
// a TUndoManager l'undo UndoRemoveKeyFrame(..).
//
//-----------------------------------------------------------------------------

class DVAPI UndoRemoveKeyFrame final : public TUndo {
  TStageObjectId m_objId;
  int m_frame;

  TXsheetHandle *m_xsheetHandle;  // Sbagliato: viene usato per prendere
                                  // l'xsheet corrente nell'undo
  TObjectHandle
      *m_objectHandle;  // OK: viene usato per notificare i cambiamenti!

  TStageObject::Keyframe m_key;

public:
  UndoRemoveKeyFrame(TStageObjectId objectId, int frame,
                     TStageObject::Keyframe key, TXsheetHandle *xsheetHandle);

  void setXsheetHandle(TXsheetHandle *xsheetHandle) {
    m_xsheetHandle = xsheetHandle;
  }
  void setObjectHandle(TObjectHandle *objectHandle) {
    m_objectHandle = objectHandle;
  }

  void undo() const override;
  void redo() const override;

  int getSize() const override;

  QString getHistoryString() override {
    return QObject::tr("Remove Keyframe   %1 at frame %2")
        .arg(QString::fromStdString(m_objId.toString()))
        .arg(m_frame);
  }
};

//=============================================================================
// UndoStageObjectCenterMove
//-----------------------------------------------------------------------------

class DVAPI UndoStageObjectCenterMove final : public TUndo {
  TStageObjectId m_pid;
  int m_frame;
  TPointD m_before, m_after;
  TXsheetHandle *m_xsheetHandle;  // Sbagliato: viene usato per prendere
                                  // l'xsheet corrente nell'undo
  TObjectHandle
      *m_objectHandle;  // OK: viene usato per notificare i cambiamenti!

public:
  UndoStageObjectCenterMove(const TStageObjectId &id, int frame,
                            const TPointD &before, const TPointD &after);

  void setXsheetHandle(TXsheetHandle *xsheetHandle) {
    m_xsheetHandle = xsheetHandle;
  }
  void setObjectHandle(TObjectHandle *objectHandle) {
    m_objectHandle = objectHandle;
  }

  void undo() const override;
  void redo() const override;
  int getSize() const override { return sizeof(*this); }

  QString getHistoryString() override {
    return QObject::tr("Move Center   %1  Frame %2")
        .arg(QString::fromStdString(m_pid.toString()))
        .arg(m_frame + 1);
  }
  int getHistoryType() override { return HistoryType::EditTool_Move; }
};

//=============================================================================
// UndoStageObjectMove
//-----------------------------------------------------------------------------

class DVAPI UndoStageObjectMove final : public TUndo {
  TStageObjectValues m_before, m_after;
  TObjectHandle
      *m_objectHandle;  // OK: viene usato per notificare i cambiamenti!

public:
  UndoStageObjectMove(const TStageObjectValues &before,
                      const TStageObjectValues &after);

  void setObjectHandle(TObjectHandle *objectHandle) {
    m_objectHandle = objectHandle;
  }

  void undo() const override;
  void redo() const override;
  int getSize() const override { return sizeof(*this); }

  QString getHistoryString() override { return m_before.getStringForHistory(); }
  int getHistoryType() override { return HistoryType::EditTool_Move; }
};

//=============================================================================
// UndoStageObjectPinned
//-----------------------------------------------------------------------------

class DVAPI UndoStageObjectPinned final : public TUndo {
  TStageObjectId m_pid;
  int m_frame;
  bool m_before, m_after;
  TXsheetHandle *m_xsheetHandle;  // Sbagliato: viene usato per prendere
                                  // l'xsheet corrente nell'undo
  TObjectHandle
      *m_objectHandle;  // OK: viene usato per notificare i cambiamenti!

public:
  UndoStageObjectPinned(const TStageObjectId &id, int frame, const bool &before,
                        const bool &after);

  void setXsheetHandle(TXsheetHandle *xsheetHandle) {
    m_xsheetHandle = xsheetHandle;
  }
  void setObjectHandle(TObjectHandle *objectHandle) {
    m_objectHandle = objectHandle;
  }

  void undo() const override;
  void redo() const override;
  int getSize() const override { return sizeof(*this); }
};

//=============================================================================
// insert/delete frame
//-----------------------------------------------------------------------------

// todo: non dovrebbero stare in questo file. possono essere usati anche per gli
// effetti

void DVAPI insertFrame(TDoubleParam *param, int frame);
void DVAPI removeFrame(TDoubleParam *param, int frame);

void DVAPI insertFrame(TStageObject *obj, int frame);
void DVAPI removeFrame(TStageObject *obj, int frame);

#endif
