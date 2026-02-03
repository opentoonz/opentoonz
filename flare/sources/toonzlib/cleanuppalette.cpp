

#include "tpalette.h"
#include "toonz/cleanupcolorstyles.h"
#include "toonz/targetcolors.h"
#include "tgl.h"

#include "cleanuppalette.h"

//===================================================================

TPalette *createStandardCleanupPalette() {
  TPalette *palette    = new TPalette();
  TPalette::Page *page = palette->getPage(0);
  page->removeStyle(1);  // tolgo il black che c'e' gia' nella palette di
                         // default
  TBlackCleanupStyle *black = new TBlackCleanupStyle();
  palette->setStyle(1, black);
  page->addStyle(1);
  // page->addStyle(palette->addStyle(new TColorCleanupStyle(TPixel32::Red)));
  // page->addStyle(palette->addStyle(new TColorCleanupStyle(TPixel32::Blue)));
  black->setName(L"color_1");
  palette->addRef();
  palette->setIsCleanupPalette(true);
  return palette;
}

//-------------------------------------------------------------------

TPalette *createToonzPalette(TPalette *cleanupPalette) {
  assert(cleanupPalette);
  assert(cleanupPalette->isCleanupPalette());

  TPalette *palette = new TPalette();

  for (int i = 0; i < cleanupPalette->getPage(0)->getStyleCount(); i++) {
    int styleId = cleanupPalette->getPage(0)->getStyleId(i);
    TCleanupStyle *cs =
        dynamic_cast<TCleanupStyle *>(cleanupPalette->getStyle(styleId));
    if (!cs) continue;
    TPixel32 color = cs->getMainColor();

    while (palette->getStyleCount() < styleId) palette->addStyle(TPixel32::Red);

    if (styleId == palette->getStyleCount())
      palette->addStyle(color);
    else
      palette->setStyle(styleId, color);

    if (styleId > 1)  // 0 e 1 sono gia' nella pagina
      palette->getPage(0)->addStyle(styleId);

    assert(0 <= styleId && styleId < palette->getStyleCount());
    if (cs->getFlags()) palette->getStyle(styleId)->setFlags(cs->getFlags());
  }

  return palette;
}

//-------------------------------------------------------------------

TPalette *createToonzPalette(TPalette *cleanupPalette, int colorParamIndex) {
  assert(cleanupPalette);
  assert(cleanupPalette->isCleanupPalette());

  TPalette *palette = new TPalette();

  for (int i = 0; i < cleanupPalette->getPage(0)->getStyleCount(); i++) {
    int styleId = cleanupPalette->getPage(0)->getStyleId(i);
    TCleanupStyle *cs =
        dynamic_cast<TCleanupStyle *>(cleanupPalette->getStyle(styleId));
    if (!cs) continue;
    TPixel32 color = cs->getColorParamValue(colorParamIndex);

    while (palette->getStyleCount() < styleId) palette->addStyle(TPixel32::Red);

    if (styleId == palette->getStyleCount())
      palette->addStyle(color);
    else
      palette->setStyle(styleId, color);

    if (styleId > 1)  // 0 e 1 sono gia' nella pagina
      palette->getPage(0)->addStyle(styleId);

    assert(0 <= styleId && styleId < palette->getStyleCount());
    if (cs->getFlags()) palette->getStyle(styleId)->setFlags(cs->getFlags());
  }

  return palette;
}

//====================================================================================

void TargetColors::update(TPalette *palette, bool noAntialias) {
  m_colors.clear();

  TargetColor transparent(TPixel32(255, 255, 255, 0) /*TPixel32::Transparent*/,
                         0,  // BackgroundStyle,
                         0, 0, 0, 0);

  m_colors.push_back(transparent);

  for (int i = 0; i < palette->getPage(0)->getStyleCount(); i++) {
    int styleId     = palette->getPage(0)->getStyleId(i);
    TColorStyle *cs = palette->getStyle(styleId);
    if (!cs) continue;
    if (TBlackCleanupStyle *blackStyle =
            dynamic_cast<TBlackCleanupStyle *>(cs)) {
      TargetColor tc(
          blackStyle->getMainColor(), styleId, (int)blackStyle->getBrightness(),
          noAntialias ? 100 : (int)blackStyle->getContrast(),
          blackStyle->getColorThreshold(), blackStyle->getWhiteThreshold());
      m_colors.push_back(tc);
    } else if (TColorCleanupStyle *colorStyle =
                   dynamic_cast<TColorCleanupStyle *>(cs)) {
      TargetColor tc(colorStyle->getMainColor(), styleId,
                     (int)colorStyle->getBrightness(),
                     noAntialias ? 100 : (int)colorStyle->getContrast(),
                     colorStyle->getHRange(), colorStyle->getLineWidth());
      m_colors.push_back(tc);
    }
  }
}

//-------------------------------------------------------------------

void convertToCleanupPalette(TPalette *palette) {
  if (palette->isCleanupPalette()) return;
  for (int i = 1; i < palette->getStyleCount(); i++) {
    TColorStyle *cs  = palette->getStyle(i);
    TPixel32 color   = cs->getMainColor();
    TColorStyle *ccs = 0;
    if (i == 1)
      ccs = new TBlackCleanupStyle();
    else
      ccs = new TColorCleanupStyle(color);
    palette->setStyle(i, ccs);
  }
  palette->setIsCleanupPalette(true);
}

//-------------------------------------------------------------------

void convertToLevelPalette(TPalette *palette) {
  if (!palette->isCleanupPalette()) return;
  for (int i = 1; i < palette->getStyleCount(); i++) {
    TPixel32 color = palette->getStyle(i)->getMainColor();
    palette->setStyle(i, color);
  }
  palette->setIsCleanupPalette(false);
}
