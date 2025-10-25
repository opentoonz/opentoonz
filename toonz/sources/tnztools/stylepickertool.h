#pragma once

#ifndef STYLEPICKERTOOL_H
#define STYLEPICKERTOOL_H

#include "tools/tool.h"
#include "tproperty.h"

#include "toonz/toonzimageutils.h"
#include "toonz/ttilesaver.h"
#include "toonz/ttileset.h"

#include "toonz/tpalettehandle.h"
#include "toonz/txshlevelhandle.h"
#include "tstroke.h"
#include "toonz/txshsimplelevel.h"

#include "toonzqt/icongenerator.h"

class StylePickerTool final : public TTool, public QObject {
  Q_DECLARE_TR_FUNCTIONS(StylePickerTool)

  int m_oldStyleId, m_currentStyleId;

  TEnumProperty m_colorType;
  TPropertyGroup m_prop;
  TBoolProperty m_replaceStyle;
  TBoolProperty m_passivePick;

  TBoolProperty m_organizePalette;

  TPalette *getPal();
  bool startOrganizePalette();

public:
  TPropertyGroup *getProperties(int targetType) override { return &m_prop; }

  StylePickerTool();

  void onActivate() override;
  void onDeactivate() override;

  ToolType getToolType() const override { return TTool::LevelReadTool; }

  void draw() override {}

  void leftButtonDown(const TPointD &pos, const TMouseEvent &e) override;

  void leftButtonDrag(const TPointD &pos, const TMouseEvent &e) override;

  void pick(const TPointD &pos, const TMouseEvent &e, bool isDragging = true);

  void mouseMove(const TPointD &pos, const TMouseEvent &e) override;

  int getCursorId() const override;

  bool onPropertyChanged(std::string propertyName) override;

  bool isOrganizePaletteActive() { return m_organizePalette.getValue(); }
  bool isReplaceStyleActive() { return m_replaceStyle.getValue(); }

  void updateTranslation() override;

public slots:
  /*
  This function is called when working paltte is switched
  If the working palette is changed, then deactivate the "organize palette"
  toggle.
*/
  void onPaletteSwitched();
};

namespace {

void replaceStyle(const TVectorImageP &vi, int styleId, int oldStyleId,
                  std::vector<IntPair> *modifiedStrokes,
                  std::vector<IntPair> *modifiedRegions) {
  std::vector<int> indexs;

  for (int i = 0; i < vi->getStrokeCount(); ++i) {
    TStroke *stroke = vi->getStroke(i);
    if (stroke->getStyle() == oldStyleId) {
      if (modifiedStrokes) modifiedStrokes->emplace_back(i, oldStyleId);
      stroke->setStyle(styleId);
      indexs.push_back(i);
    }
  }
  // vi->notifyChangedStrokes(indexs, {});
  for (int i = 0; i < vi->getRegionCount(); ++i) {
    TRegion *region = vi->getRegion(i);
    if (region->getStyle() == oldStyleId) {
      if (modifiedRegions) modifiedRegions->emplace_back(i, oldStyleId);
      region->setStyle(styleId);
    }
  }
  // vi->validateRegions();
}

void replaceStyle(const TRasterCM32P &r, int styleId, int oldStyleId,
                  TTileSaverCM32 *saver) {
  for (int y = 0; y < r->getLy(); y++) {
    TPixelCM32 *pix = r->pixels(y), *endPix = pix + r->getLx();

    if (saver) {
      std::vector<std::pair<int, int>> segments;
      int startX = -1;

      TPixelCM32 *scanPix = pix;
      for (int x = 0; scanPix < endPix; x++, scanPix++) {
        bool needsChange = (scanPix->getInk() == oldStyleId ||
                            scanPix->getPaint() == oldStyleId);

        if (needsChange) {
          if (startX == -1) startX = x;
        } else {
          if (startX != -1) {
            segments.emplace_back(startX, x - 1);
            startX = -1;
          }
        }
      }

      if (startX != -1) {
        segments.emplace_back(startX, r->getLx() - 1);
      }

      for (const auto &seg : segments) {
        saver->save(TRect(seg.first, y, seg.second, y));
      }

      for (const auto &seg : segments) {
        for (int x = seg.first; x <= seg.second; x++) {
          TPixelCM32 *p = pix + x;
          if (p->getInk() == oldStyleId) p->setInk(styleId);
          if (p->getPaint() == oldStyleId) p->setPaint(styleId);
        }
      }
    } else {
      for (int x = 0; pix < endPix; x++, pix++) {
        if (pix->getInk() == oldStyleId) pix->setInk(styleId);
        if (pix->getPaint() == oldStyleId) pix->setPaint(styleId);
      }
    }
  }
}

class ReplaceStyleUndo final : public TUndo {
  TXshLevelHandle *m_levelHandle;
  TPaletteHandle *m_paletteHandler;
  TPalette *m_palette;
  TXshSimpleLevel *m_level;
  int m_styleId, m_oldStyleId;
  int m_levelType;
  std::map<TFrameId, TTileSetCM32 *> m_tileSets;
  std::map<TFrameId, std::pair<std::vector<IntPair>, std::vector<IntPair>>>
      m_modifiedStrokeRegions;

