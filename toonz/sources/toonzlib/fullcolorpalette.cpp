

#include "toonz/fullcolorpalette.h"
#include "toonz/tscenehandle.h"
#include "toonz/toonzscene.h"
#include "tsystem.h"
#include "tstream.h"
#include "tpalette.h"

//==================================================
//
// FullColorPalette
//
//==================================================

FullColorPalette::FullColorPalette()
    : m_fullcolorPalettePath("+palettes\\Raster_Drawing_Palette.tpl")
    , m_palette(0) {}

//----------------------------------------------------

FullColorPalette *FullColorPalette::instance() {
  static FullColorPalette _instance;
  return &_instance;
}

//----------------------------------------------------

FullColorPalette::~FullColorPalette() { clear(); }

//----------------------------------------------------

void FullColorPalette::clear() {
  if (m_palette) m_palette->release();
  m_palette = 0;
}

//----------------------------------------------------

TPalette *FullColorPalette::getPalette(ToonzScene *scene) {
  if (m_palette) return m_palette;
  m_palette = new TPalette();
  m_palette->addRef();
  TFilePath fullPath = scene->decodeFilePath(m_fullcolorPalettePath);
  if (!TSystem::doesExistFileOrLevel(fullPath)) {
    // Per I francesi che hanno il nome vecchio della paletta
    // Verra' caricata la vecchia ma salvata col nome nuovo!
    TFilePath app("+palettes\\fullcolorPalette.tpl");
    fullPath = scene->decodeFilePath(app);
  }
  if (TSystem::doesExistFileOrLevel(fullPath)) {
    TPalette *app = new TPalette();
    TIStream is(fullPath);
    is >> app;
    m_palette->assign(app);
    delete app;
  }
  m_palette->setPaletteName(L"Raster Drawing Palette");

  return m_palette;
}

//----------------------------------------------------

void FullColorPalette::savePalette(ToonzScene *scene) {
  if (!m_palette || !m_palette->getDirtyFlag()) return;

  TFilePath fullPath = scene->decodeFilePath(m_fullcolorPalettePath);
  if (TSystem::touchParentDir(fullPath)) {
    if (TSystem::doesExistFileOrLevel(fullPath))
      TSystem::removeFileOrLevel(fullPath);
    TOStream os(fullPath);
    os << m_palette;
    m_palette->setDirtyFlag(false);
  }
}
