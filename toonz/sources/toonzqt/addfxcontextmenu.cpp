

#include "toonzqt/addfxcontextmenu.h"

// TnzQt includes
#include "toonzqt/fxselection.h"

// TnzLib includes
#include "toonz/toonzfolders.h"
#include "toonz/txsheethandle.h"
#include "toonz/tcolumnhandle.h"
#include "toonz/tframehandle.h"
#include "toonz/tfxhandle.h"
#include "toonz/fxcommand.h"
#include "toonz/txsheet.h"
#include "toonz/fxdag.h"
#include "toonz/tapplication.h"
#include "toonz/txshzeraryfxcolumn.h"
#include "toonz/tcolumnfx.h"
#include "tw/stringtable.h"

// TnzBase includes
#include "texternfx.h"
#include "tmacrofx.h"
#include "tfxattributes.h"

// TnzCore includes
#include "tstream.h"
#include "tsystem.h"
#include "tconst.h"

// Qt includes
#include <QMenu>
#include <QAction>
#include <QStack>

#include "pluginhost.h"
#include <map>
#include <string>

std::map<std::string, PluginInformation *> plugin_dict_;

namespace {

TFx *createFxByName(std::string fxId) {
  if (fxId.find("_ext_") == 0) return TExternFx::create(fxId.substr(5));
  if (fxId.find("_plg_") == 0) {
    std::string id = fxId.substr(5);
    std::map<std::string, PluginInformation *>::iterator it =
        plugin_dict_.find(id);
    if (it != plugin_dict_.end()) {
      RasterFxPluginHost *plugin = new RasterFxPluginHost(it->second);
      plugin->notify();
      return plugin;
    }
    return NULL;
  } else {
    return TFx::create(fxId);
  }
}

//-----------------------------------------------------------------------------

TFx *createPresetFxByName(TFilePath path) {
  std::string id = path.getParentDir().getName();
  TFx *fx        = createFxByName(id);
  if (fx) {
    TIStream is(path);
    fx->loadPreset(is);
    fx->setName(path.getWideName());
  }
  return fx;
}

//-----------------------------------------------------------------------------

TFx *createMacroFxByPath(TFilePath path, TXsheet *xsheet) {
  try {
    TIStream is(path);
    TPersist *p = 0;
    is >> p;
    TMacroFx *fx = dynamic_cast<TMacroFx *>(p);
    if (!fx) return 0;
    fx->setName(path.getWideName());
    // Assign a unic ID to each fx in the macro!
    if (!xsheet) return fx;
    FxDag *fxDag = xsheet->getFxDag();
    if (!fxDag) return fx;
    std::vector<TFxP> fxs;
    fxs = fx->getFxs();
    QMap<std::wstring, std::wstring> oldNewId;
    int i;
    for (i = 0; i < (int)fxs.size(); i++) {
      std::wstring oldId = fxs[i]->getFxId();
      fxDag->assignUniqueId(fxs[i].getPointer());
      std::wstring newId = fxs[i]->getFxId();
      oldNewId[oldId]    = newId;

      // cambiando l'id degli effetti interni di una macro si rompono i legami
      // tra il nome della porta
      // e la porta a cui e' legato: devo cambiare i nomei delle porte e
      // rimapparli all'interno della macro
      int j;
      for (j = 0; j < fx->getInputPortCount(); j++) {
        QString inputName = QString::fromStdString(fx->getInputPortName(j));
        if (inputName.endsWith(QString::fromStdWString(oldId))) {
          QString newInputName = inputName;
          newInputName.replace(QString::fromStdWString(oldId),
                               QString::fromStdWString(newId));
          fx->renamePort(inputName.toStdString(), newInputName.toStdString());
        }
      }
    }

    return fx;
  } catch (...) {
    return 0;
  }
}

//-----------------------------------------------------------------------------

TFx *createFx(QAction *action, TXsheetHandle *xshHandle) {
  TXsheet *xsh = xshHandle->getXsheet();
  QString text = action->data().toString();
  if (text.isEmpty()) return 0;

  TFx *fx = 0;

  TFilePath path = TFilePath(text.toStdWString());

  if (TFileStatus(path).doesExist() &&
      TFileStatus(path.getParentDir()).isDirectory()) {
    std::string folder = path.getParentDir().getName();
    if (folder == "macroFx")  // have to load a Macro
      fx = createMacroFxByPath(path, xsh);
    else {
      folder = path.getParentDir().getParentDir().getName();
      if (folder == "presets")  // have to load a preset
        fx = createPresetFxByName(path);
    }
  } else
    fx = createFxByName(text.toStdString());

  return fx;
}

}  // namespace

