#include "toonzqt/brushpresetbridge.h"
#include "tcolorstyles.h"
#include "tpalette.h"
#include "toonz/tpalettehandle.h"
#include "tundo.h"
#include "historytypes.h"
#include <QMutex>
#include <QMutexLocker>

namespace BrushPresetBridge {
static QString s_pendingPresetName;
static QStringList s_pendingRemoveNames;
static bool s_nonDestructiveMode  = false;
static bool s_selectivePresetMode = false;
static QMutex s_mutex;

void setPendingPresetName(const QString &name) {
  QMutexLocker lock(&s_mutex);
  s_pendingPresetName = name;
}

QString takePendingPresetName() {
  QMutexLocker lock(&s_mutex);
  QString name = s_pendingPresetName;
  s_pendingPresetName.clear();
  return name;
}

void addPendingRemoveName(const QString &name) {
  QMutexLocker lock(&s_mutex);
  s_pendingRemoveNames.append(name);
}

QStringList takePendingRemoveNames() {
  QMutexLocker lock(&s_mutex);
  QStringList names = s_pendingRemoveNames;
  s_pendingRemoveNames.clear();
  return names;
}

void setNonDestructiveMode(bool enabled) { s_nonDestructiveMode = enabled; }
bool isNonDestructiveMode() { return s_nonDestructiveMode; }

void setSelectivePresetMode(bool enabled) { s_selectivePresetMode = enabled; }
bool isSelectivePresetMode() { return s_selectivePresetMode; }

//-----------------------------------------------------------------------------
// Visual style comparison (ignores name, globalName, flags, pickedPosition)
//-----------------------------------------------------------------------------

bool stylesMatchVisually(const TColorStyle *a, const TColorStyle *b) {
  if (!a || !b) return false;
  if (a->getTagId() != b->getTagId()) return false;

  // Intentionally skip mainColor and colorParamValues:
  // colors are user-controlled drawing data, not part of the brush identity.
  // Matching is based purely on structural/technical parameters.

  int paramCount = a->getParamCount();
  if (paramCount != b->getParamCount()) return false;

  for (int p = 0; p < paramCount; ++p) {
    TColorStyle::ParamType pt = a->getParamType(p);
    if (pt != b->getParamType(p)) return false;
    switch (pt) {
    case TColorStyle::BOOL:
      if (a->getParamValue(TColorStyle::bool_tag(), p) !=
          b->getParamValue(TColorStyle::bool_tag(), p))
        return false;
      break;
    case TColorStyle::INT:
    case TColorStyle::ENUM:
      if (a->getParamValue(TColorStyle::int_tag(), p) !=
          b->getParamValue(TColorStyle::int_tag(), p))
        return false;
      break;
    case TColorStyle::DOUBLE:
      if (a->getParamValue(TColorStyle::double_tag(), p) !=
          b->getParamValue(TColorStyle::double_tag(), p))
        return false;
      break;
    case TColorStyle::FILEPATH:
      if (a->getParamValue(TColorStyle::TFilePath_tag(), p) !=
          b->getParamValue(TColorStyle::TFilePath_tag(), p))
        return false;
      break;
    default:
      break;
    }
  }
  return true;
}

int findMatchingStyleInPalette(TPalette *palette,
                               const TColorStyle *targetStyle) {
  if (!palette || !targetStyle) return -1;
  for (int i = 1; i < palette->getStyleCount(); ++i) {
    TColorStyle *existing = palette->getStyle(i);
    if (stylesMatchVisually(existing, targetStyle)) return i;
  }
  return -1;
}
//-----------------------------------------------------------------------------
// Undo for style overwrite (destructive preset application or Plain Color)
//-----------------------------------------------------------------------------

namespace {
class PresetStyleOverwriteUndo final : public TUndo {
  TPaletteP m_palette;
  int m_styleId;
  TColorStyleP m_oldStyle, m_newStyle;
  TPaletteHandle *m_ph;

public:
  PresetStyleOverwriteUndo(TPalette *pal, int id, const TColorStyle *oldS,
                           const TColorStyle *newS, TPaletteHandle *ph)
      : m_palette(pal)
      , m_styleId(id)
      , m_oldStyle(oldS->clone())
      , m_newStyle(newS->clone())
      , m_ph(ph) {}
  void undo() const override {
    m_palette->setStyle(m_styleId, m_oldStyle->clone());
    if (m_ph) m_ph->notifyColorStyleChanged(false, false);
  }
  void redo() const override {
    m_palette->setStyle(m_styleId, m_newStyle->clone());
    if (m_ph) m_ph->notifyColorStyleChanged(false, false);
  }
  int getSize() const override { return sizeof(*this); }
  QString getHistoryString() override {
    return QObject::tr("Apply Brush Preset");
  }
  int getHistoryType() override { return HistoryType::Palette; }
};
}  // namespace

TUndo *createStyleOverwriteUndo(TPalette *palette, int styleId,
                                const TColorStyle *oldStyle,
                                const TColorStyle *newStyle,
                                TPaletteHandle *paletteHandle) {
  return new PresetStyleOverwriteUndo(palette, styleId, oldStyle, newStyle,
                                      paletteHandle);
}

}  // namespace BrushPresetBridge
