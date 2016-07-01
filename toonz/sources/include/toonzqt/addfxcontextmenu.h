#pragma once

#ifndef ADDFXCONTEXTMENU_H
#define ADDFXCONTEXTMENU_H

#include "tfilepath.h"
#include <QObject>
#include <QPointF>

class QMenu;
class QActionGroup;
class TIStream;
class QAction;
class TXsheetHandle;
class TFxHandle;
class FxSelection;
class TColumnHandle;
class TFrameHandle;
class TApplication;

class PluginInformation;

//! Create Insert - Add - Replace menus for the context menu of a
//! FxSchematicScene and its items!
//! This method is used to create and keep updated the three menus.
//! Each menus contains all toonz special effect and effect presets.
class AddFxContextMenu final : public QObject {
  Q_OBJECT

  QMenu *m_insertMenu, *m_addMenu, *m_replaceMenu;
  TFilePath m_fxListPath, m_presetPath;
  QActionGroup *m_insertActionGroup, *m_addActionGroup, *m_replaceActionGroup;
  TApplication *m_app;
  FxSelection *m_selection;
  // in order to add fx at the cursor position
  QPointF m_currentCursorScenePos;
  // for reproduce the last added fx
  QAction *m_againCommand;

public:
  AddFxContextMenu();
  ~AddFxContextMenu();

  QMenu *getInsertMenu() { return m_insertMenu; }
  QMenu *getAddMenu() { return m_addMenu; }
  QMenu *getReplaceMenu() { return m_replaceMenu; }

  void setApplication(TApplication *app);
  void setSelection(FxSelection *selection) { m_selection = selection; }

  void setCurrentCursorScenePos(const QPointF &scenePos) {
    m_currentCursorScenePos = scenePos;
  }

  enum Commands { Insert = 0x1, Add = 0x2, Replace = 0x4 };
  QAction *getAgainCommand(int command);

private:
  void fillMenus();
  void loadFxs();
  void loadFxGroup(TIStream *is);
  void loadFx(TIStream *is, QMenu *insertFxGroup, QMenu *addFxGroup,
              QMenu *replaceFxGroup);
  bool loadPreset(const std::string &name, QMenu *insertFxGroup,
                  QMenu *addFxGroup, QMenu *replaceFxGroup);
  void loadMacro();
  void loadFxPluginGroup();
  void loadFxPlugins(QMenu *insertFxGroup, QMenu *addFxGroup,
                     QMenu *replaceFxGroup);

private slots:
  void onInsertFx(QAction *);
  void onAddFx(QAction *);
  void onReplaceFx(QAction *);
  void onFxPresetHandled();
  void onAgainCommand();
public slots:
  void result(PluginInformation *);
  void fixup();
};

#endif  // ADDFXCONTEXTMENU_H