//***************************************************
//
// AddFxContextMenu
//
//***************************************************

AddFxContextMenu::AddFxContextMenu()
    : QObject(), m_app(0), m_currentCursorScenePos(0, 0), m_againCommand(0) {
  m_fxListPath = TFilePath(ToonzFolder::getProfileFolder() + "layouts" + "fxs" +
                           "fxs.lst");
  m_presetPath = TFilePath(ToonzFolder::getFxPresetFolder() + "presets");

  m_insertMenu         = new QMenu(tr("Insert FX"), 0);
  m_insertActionGroup  = new QActionGroup(m_insertMenu);
  m_addMenu            = new QMenu(tr("Add FX"), 0);
  m_addActionGroup     = new QActionGroup(m_addMenu);
  m_replaceMenu        = new QMenu(tr("Replace FX"), 0);
  m_replaceActionGroup = new QActionGroup(m_replaceMenu);

  connect(m_insertActionGroup, SIGNAL(triggered(QAction *)), this,
          SLOT(onInsertFx(QAction *)));
  connect(m_addActionGroup, SIGNAL(triggered(QAction *)), this,
          SLOT(onAddFx(QAction *)));
  connect(m_replaceActionGroup, SIGNAL(triggered(QAction *)), this,
          SLOT(onReplaceFx(QAction *)));

  fillMenus();
}

//---------------------------------------------------
static void clear_all_plugins() {
  for (std::map<std::string, PluginInformation *>::iterator it =
           plugin_dict_.begin();
       it != plugin_dict_.end(); ++it) {
    it->second->release();
  }
  plugin_dict_.clear();
}

AddFxContextMenu::~AddFxContextMenu() { clear_all_plugins(); }

//---------------------------------------------------

void AddFxContextMenu::setApplication(TApplication *app) {
  m_app = app;

  if (TFxHandle *fxHandle = app->getCurrentFx()) {
    connect(fxHandle, SIGNAL(fxPresetSaved()), this, SLOT(onFxPresetHandled()));
    connect(fxHandle, SIGNAL(fxPresetRemoved()), this,
            SLOT(onFxPresetHandled()));
  }
}

//---------------------------------------------------

void AddFxContextMenu::fillMenus() {
  loadFxs();
  loadMacro();
}

//---------------------------------------------------

static void scan_all_plugins(const std::string &basedir, QObject *listener) {
  // clear_all_plugins();
  new PluginLoadController(basedir, listener);
}

void AddFxContextMenu::result(PluginInformation *pi) {
  /* slot receives PluginInformation on the main thread たぶん */
  printf("AddFxContextMenu::result() pi:%p\n", pi);
  /* addfxcontextmenu.cpp の dict に登録する */
  if (pi)
    plugin_dict_.insert(
        std::pair<std::string, PluginInformation *>(pi->desc_->id_, pi));
  // RasterFxPluginHost* plug = new RasterFxPluginHost(pi);
  // pi->handler_->create(plug);
}

void AddFxContextMenu::fixup() { loadFxPluginGroup(); }

void AddFxContextMenu::loadFxs() {
  TIStream is(m_fxListPath);

  try {
    std::string tagName;
    if (is.matchTag(tagName) && tagName == "fxs") {
      loadFxGroup(&is);
      is.closeChild();
    }
  } catch (...) {
  }

  scan_all_plugins("", this);
}

//---------------------------------------------------

