

#include "tpaletteutil.h"
#include "tcolorstyles.h"

void mergePalette(const TPaletteP &targetPalette,
                  std::map<int, int> &indexTable,
                  const TPaletteP &sourcePalette,
                  const std::set<int> &sourceIndices) {
  assert(targetPalette);
  assert(targetPalette->getPageCount() > 0);

  indexTable[0]                         = 0;
  std::set<int>::const_iterator styleIt = sourceIndices.begin();
  for (; styleIt != sourceIndices.end(); ++styleIt) {
    int srcStyleId = *styleIt;
    if (srcStyleId == 0) continue;
    assert(0 <= srcStyleId && srcStyleId < sourcePalette->getStyleCount());
    TColorStyle *srcStyle = sourcePalette->getStyle(srcStyleId);
    assert(srcStyle);
    TPalette::Page *page = sourcePalette->getStylePage(srcStyleId);
    if (page) {
      std::wstring pageName = page->getName();
      int j                 = 0;
      for (j = 0; j < targetPalette->getPageCount(); j++) {
        if (targetPalette->getPage(j)->getName() != pageName) continue;
        break;
      }
      if (j < targetPalette->getPageCount())
        page = targetPalette->getPage(j);
      else
        page = targetPalette->addPage(pageName);
    } else
      page         = targetPalette->getPage(0);
    int tarStyleId = 0;
    int i          = 0;
    for (i = 0; i < targetPalette->getStyleCount(); i++)
      if (*srcStyle == *targetPalette->getStyle(i) && i == srcStyleId) break;
    if (i < targetPalette->getStyleCount())
      tarStyleId = i;
    else {
      TColorStyle *dstStyle = srcStyle->clone();
      tarStyleId            = targetPalette->addStyle(dstStyle);
      page->addStyle(tarStyleId);
      targetPalette->setDirtyFlag(true);
    }
    assert(indexTable.count(srcStyleId) == 0);
    indexTable[srcStyleId] = tarStyleId;
  }
}

//------------------------------------------------------------------------
// replace palette and lacking amount of styles will be copied from the other
// one
// return value will be true if the style amount is changed after the operation

bool mergePalette_Overlap(const TPaletteP &dstPalette,
                          const TPaletteP &copiedPalette,
                          bool keepOriginalPalette) {
  if (!dstPalette || !copiedPalette) return false;

  bool styleAdded = false;

  int dstStyleCount    = dstPalette->getStyleCount();
  int copiedStyleCount = copiedPalette->getStyleCount();

  // keep original
  if (keepOriginalPalette) {
    // do nothing if the style amount of the dst is equal or larger than the
    // copied
    if (dstStyleCount >= copiedStyleCount) {
      return false;
    }
    // if the style amount of the dst is less than the copied
    else {
      // for lacking amount of styles
      for (int i = dstStyleCount; i < copiedStyleCount; i++) {
        // get the page index of the copied style
        TPalette::Page *tmpPage = copiedPalette->getStylePage(i);
        // clone copied style
        TColorStyle *tmpStyle = copiedPalette->getStyle(i)->clone();
        // add it to the dst
        int id = dstPalette->addStyle(tmpStyle);
        // add to the page if it is not deleted in the copied
        if (tmpPage) dstPalette->getPage(0)->addStyle(id);
      }

      styleAdded = true;
    }
  }

  // replace
  else {
    // if the style amount of the dst is larger than the copied
    if (dstStyleCount > copiedStyleCount) {
      TPalette *tmpPalette = copiedPalette->clone();

      // for lacking amount of styles
      for (int i = copiedStyleCount; i < dstStyleCount; i++) {
        TColorStyle *tmpStyle = dstPalette->getStyle(i)->clone();
        int id                = tmpPalette->addStyle(tmpStyle);

        TPalette::Page *tmpPage = dstPalette->getStylePage(i);
        if (!tmpPage) continue;
        std::wstring pageName = tmpPage->getName();
        // create new page with the same name if needed
        int p;
        for (p = 0; p < tmpPalette->getPageCount(); p++) {
          if (tmpPalette->getPage(p)->getName() == pageName) break;
        }
        if (p != tmpPalette->getPageCount())
          tmpPalette->getPage(p)->addStyle(id);
        else {
          tmpPalette->addPage(pageName)->addStyle(id);
        }
      }
      dstPalette->assign(tmpPalette);

      styleAdded = false;
    }
    // if the style amount of the dst is equal or less than the copied
    else {
      dstPalette->assign(copiedPalette.getPointer());

      styleAdded = (dstStyleCount < copiedStyleCount);
    }
  }
  dstPalette->setDirtyFlag(true);
  return styleAdded;
}
