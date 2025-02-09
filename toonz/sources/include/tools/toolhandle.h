#pragma once

#ifndef TOOLEHANDLE_H
#define TOOLEHANDLE_H

#include "tcommon.h"
#include "timage.h"
#include <QObject>
#include <QMap>
#include <QElapsedTimer>

// forward declaration
class TTool;
class QAction;
class QString;

#undef DVAPI
#undef DVVAR
#ifdef TNZTOOLS_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//=============================================================================
// ToolHandle
//-----------------------------------------------------------------------------

class DVAPI ToolHandle final : public QObject {
  Q_OBJECT

  TTool *m_tool;
  QString m_toolName;
  int m_toolTargetType;
  QString m_storedToolName;
  QElapsedTimer m_storedToolTime;
  QString m_oldToolName;
  bool m_toolIsBusy;

  // State tracking for intuitive viewer navigation
  QString m_originalTool;  // Store tool before navigation mode
  bool m_inNavigationMode = false;
  bool m_spacePressed     = false;
  bool m_shiftPressed     = false;
  bool m_ctrlPressed      = false;

public:
  ToolHandle();
  ~ToolHandle();

  TTool *getTool() const;
  void setTool(QString name);
  void setTargetType(int targetType);  // TODO: unused, remove?

  const QString &getRequestedToolName() const { return m_toolName; }

  // used to handle viewer navigation state
  void setSpacePressed(bool pressed);
  void setShiftPressed(bool pressed);
  void setCtrlPressed(bool pressed);
  bool isSpacePressed() const { return m_spacePressed; }
  bool isShiftPressed() const { return m_shiftPressed; }
  bool isCtrlPressed() const { return m_ctrlPressed; }

  // used to change tool for a short while (e.g. while keeping pressed a
  // short-key)
  void storeTool();
  void restoreTool();

  // used to set a tool that is not listed in the toolbar (e.g. the
  // ShiftTraceTool).
  void setPseudoTool(QString name);
  void unsetPseudoTool();
  void setToolBusy(bool value);
  bool isToolBusy() { return m_toolIsBusy; }

  /*! Notify tool parameters change (out of toolOption bar).*/
  void notifyToolChanged() { emit toolChanged(); }
  void notifyToolOptionsBoxChanged() { emit toolOptionsBoxChanged(); }
  void notifyToolCursorTypeChanged() { emit toolCursorTypeChanged(); }

  void notifyToolComboBoxListChanged(std::string id) {
    emit toolComboBoxListChanged(id);
  }

private:
  void updateNavigationState();

signals:
  void toolComboBoxListChanged(std::string);
  void toolSwitched();
  void toolChanged();
  void toolOptionsBoxChanged();
  void toolEditingFinished();
  // used for changing the tool cursor when the options changed with short cut
  // keys assigned for tool options.
  void toolCursorTypeChanged();

public slots:
  // void changeTool(QAction* action);
  void onImageChanged(TImage::Type type);
  void updateMatrix();
};

#endif  // TOOLEHANDLE_H