void AddFxContextMenu::loadFxPluginGroup() {
  QString groupName = QString::fromStdString("Plugins");

  std::unique_ptr<QMenu> insertFxGroup(new QMenu(groupName, m_insertMenu));
  std::unique_ptr<QMenu> addFxGroup(new QMenu(groupName, m_addMenu));
  std::unique_ptr<QMenu> replaceFxGroup(new QMenu(groupName, m_replaceMenu));

  loadFxPlugins(insertFxGroup.get(), addFxGroup.get(), replaceFxGroup.get());

  if (!insertFxGroup->isEmpty()) m_insertMenu->addMenu(insertFxGroup.release());
  if (!addFxGroup->isEmpty()) m_addMenu->addMenu(addFxGroup.release());
  if (!replaceFxGroup->isEmpty())
    m_replaceMenu->addMenu(replaceFxGroup.release());
}

void AddFxContextMenu::loadFxGroup(TIStream *is) {
  while (!is->eos()) {
    std::string tagName;
    if (is->matchTag(tagName)) {
      QString groupName = QString::fromStdString(tagName);

      std::unique_ptr<QMenu> insertFxGroup(new QMenu(groupName, m_insertMenu));
      std::unique_ptr<QMenu> addFxGroup(new QMenu(groupName, m_addMenu));
      std::unique_ptr<QMenu> replaceFxGroup(
          new QMenu(groupName, m_replaceMenu));

      loadFx(is, insertFxGroup.get(), addFxGroup.get(), replaceFxGroup.get());

      if (!insertFxGroup->isEmpty())
        m_insertMenu->addMenu(insertFxGroup.release());
      if (!addFxGroup->isEmpty()) m_addMenu->addMenu(addFxGroup.release());
      if (!replaceFxGroup->isEmpty())
        m_replaceMenu->addMenu(replaceFxGroup.release());

      is->closeChild();
    }
  }
}

//---------------------------------------------------
void AddFxContextMenu::loadFxPlugins(QMenu *insertFxGroup, QMenu *addFxGroup,
                                     QMenu *replaceFxGroup) {
  // list vendors
  std::vector<std::string> vendors;
  for (auto &&plugin : plugin_dict_) {
    PluginDescription *desc = plugin.second->desc_;
    vendors.push_back(desc->vendor_);
  }
  std::sort(std::begin(vendors), std::end(vendors));

  // add vendor folders
  std::map<std::string, QMenu *> insVendors;
  std::map<std::string, QMenu *> addVendors;
  std::map<std::string, QMenu *> repVendors;
  for (std::string vendor : vendors) {
    std::map<std::string, QMenu *>::iterator v = insVendors.find(vendor);
    if (v == insVendors.end()) {
      QString vendorQStr = QString::fromStdString(vendor);
      insVendors.insert(
          std::make_pair(vendor, insertFxGroup->addMenu(vendorQStr)));
      addVendors.insert(
          std::make_pair(vendor, addFxGroup->addMenu(vendorQStr)));
      repVendors.insert(
          std::make_pair(vendor, replaceFxGroup->addMenu(vendorQStr)));
    }
  }

  // add actions
  for (auto &&plugin : plugin_dict_) {
    PluginDescription *desc = plugin.second->desc_;
    QString label           = QString::fromStdString(desc->name_);

    QAction *insertAction  = new QAction(label, insertFxGroup);
    QAction *addAction     = new QAction(label, addFxGroup);
    QAction *replaceAction = new QAction(label, replaceFxGroup);

    insertAction->setData(
        QVariant("_plg_" + QString::fromStdString(desc->id_)));
    addAction->setData(QVariant("_plg_" + QString::fromStdString(desc->id_)));
    replaceAction->setData(
        QVariant("_plg_" + QString::fromStdString(desc->id_)));

    (*insVendors.find(desc->vendor_)).second->addAction(insertAction);
    (*addVendors.find(desc->vendor_)).second->addAction(addAction);
    (*repVendors.find(desc->vendor_)).second->addAction(replaceAction);

    m_insertActionGroup->addAction(insertAction);
    m_addActionGroup->addAction(addAction);
    m_replaceActionGroup->addAction(replaceAction);
  }

  // sort actions
  auto const comp = [](QAction *lhs, QAction *rhs) {
    return lhs->text() < rhs->text();
  };

  for (auto &&ins : insVendors) {
    QList<QAction *> actions = ins.second->actions();
    ins.second->clear();
    std::sort(actions.begin(), actions.end(), comp);
    ins.second->addActions(actions);
  }

  for (auto &&ins : addVendors) {
    QList<QAction *> actions = ins.second->actions();
    ins.second->clear();
    std::sort(actions.begin(), actions.end(), comp);
    ins.second->addActions(actions);
  }

  for (auto &&ins : repVendors) {
    QList<QAction *> actions = ins.second->actions();
    ins.second->clear();
    std::sort(actions.begin(), actions.end(), comp);
    ins.second->addActions(actions);
  }
}

