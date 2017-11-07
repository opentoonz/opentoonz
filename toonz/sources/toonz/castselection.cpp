

#include "castselection.h"
#include "castviewer.h"
#include "menubarcommandids.h"
#include "toonzqt/icongenerator.h"
#include "toonzqt/gutil.h"

#include "toonz/toonzscene.h"
#include "toonz/txshlevel.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/txshpalettelevel.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/tscenehandle.h"
#include "toonz/sceneproperties.h"
#include "tapp.h"
#include "toonz/stage2.h"
#include "tsystem.h"
#include "toonz/txshsoundlevel.h"
#include "toonz/preferences.h"

//=============================================================================
//
// CastSelection
//
//-----------------------------------------------------------------------------

CastSelection::CastSelection() : m_browser(0) {}

//-----------------------------------------------------------------------------

CastSelection::~CastSelection() {}

//-----------------------------------------------------------------------------

void CastSelection::getSelectedLevels(std::vector<TXshLevel *> &levels) {
  assert(m_browser);
  CastItems const &castItems = m_browser->getCastItems();
  for (int i = 0; i < castItems.getItemCount(); i++) {
    if (!isSelected(i)) continue;
    TXshLevel *level  = castItems.getItem(i)->getSimpleLevel();
    if (!level) level = castItems.getItem(i)->getPaletteLevel();
    if (!level) level = castItems.getItem(i)->getSoundLevel();
    if (level) levels.push_back(level);
  }
}

//-----------------------------------------------------------------------------

void CastSelection::enableCommands() {
  DvItemSelection::enableCommands();
  assert(m_browser);
  if (m_browser) {
    enableCommand(m_browser, MI_ExposeResource, &CastBrowser::expose);
    enableCommand(m_browser, MI_EditLevel, &CastBrowser::edit);
    // enableCommand(m_browser, MI_ConvertToVectors, &CastBrowser::vectorize);
    enableCommand(m_browser, MI_ShowFolderContents,
                  &CastBrowser::showFolderContents);
  }
}

// TODO: da qui in avanti: spostare in un altro file

//=============================================================================
//
// CastBrowser::LevelItem
//
//-----------------------------------------------------------------------------

QString LevelCastItem::getName() const {
  return QString::fromStdWString(m_level->getName());
}

//-----------------------------------------------------------------------------

QString LevelCastItem::getToolTip() const {
  TFilePath path = m_level->getPath();
  return QString::fromStdWString(path.withoutParentDir().getWideString());
}

//-----------------------------------------------------------------------------

int LevelCastItem::getFrameCount() const { return m_level->getFrameCount(); }

//-----------------------------------------------------------------------------

QPixmap LevelCastItem::getPixmap(bool isSelected) const {
  TXshSimpleLevel *sl = m_level->getSimpleLevel();
  if (!sl) return QPixmap();
  ToonzScene *scene = sl->getScene();
  assert(scene);
  if (!scene) return QPixmap();
  bool onDemand = false;
  if (Preferences::instance()->getColumnIconLoadingPolicy() ==
      Preferences::LoadOnDemand)
    onDemand   = !isSelected;
  QPixmap icon = IconGenerator::instance()->getIcon(sl, sl->getFirstFid(),
                                                    false, onDemand);
  return scalePixmapKeepingAspectRatio(icon, m_itemPixmapSize, Qt::transparent);
}

//-----------------------------------------------------------------------------

bool LevelCastItem::exists() const {
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  return TSystem::doesExistFileOrLevel(
             scene->decodeFilePath(m_level->getPath())) ||
         !getPixmap(false).isNull();
}

//-------------------------------------------------------

TXshSimpleLevel *LevelCastItem::getSimpleLevel() const {
  return m_level ? m_level->getSimpleLevel() : 0;
}

//=============================================================================
//
// CastBrowser::SoundItem
//
//-----------------------------------------------------------------------------

QString SoundCastItem::getName() const {
  return QString::fromStdWString(m_soundLevel->getName());
}

//-----------------------------------------------------------------------------

QString SoundCastItem::getToolTip() const {
  TFilePath path = m_soundLevel->getPath();
  return QString::fromStdWString(path.withoutParentDir().getWideString());
}
//-----------------------------------------------------------------------------

int SoundCastItem::getFrameCount() const {
  return m_soundLevel->getFrameCount();
}
//-----------------------------------------------------------------------------

QPixmap SoundCastItem::getPixmap(bool isSelected) const {
  static QPixmap loudspeaker(svgToPixmap(
      ":Resources/audio.svg", m_itemPixmapSize, Qt::KeepAspectRatio));
  return loudspeaker;
}

//-----------------------------------------------------------------------------

bool SoundCastItem::exists() const {
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  return TSystem::doesExistFileOrLevel(
      scene->decodeFilePath(m_soundLevel->getPath()));
}

//=============================================================================
//
// CastBrowser::PaletteCastItem
//
//-----------------------------------------------------------------------------

QString PaletteCastItem::getName() const {
  return QString::fromStdWString(m_paletteLevel->getName());
}
//-----------------------------------------------------------------------------

QString PaletteCastItem::getToolTip() const {
  TFilePath path = m_paletteLevel->getPath();
  return QString::fromStdWString(path.withoutParentDir().getWideString());
}
//-----------------------------------------------------------------------------

int PaletteCastItem::getFrameCount() const {
  return m_paletteLevel->getFrameCount();
}
//-----------------------------------------------------------------------------

QPixmap PaletteCastItem::getPixmap(bool isSelected) const {
  static QPixmap palette(svgToPixmap(":Resources/paletteicon.svg",
                                     m_itemPixmapSize, Qt::KeepAspectRatio));
  return palette;
}

bool PaletteCastItem::exists() const {
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  return TSystem::doesExistFileOrLevel(
      scene->decodeFilePath(m_paletteLevel->getPath()));
}

//=============================================================================
//
// CastItems
//
//-----------------------------------------------------------------------------

CastItems::CastItems() : QMimeData() {}

//-----------------------------------------------------------------------------

CastItems::~CastItems() { clear(); }

//-----------------------------------------------------------------------------

void CastItems::clear() { clearPointerContainer(m_items); }

//-----------------------------------------------------------------------------

void CastItems::addItem(CastItem *item) { m_items.push_back(item); }

//-----------------------------------------------------------------------------

CastItem *CastItems::getItem(int index) const {
  assert(0 <= index && index < (int)m_items.size());
  return m_items[index];
}

//-----------------------------------------------------------------------------

bool CastItems::hasFormat(const QString &mimeType) const {
  return mimeType == getMimeFormat();
}

//-----------------------------------------------------------------------------

QString CastItems::getMimeFormat() { return "application/vnd.toonz.levels"; }

//-----------------------------------------------------------------------------

QStringList CastItems::formats() const {
  return QStringList(QString("application/vnd.toonz.levels"));
}

//-----------------------------------------------------------------------------

CastItems *CastItems::getSelectedItems(const std::set<int> &indices) const {
  CastItems *c = new CastItems();
  std::set<int>::const_iterator it;
  for (it = indices.begin(); it != indices.end(); ++it) {
    int index = *it;
    if (0 <= index && index < getItemCount())
      c->addItem(getItem(index)->clone());
  }
  return c;
}
