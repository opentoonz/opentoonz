#pragma once

#ifndef TSELECTION_H
#define TSELECTION_H

#include "menubarcommand.h"
#include "tcommon.h"

#undef DVAPI
#undef DVVAR
#ifdef TOONZQT_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

class QMenu;
class QWidget;

#include <QMap>

//=============================================================================
// TSelection
//-----------------------------------------------------------------------------

class DVAPI TSelection {
public:
  class View {
  public:
    virtual ~View(){};

    virtual void onSelectionChanged() = 0;
    virtual void enableCommands() {}
  };

public:
  TSelection();
  virtual ~TSelection();

  // override this to define selection related commands
  virtual void enableCommands() {
    if (m_view) m_view->enableCommands();
  }

  // call selection handler enableCommand()
  void enableCommand(CommandId cmdId, CommandHandlerInterface *handler);

  // overridden enableCommands() will call enableCommand()
  template <class T>
  inline void enableCommand(T *target, CommandId cmdId, void (T::*method)()) {
    enableCommand(cmdId, new CommandHandlerHelper<T>(target, method));
  }

  template <class T, typename R>
  inline void enableCommand(T *target, CommandId cmdId, void (T::*method)(R),
                            R value) {
    enableCommand(cmdId,
                  new CommandHandlerHelper2<T, R>(target, method, value));
  }

  void makeCurrent();
  void makeNotCurrent();
  static TSelection *getCurrent();
  static void setCurrent(TSelection *selection);

  virtual bool isEmpty() const = 0;
  virtual void selectNone()    = 0;

  virtual bool addMenuActions(QMenu *menu) { return false; }
  void addMenuAction(QMenu *menu, CommandId cmdId);

  void setView(View *view) { m_view = view; }
  View *getView() const { return m_view; }

  void notifyView();

  // specify alternative command name when the selection is current.
  // the commands must be "Edit" category.
  const QMap<CommandId, QString> &alternativeCommandNames() {
    return m_alternativeCommandNames;
  }

private:
  View *m_view;

protected:
  QMap<CommandId, QString> m_alternativeCommandNames;
};

#endif  // TSELECTION_H