void AddFxContextMenu::loadFx(TIStream *is, QMenu *insertFxGroup,
                              QMenu *addFxGroup, QMenu *replaceFxGroup) {
  while (!is->eos()) {
    std::string fxName;
    *is >> fxName;

    if (!fxName.empty()) {
      if (!loadPreset(fxName, insertFxGroup, addFxGroup, replaceFxGroup)) {
        QString translatedName =
            QString::fromStdWString(TStringTable::translate(fxName));

        QAction *insertAction  = new QAction(translatedName, insertFxGroup);
        QAction *addAction     = new QAction(translatedName, addFxGroup);
        QAction *replaceAction = new QAction(translatedName, replaceFxGroup);

        insertAction->setData(QVariant(QString::fromStdString(fxName)));
        addAction->setData(QVariant(QString::fromStdString(fxName)));
        replaceAction->setData(QVariant(QString::fromStdString(fxName)));

        insertFxGroup->addAction(insertAction);
        addFxGroup->addAction(addAction);
        replaceFxGroup->addAction(replaceAction);

        m_insertActionGroup->addAction(insertAction);
        m_addActionGroup->addAction(addAction);
        m_replaceActionGroup->addAction(replaceAction);
      }
    }
  }
}

//---------------------------------------------------

bool AddFxContextMenu::loadPreset(const std::string &name, QMenu *insertFxGroup,
                                  QMenu *addFxGroup, QMenu *replaceFxGroup) {
  TFilePath presetsFilepath(m_presetPath + name);
  if (TFileStatus(presetsFilepath).isDirectory()) {
    TFilePathSet presets = TSystem::readDirectory(presetsFilepath, false);
    if (!presets.empty()) {
      QMenu *inserMenu =
          new QMenu(QString::fromStdWString(TStringTable::translate(name)),
                    insertFxGroup);
      insertFxGroup->addMenu(inserMenu);
      QMenu *addMenu = new QMenu(
          QString::fromStdWString(TStringTable::translate(name)), addFxGroup);
      addFxGroup->addMenu(addMenu);
      QMenu *replaceMenu =
          new QMenu(QString::fromStdWString(TStringTable::translate(name)),
                    replaceFxGroup);
      replaceFxGroup->addMenu(replaceMenu);

      // This is a workaround to set the bold style to the first element of this
      // menu
      // Setting a font directly to a QAction is not enough; style sheet
      // definitions
      // preval over QAction font settings.
      inserMenu->setObjectName("fxMenu");
      addMenu->setObjectName("fxMenu");
      replaceMenu->setObjectName("fxMenu");

      QAction *insertAction = new QAction(
          QString::fromStdWString(TStringTable::translate(name)), inserMenu);
      QAction *addAction = new QAction(
          QString::fromStdWString(TStringTable::translate(name)), addMenu);
      QAction *replaceAction = new QAction(
          QString::fromStdWString(TStringTable::translate(name)), replaceMenu);

      insertAction->setCheckable(true);
      addAction->setCheckable(true);
      replaceAction->setCheckable(true);

      insertAction->setData(QVariant(QString::fromStdString(name)));
      addAction->setData(QVariant(QString::fromStdString(name)));
      replaceAction->setData(QVariant(QString::fromStdString(name)));

      inserMenu->addAction(insertAction);
      addMenu->addAction(addAction);
      replaceMenu->addAction(replaceAction);

      m_insertActionGroup->addAction(insertAction);
      m_addActionGroup->addAction(addAction);
      m_replaceActionGroup->addAction(replaceAction);

      for (TFilePathSet::iterator it2 = presets.begin(); it2 != presets.end();
           ++it2) {
        TFilePath presetName = *it2;
        QString qPresetName = QString::fromStdWString(presetName.getWideName());

        insertAction  = new QAction(qPresetName, inserMenu);
        addAction     = new QAction(qPresetName, addMenu);
        replaceAction = new QAction(qPresetName, replaceMenu);

        insertAction->setData(
            QVariant(QString::fromStdWString(presetName.getWideString())));
        addAction->setData(
            QVariant(QString::fromStdWString(presetName.getWideString())));
        replaceAction->setData(
            QVariant(QString::fromStdWString(presetName.getWideString())));

        inserMenu->addAction(insertAction);
        addMenu->addAction(addAction);
        replaceMenu->addAction(replaceAction);

        m_insertActionGroup->addAction(insertAction);
        m_addActionGroup->addAction(addAction);
        m_replaceActionGroup->addAction(replaceAction);
      }
      return true;
    } else
      return false;
  } else
    return false;
}

