

#include "toonz/scriptbinding_scene.h"
#include "toonz/scriptbinding_level.h"
#include "toonz/scriptbinding_files.h"
#include "toonz/txshleveltypes.h"

#include "tsystem.h"
#include "tfiletype.h"

#include "toonz/tproject.h"
#include "toonz/toonzscene.h"
#include "toonz/txsheet.h"
#include "toonz/txshcell.h"
#include "toonz/levelset.h"

namespace TScriptBinding {

Scene::Scene() {
  m_scene = new ToonzScene();
  TProjectManager::instance()->initializeScene(m_scene);
}

Scene::~Scene() { delete m_scene; }

QScriptValue Scene::ctor(QScriptContext *context, QScriptEngine *engine) {
  QScriptValue obj = create(engine, new Scene());
  if (context->argumentCount() == 1) {
    return obj.property("load").call(obj, context->argumentsObject());
  }
  return obj;
}

QScriptValue Scene::toString() {
  return QString("Scene (%1 frames)").arg(getFrameCount());
}

int Scene::getFrameCount() { return m_scene->getFrameCount(); }

int Scene::getColumnCount() { return m_scene->getXsheet()->getColumnCount(); }

QScriptValue Scene::load(const QScriptValue &fpArg) {
  TFilePath fp;
  QScriptValue err = checkFilePath(context(), fpArg, fp);
  if (err.isError()) return err;
  if (!fp.isAbsolute())
    fp = TProjectManager::instance()->getCurrentProject()->getScenesPath() + fp;
  try {
    if (!TSystem::doesExistFileOrLevel(fp)) {
      return context()->throwError(
          tr("File %1 doesn't exist").arg(fpArg.toString()));
    }
    m_scene->load(fp);
    return context()->thisObject();
  } catch (...) {
    return context()->throwError(
        tr("Exception reading %1").arg(fpArg.toString()));
  }
}

QScriptValue Scene::save(const QScriptValue &fpArg) {
  TFilePath fp;
  QScriptValue err = checkFilePath(context(), fpArg, fp);
  if (err.isError()) return err;
  if (!fp.isAbsolute())
    fp = TProjectManager::instance()->getCurrentProject()->getScenesPath() + fp;
  try {
    m_scene->save(fp);
    return context()->thisObject();
  } catch (...) {
    return context()->throwError(
        tr("Exception writing %1").arg(fpArg.toString()));
  }
}

QScriptValue Scene::getLevels() const {
  QScriptValue result = engine()->newArray();
  qint32 index        = 0;
  std::vector<TXshLevel *> levels;
  m_scene->getLevelSet()->listLevels(levels);
  for (std::vector<TXshLevel *>::iterator it = levels.begin();
       it != levels.end(); ++it) {
    TXshSimpleLevel *sl = (*it)->getSimpleLevel();
    if (sl) {
      Level *level = new Level(sl);
      result.setProperty(index++, create(engine(), level));
    }
  }
  return result;
}

QScriptValue Scene::getLevel(const QString &name) const {
  TXshLevel *xl       = m_scene->getLevelSet()->getLevel(name.toStdWString());
  TXshSimpleLevel *sl = xl ? xl->getSimpleLevel() : 0;
  if (sl) {
    Level *level = new Level(sl);
    return create(engine(), level);
  } else
    return QScriptValue();
}

QScriptValue Scene::newLevel(const QString &levelTypeStr,
                             const QString &levelName) const {
  int levelType = NO_XSHLEVEL;
  if (levelTypeStr == "Vector")
    levelType = PLI_XSHLEVEL;
  else if (levelTypeStr == "ToonzRaster")
    levelType = TZP_XSHLEVEL;
  else if (levelTypeStr == "Raster")
    levelType = OVL_XSHLEVEL;
  if (levelType == NO_XSHLEVEL)
    return context()->throwError(
        tr("Bad level type (%1): must be Vector,Raster or ToonzRaster")
            .arg(levelTypeStr));

  if (m_scene->getLevelSet()->hasLevel(levelName.toStdWString()))
    return context()->throwError(
        tr("Can't add the level: name(%1) is already used").arg(levelName));

  TXshLevel *xl = m_scene->createNewLevel(levelType, levelName.toStdWString());
  xl->getSimpleLevel()->setDirtyFlag(true);
  return create(engine(), new Level(xl->getSimpleLevel()));
}

QScriptValue Scene::loadLevel(const QString &levelName,
                              const QScriptValue &fpArg) const {
  if (m_scene->getLevelSet()->hasLevel(levelName.toStdWString()))
    return context()->throwError(
        tr("Can't add the level: name(%1) is already used").arg(levelName));
  TFilePath fp;
  QScriptValue err = checkFilePath(context(), fpArg, fp);
  if (err.isError()) return err;
  TFileType::Type type = TFileType::getInfo(fp);
  if ((type & TFileType::VIEWABLE) == 0)
    return context()->throwError(
        tr("Can't load this kind of file as a level : %1")
            .arg(fpArg.toString()));
  TXshLevel *xl = m_scene->loadLevel(fp);
  if (!xl || !xl->getSimpleLevel())
    return context()->throwError(
        tr("Could not load level %1").arg(fpArg.toString()));
  return create(engine(), new Level(xl->getSimpleLevel()));
}

QString Scene::doSetCell(int row, int col, const QScriptValue &levelArg,
                         const QScriptValue &fidArg) {
  if (row < 0 || col < 0) {
    return "Bad row/col values";
  }

  QString err;
  TXshCell cell;
  cell.m_frameId = Level::getFid(fidArg, err);
  if (err != "") return err;
  Level *level = qscriptvalue_cast<Level *>(levelArg);
  if (level) {
    TXshSimpleLevel *sl = level->getSimpleLevel();
    TXshLevel *xl       = m_scene->getLevelSet()->getLevel(sl->getName());
    if (!xl || xl->getSimpleLevel() != sl) {
      return tr("Level is not included in the scene : %1")
          .arg(levelArg.toString());
    }
    cell.m_level = sl;
  } else {
    if (!levelArg.isString())
      return tr("%1 : Expected a Level instance or a level name")
          .arg(levelArg.toString());
    QString levelName = levelArg.toString();
    TXshLevel *xl = m_scene->getLevelSet()->getLevel(levelName.toStdWString());
    if (!xl)
      return tr("Level '%1' is not included in the scene").arg(levelName);
    cell.m_level = xl;
  }
  m_scene->getXsheet()->setCell(row, col, cell);
  return "";
}

QScriptValue Scene::setCell(int row, int col, const QScriptValue &level,
                            const QScriptValue &fid) {
  QString err = doSetCell(row, col, level, fid);
  if (err != "") return context()->throwError(err);
  return context()->thisObject();
}

QScriptValue Scene::setCell(int row, int col, const QScriptValue &cellArg) {
  if (cellArg.isUndefined()) {
    if (row >= 0 && col >= 0)
      m_scene->getXsheet()->setCell(row, col, TXshCell());
  } else {
    if (!cellArg.isObject() || cellArg.property("level").isUndefined() ||
        cellArg.property("fid").isUndefined())
      return context()->throwError(
          "Third argument should be an object with attributes 'level' and "
          "'fid'");
    QScriptValue levelArg = cellArg.property("level");
    QScriptValue fidArg   = cellArg.property("fid");
    QString err           = doSetCell(row, col, levelArg, fidArg);
    if (err != "") return context()->throwError(err);
  }
  return context()->thisObject();
}

QScriptValue Scene::getCell(int row, int col) {
  TXshCell cell       = m_scene->getXsheet()->getCell(row, col);
  TXshSimpleLevel *sl = cell.getSimpleLevel();
  if (sl) {
    QScriptValue level = create(engine(), new Level(sl));
    QScriptValue fid;
    if (cell.m_frameId.getLetter() == 0)
      fid = cell.m_frameId.getNumber();
    else
      fid               = QString::fromStdString(cell.m_frameId.expand());
    QScriptValue result = engine()->newObject();
    result.setProperty("level", level);
    result.setProperty("fid", fid);
    return result;
  } else {
    return QScriptValue();
  }
}

QScriptValue Scene::insertColumn(int col) {
  m_scene->getXsheet()->insertColumn(col);
  return context()->thisObject();
}

QScriptValue Scene::deleteColumn(int col) {
  m_scene->getXsheet()->removeColumn(col);
  return context()->thisObject();
}

}  // namespace TScriptBinding
