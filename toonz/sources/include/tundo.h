#pragma once

#ifndef T_UNDO_INCLUDED
#define T_UNDO_INCLUDED

#include <memory>

// TnzCore includes
#include "tsmartpointer.h"

// Qt includes
#include <QObject>
#include <QString>

#undef DVAPI
#undef DVVAR
#ifdef TNZCORE_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//************************************************************************
//    TUndo  class
//************************************************************************

class DVAPI TUndo {
public:
  // To be called in the last of the block when undo
  bool m_isLastInBlock;
  // To be called in the last of the block when redo
  bool m_isLastInRedoBlock;

public:
  TUndo() {}
  virtual ~TUndo() {}

  virtual int getSize() const = 0;

  virtual void undo() const = 0;
  virtual void redo() const = 0;

  virtual void onAdd() {}

  // for history viewer panel
  virtual QString getHistoryString() {
    return QObject::tr("Unidentified Action");
  }
  virtual int getHistoryType() { return 0; }  // HistoryType::Unidentified

private:
  // Noncopyable
  TUndo(const TUndo &undo);
  TUndo &operator=(const TUndo &undo);
};

//************************************************************************
//    TUndoManager  class
//************************************************************************

class DVAPI TUndoManager final : public QObject {
  Q_OBJECT

public:
  TUndoManager();
  ~TUndoManager();

  static TUndoManager *manager();

  void beginBlock();
  void endBlock();

  void add(TUndo *undo);

  bool undo();
  bool redo();

  void reset();

  /*!If forward = false remove n elements in the stack before m_imp->m_current;
else if forward = true remove n elements in the stack after m_imp->m_current.
If n = -1 remove all elements before or after m_imp->m_current.*/

  void popUndo(int n = -1, bool forward = false);

  //! undo() and redo() methods call TUndoManager::manager()->skip() if
  //! they want another undo/redo performed

  void skip();

  //! Set the size of memory used for undo redo.
  //! \b memorySyze is expressed in MB
  void setUndoMemorySize(int memorySyze);

  // for history viewer pane
  int getHistoryCount();
  int getCurrentHistoryIndex();
  TUndo *getUndoItem(int index);

Q_SIGNALS:

  void historyChanged();

  void somethingChanged();

private:
  struct TUndoManagerImp;
  std::unique_ptr<TUndoManagerImp> m_imp;

private:
  // Noncopyable
  TUndoManager(const TUndoManager &);
  TUndoManager &operator=(const TUndoManager &);
};

//************************************************************************
//    TUndoScopedBlock  class
//************************************************************************

struct TUndoScopedBlock {
  TUndoScopedBlock() { TUndoManager::manager()->beginBlock(); }
  ~TUndoScopedBlock() { TUndoManager::manager()->endBlock(); }

private:
  // Noncopyable
  TUndoScopedBlock(const TUndoScopedBlock &);
  TUndoScopedBlock &operator=(const TUndoScopedBlock &);
};

#endif  // T_UNDO_INCLUDED