//---------------------------------------------------

void AddFxContextMenu::loadMacro() {
  TFilePath macroDir = m_presetPath + TFilePath("macroFx");
  try {
    if (TFileStatus(macroDir).isDirectory()) {
      TFilePathSet macros = TSystem::readDirectory(macroDir);
      if (macros.empty()) return;

      QMenu *insertMacroMenu  = new QMenu("Macro", m_insertMenu);
      QMenu *addMacroMenu     = new QMenu("Macro", m_addMenu);
      QMenu *replaceMacroMenu = new QMenu("Macro", m_replaceMenu);

      m_insertMenu->addMenu(insertMacroMenu);
      m_addMenu->addMenu(addMacroMenu);
      m_replaceMenu->addMenu(replaceMacroMenu);

      for (TFilePathSet::iterator it = macros.begin(); it != macros.end();
           ++it) {
        TFilePath macroPath = *it;
        QString name        = QString::fromStdWString(macroPath.getWideName());

        QAction *insertAction  = new QAction(name, insertMacroMenu);
        QAction *addAction     = new QAction(name, addMacroMenu);
        QAction *replaceAction = new QAction(name, replaceMacroMenu);

        insertAction->setData(
            QVariant(QString::fromStdWString(macroPath.getWideString())));
        addAction->setData(
            QVariant(QString::fromStdWString(macroPath.getWideString())));
        replaceAction->setData(
            QVariant(QString::fromStdWString(macroPath.getWideString())));

        insertMacroMenu->addAction(insertAction);
        addMacroMenu->addAction(addAction);
        replaceMacroMenu->addAction(replaceAction);

        m_insertActionGroup->addAction(insertAction);
        m_addActionGroup->addAction(addAction);
        m_replaceActionGroup->addAction(replaceAction);
      }
    }
  } catch (...) {
  }
}

//---------------------------------------------------

void AddFxContextMenu::onInsertFx(QAction *action) {
  if (action->isCheckable() && action->isChecked()) action->setChecked(false);
  TFx *fx = createFx(action, m_app->getCurrentXsheet());
  if (fx) {
    QList<TFxP> fxs               = m_selection->getFxs();
    QList<TFxCommand::Link> links = m_selection->getLinks();
    TFxCommand::insertFx(fx, fxs, links, m_app,
                         m_app->getCurrentColumn()->getColumnIndex(),
                         m_app->getCurrentFrame()->getFrameIndex());
    m_app->getCurrentXsheet()->notifyXsheetChanged();
    // memorize the latest operation
    m_app->getCurrentFx()->setPreviousActionString(QString("I ") +
                                                   action->data().toString());
  }
}

//---------------------------------------------------

