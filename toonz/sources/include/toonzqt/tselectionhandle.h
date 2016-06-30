#pragma once

#ifndef TSELECTIONHANDLE_H
#define TSELECTIONHANDLE_H

#include <QObject>
#include <string>
#include <vector>

#include "toonzqt/menubarcommand.h"
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

// forward declaration
class TSelection;
class CommandHandlerInterface;

//=============================================================================
// TSelectionHandle
//-----------------------------------------------------------------------------

class DVAPI TSelectionHandle final : public QObject {
  Q_OBJECT

  std::vector<TSelection *> m_selectionStack;
  std::vector<std::string> m_enabledCommandIds;

public:
  TSelectionHandle();
  ~TSelectionHandle();

  TSelection *getSelection() const;
  void setSelection(TSelection *selection);

  void pushSelection();
  void popSelection();

  // called by TSelection::enableCommand
  void enableCommand(std::string cmdId, CommandHandlerInterface *handler);

  void notifySelectionChanged();

  static TSelectionHandle *getCurrent();

signals:
  void selectionSwitched(TSelection *oldSelection, TSelection *newSelection);
  void selectionChanged(TSelection *selection);
};

#endif  // TSELECTIONHANDLE_H