  void notifyLevelChanged() const {
    if (m_level) {
      m_level->getProperties()->setDirtyFlag(true);
      m_levelHandle->notifyLevelChange();
      std::vector<TFrameId> fids;
      m_level->getFids(fids);
      for (TFrameId fid : fids)
        IconGenerator::instance()->invalidate(m_level, fid);
    }
  };

public:
  ReplaceStyleUndo(TXshLevelHandle *levelHandler, TPaletteHandle *paletteHandle,
                   int styleId, int oldStyleId)
      : m_levelHandle(levelHandler)
      , m_paletteHandler(paletteHandle)
      , m_palette(paletteHandle->getPalette())
      , m_level(levelHandler->getSimpleLevel())
      , m_styleId(styleId)
      , m_oldStyleId(oldStyleId) {
    if (!m_level || !m_palette) return;
    std::vector<TFrameId> fids;
    m_level->getFids(fids);
    m_levelType = m_level->getType() == TXshLevelType::TZP_XSHLEVEL   ? 1
                  : m_level->getType() == TXshLevelType::PLI_XSHLEVEL ? 2
                                                                      : 0;
    if (!m_levelType) return;
    if (m_levelType == 1)
      for (const TFrameId &fid : fids) {
        TToonzImageP ti = m_level->getFrame(fid, true);
        if (!ti) continue;

        TRasterCM32P ras = ti->getRaster();
        if (!ras) continue;

        TTileSetCM32 *tileSet = new TTileSetCM32(ras->getSize());

        TTileSaverCM32 saver(ras, tileSet);

        replaceStyle(ras, m_styleId, m_oldStyleId, &saver);
        if (saver.getTileSet()->getTileCount() == 0) {
          delete tileSet;
          continue;
        }
        m_tileSets[fid] = tileSet;
      }
    else
      for (const TFrameId &fid : fids) {
        TVectorImageP vi = m_level->getFrame(fid, true);
        if (!vi) continue;
        std::vector<IntPair> modifiedStrokes;
        std::vector<IntPair> modifiedRegions;

        replaceStyle(vi, m_styleId, m_oldStyleId, &modifiedStrokes,
                     &modifiedRegions);
        if (modifiedStrokes.empty() && modifiedRegions.empty())
          continue;
        else
          m_modifiedStrokeRegions[fid] = std::make_pair(
              std::move(modifiedStrokes), std::move(modifiedRegions));
      }
    notifyLevelChanged();
    paletteHandle->setStyleIndex(styleId);
  }
  ~ReplaceStyleUndo() {
    for (auto &pair : m_tileSets) {
      delete pair.second;
    }
    m_tileSets.clear();
  };

  void redo() const override {
    if (!m_level) return;

    if (m_levelType == 1)
      for (auto const [fid, tileSet] : m_tileSets) {
        auto it = m_tileSets.find(fid);
        if (it == m_tileSets.end()) continue;

        TToonzImageP img = m_level->getFrame(fid, true);
        if (!img) continue;

        TRasterCM32P ras = img->getRaster();
        if (!ras) continue;

        replaceStyle(ras, m_styleId, m_oldStyleId, nullptr);
      }
    else
      for (auto const [fid, pair] : m_modifiedStrokeRegions) {
        TVectorImageP vi = m_level->getFrame(fid, true);
        if (!vi) continue;
        replaceStyle(vi, m_styleId, m_oldStyleId, nullptr, nullptr);
      }

    m_levelHandle->setLevel(m_level);
    notifyLevelChanged();
    if (m_palette && m_paletteHandler) m_paletteHandler->setPalette(m_palette);
    if (m_paletteHandler) m_paletteHandler->setStyleIndex(m_styleId);
  };

  void undo() const override {
    if (!m_level) return;

    if (m_levelType == 1)
      for (auto const [fid, tileSet] : m_tileSets) {
        TToonzImageP img = m_level->getFrame(fid, true);
        if (img && tileSet) {
          ToonzImageUtils::paste(img, tileSet);
          m_level->setFrame(fid, img);
        }
      }
    else
      for (auto const [fid, pair] : m_modifiedStrokeRegions) {
        TVectorImageP vi = m_level->getFrame(fid, true);
        if (!vi) continue;
        std::vector<int> indexs;
        for (auto const [index, id] : pair.first) {
          TStroke *stroke = vi->getStroke(index);
          if (stroke) {
            stroke->setStyle(id);
            indexs.push_back(index);
          }
        }
        // vi->notifyChangedStrokes(indexs, {});
        for (auto const [index, id] : pair.second) {
          TRegion *region = vi->getRegion(index);
          if (region) {
            region->setStyle(id);
          }
        }
        // vi->validateRegions();
      }
    m_levelHandle->setLevel(m_level);
    notifyLevelChanged();
    if (m_palette && m_paletteHandler) m_paletteHandler->setPalette(m_palette);
    if (m_paletteHandler) m_paletteHandler->setStyleIndex(m_oldStyleId);
  };

  int getSize() const override {
    int size = sizeof(*this);
    for (const auto &pair : m_tileSets) {
      if (pair.second) {
        size += pair.second->getMemorySize();
      }
    }
    return size;
  };

  QString getHistoryString() override {
    return QString("Replace Level %1 Style: #%2 -> #%3")
        .arg(m_level ? QString::fromStdWString(m_level->getName()) : QString())
        .arg(m_oldStyleId)
        .arg(m_styleId);
  };
};

}  // namespace

/* called in ColorModelViewer::pick() - replace current style with selected
 * style */
static void replaceLevelStyle(TXshLevelHandle *levelHandle,
                              TPaletteHandle *paletteHandle, int styleId,
                              int oldStyleId) {
  TXshSimpleLevel *level = levelHandle->getSimpleLevel();
  TPalette *palette      = paletteHandle->getPalette();
  if (!palette || !level) return;
  if (!palette || styleId == oldStyleId) return;
  auto style = palette->getStyle(styleId);

  ReplaceStyleUndo *undo =
      new ReplaceStyleUndo(levelHandle, paletteHandle, styleId, oldStyleId);
  // undo->redo();
  TUndoManager::manager()->add(undo);
}
#endif