void AddFxContextMenu::onAddFx(QAction *action) {
  if (action->isCheckable() && action->isChecked()) action->setChecked(false);

  TFx *fx = createFx(action, m_app->getCurrentXsheet());
  if (fx) {
    QList<TFxP> fxs = m_selection->getFxs();
    // try to add node at cursor position
    if (m_currentCursorScenePos.x() != 0 || m_currentCursorScenePos.y() != 0) {
      fx->getAttributes()->setDagNodePos(
          TPointD(m_currentCursorScenePos.x(), m_currentCursorScenePos.y()));
      m_currentCursorScenePos.setX(0);
      m_currentCursorScenePos.setY(0);
    }

    int colId = m_app->getCurrentColumn()->getColumnIndex();
    // prevent crash when the camera column is selected
    if (colId < 0) colId = 0;

    // the signal xsheetChanged is to be emitted in this function
    TFxCommand::addFx(fx, fxs, m_app, colId,
                      m_app->getCurrentFrame()->getFrameIndex());

    // move the zerary fx node to the clicked position
    if (fx->isZerary() &&
        fx->getAttributes()->getDagNodePos() != TConst::nowhere) {
      TXsheet *xsh               = m_app->getCurrentXsheet()->getXsheet();
      TXshZeraryFxColumn *column = xsh->getColumn(colId)->getZeraryFxColumn();
      if (column)
        column->getZeraryColumnFx()->getAttributes()->setDagNodePos(
            fx->getAttributes()->getDagNodePos());
      m_app->getCurrentXsheet()->notifyXsheetChanged();
    }

    // memorize the latest operation
    m_app->getCurrentFx()->setPreviousActionString(QString("A ") +
                                                   action->data().toString());
  }
}

//---------------------------------------------------

void AddFxContextMenu::onReplaceFx(QAction *action) {
  if (action->isCheckable() && action->isChecked()) action->setChecked(false);
  TFx *fx = createFx(action, m_app->getCurrentXsheet());
  if (fx) {
    QList<TFxP> fxs = m_selection->getFxs();
    TFxCommand::replaceFx(fx, fxs, m_app->getCurrentXsheet(),
                          m_app->getCurrentFx());
    m_app->getCurrentXsheet()->notifyXsheetChanged();
    // memorize the latest operation
    m_app->getCurrentFx()->setPreviousActionString(QString("R ") +
                                                   action->data().toString());
  }
}

//---------------------------------------------------

void AddFxContextMenu::onFxPresetHandled() {
  m_insertMenu->clear();
  m_addMenu->clear();
  m_replaceMenu->clear();
  fillMenus();
}

//---------------------------------------------------
/*! repeat the last fx creation command done in the schematic.
    arrgument "command" is sum of the ids of available commands(Insert, Add,
   Replace)
*/
QAction *AddFxContextMenu::getAgainCommand(int command) {
  QString commandName = m_app->getCurrentFx()->getPreviousActionString();
  // return if the last action is not registered
  if (commandName.isEmpty()) return 0;

  // classify action by commandName
  Commands com;
  QString commandStr;
  if (commandName.startsWith("I ")) {
    com        = Insert;
    commandStr = tr("Insert ");
  } else if (commandName.startsWith("A ")) {
    com        = Add;
    commandStr = tr("Add ");
  } else if (commandName.startsWith("R ")) {
    com        = Replace;
    commandStr = tr("Replace ");
  } else
    return 0;

  // return if the action is not available
  if (!(command & com)) return 0;

  QString fxStr = commandName.right(commandName.size() - 2);
  QString translatedCommandName =
      commandStr +
      QString::fromStdWString(TStringTable::translate(fxStr.toStdString()));
  // return the action if the command is the exactly same
  if (m_againCommand && commandName == m_againCommand->data().toString())
    return m_againCommand;

  // create an action
  if (!m_againCommand) {
    m_againCommand = new QAction();
    connect(m_againCommand, SIGNAL(triggered()), this, SLOT(onAgainCommand()));
  }
  // set the action name
  m_againCommand->setText(translatedCommandName);
  m_againCommand->setData(commandName);
  return m_againCommand;
}

//---------------------------------------------------
/*! change the command behavior according to the command name
 */
void AddFxContextMenu::onAgainCommand() {
  QString commandName = m_againCommand->data().toString();
  m_againCommand->setData(commandName.right(commandName.size() - 2));
  if (commandName.startsWith("I ")) {
    onInsertFx(m_againCommand);
  } else if (commandName.startsWith("A ")) {
    onAddFx(m_againCommand);
  } else if (commandName.startsWith("R ")) {
    onReplaceFx(m_againCommand);
  }
}
