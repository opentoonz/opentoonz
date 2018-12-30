

#include "toonz/tpalettehandle.h"

#include "tundo.h"
#include "historytypes.h"

//=============================================================================
// AutopaintToggleUndo
//-----------------------------------------------------------------------------

namespace {
class AutopaintToggleUndo final : public TUndo {
  TPaletteHandle *m_paletteHandle;
  TPaletteP m_palette;
  int m_styleId;
  bool m_flag;

public:
  AutopaintToggleUndo(TPaletteHandle *paletteHandle, int styleId)
      : m_paletteHandle(paletteHandle)
      , m_palette(paletteHandle->getPalette())
      , m_styleId(styleId) {}

  void toggleAutopaint() const {
    TColorStyle *s = m_palette->getStyle(m_styleId);
    s->setFlags(s->getFlags() == 0 ? 1 : 0);
    m_paletteHandle->notifyColorStyleChanged();
  }

  void undo() const override { toggleAutopaint(); }

  void redo() const override { toggleAutopaint(); }

  void onAdd() { redo(); }

  int getSize() const override { return sizeof(*this); }

  QString getHistoryString() override {
    return QObject::tr("Toggle Autopaint Option  Palette : %1  Style#%2")
        .arg(QString::fromStdWString(m_palette->getPaletteName()))
        .arg(QString::number(m_styleId));
  }

  int getHistoryType() override { return HistoryType::Palette; }
};

}  // namespace

//=============================================================================
// TPaletteHandle
//-----------------------------------------------------------------------------

TPaletteHandle::TPaletteHandle()
    : m_palette(0), m_styleIndex(-1), m_styleParamIndex(-1) {
  connectBroadcasts(this);
}

//-----------------------------------------------------------------------------

TPaletteHandle::~TPaletteHandle() {}

//-----------------------------------------------------------------------------

TPalette *TPaletteHandle::getPalette() const { return m_palette; }

//-----------------------------------------------------------------------------

int TPaletteHandle::getStyleIndex() const { return m_styleIndex; }

//-----------------------------------------------------------------------------

int TPaletteHandle::getStyleParamIndex() const { return m_styleParamIndex; }

//-----------------------------------------------------------------------------

TColorStyle *TPaletteHandle::getStyle() const {
  if (!m_palette || m_styleIndex == -1) return 0;
  return m_palette->getStyle(m_styleIndex);
}

//-----------------------------------------------------------------------------

bool TPaletteHandle::connectBroadcasts(const QObject *receiver) {
  bool ret = true;

  ret = connect(this, SIGNAL(broadcastPaletteChanged()), receiver,
                SIGNAL(paletteChanged())) &&
        ret;
  ret = connect(this, SIGNAL(broadcastPaletteTitleChanged()), receiver,
                SIGNAL(paletteTitleChanged())) &&
        ret;
  ret = connect(this, SIGNAL(broadcastColorStyleSwitched()), receiver,
                SIGNAL(colorStyleSwitched())) &&
        ret;
  ret = connect(this, SIGNAL(broadcastColorStyleChanged(bool)), receiver,
                SIGNAL(colorStyleChanged(bool))) &&
        ret;
  ret = connect(this, SIGNAL(broadcastColorStyleChangedOnMouseRelease()),
                receiver, SIGNAL(colorStyleChangedOnMouseRelease())) &&
        ret;

  return ret;
}

//-----------------------------------------------------------------------------

bool TPaletteHandle::disconnectBroadcasts(const QObject *receiver) {
  bool ret = true;

  ret = disconnect(this, SIGNAL(broadcastPaletteChanged()), receiver,
                   SIGNAL(paletteChanged())) &&
        ret;
  ret = disconnect(this, SIGNAL(broadcastPaletteTitleChanged()), receiver,
                   SIGNAL(paletteTitleChanged())) &&
        ret;
  ret = disconnect(this, SIGNAL(broadcastColorStyleSwitched()), receiver,
                   SIGNAL(colorStyleSwitched())) &&
        ret;
  ret = disconnect(this, SIGNAL(broadcastColorStyleChanged(bool)), receiver,
                   SIGNAL(colorStyleChanged(bool))) &&
        ret;
  ret = disconnect(this, SIGNAL(broadcastColorStyleChangedOnMouseRelease()),
                   receiver, SIGNAL(colorStyleChangedOnMouseRelease())) &&
        ret;

  return ret;
}

//-----------------------------------------------------------------------------

void TPaletteHandle::setPalette(TPalette *palette, int styleIndex) {
  if (m_palette == palette)
    setStyleIndex(styleIndex);
  else {
    m_palette         = palette;
    m_styleIndex      = styleIndex;
    m_styleParamIndex = 0;

    emit paletteSwitched();
  }
}

//-----------------------------------------------------------------------------

void TPaletteHandle::setStyleIndex(int index) {
  if (m_styleIndex != index || m_styleParamIndex != 0) {
    m_styleIndex      = index;
    m_styleParamIndex = 0;
    emit broadcastColorStyleSwitched();
  }
}

//-----------------------------------------------------------------------------

void TPaletteHandle::setStyleParamIndex(int index) {
  if (m_styleParamIndex != index) {
    m_styleParamIndex = index;
    emit broadcastColorStyleSwitched();
  }
}

//-----------------------------------------------------------------------------

void TPaletteHandle::notifyPaletteChanged() { emit broadcastPaletteChanged(); }

//-----------------------------------------------------------------------------

void TPaletteHandle::notifyPaletteTitleChanged() {
  emit broadcastPaletteTitleChanged();
}

//-----------------------------------------------------------------------------

void TPaletteHandle::notifyColorStyleSwitched() {
  emit broadcastColorStyleSwitched();
}

//-----------------------------------------------------------------------------

void TPaletteHandle::notifyColorStyleChanged(bool onDragging,
                                             bool setDirtyFlag) {
  if (setDirtyFlag && getPalette() && !getPalette()->getDirtyFlag())
    getPalette()->setDirtyFlag(true);

  emit broadcastColorStyleChanged(onDragging);

  if (!onDragging) emit broadcastColorStyleChangedOnMouseRelease();
}

//-----------------------------------------------------------------------------

void TPaletteHandle::toggleAutopaint() {
  int index = getStyleIndex();
  if (index > 0) {
    TUndoManager::manager()->add(new AutopaintToggleUndo(this, index));
  }
}