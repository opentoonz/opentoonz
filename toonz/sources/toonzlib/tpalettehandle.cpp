

#include "toonz/tpalettehandle.h"

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
  ret = connect(this, SIGNAL(broadcastColorStyleChanged()), receiver,
                SIGNAL(colorStyleChanged())) &&
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
  ret = disconnect(this, SIGNAL(broadcastColorStyleChanged()), receiver,
                   SIGNAL(colorStyleChanged())) &&
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
  //	if(m_styleIndex != index)
  //	{
  m_styleIndex      = index;
  m_styleParamIndex = 0;
  emit broadcastColorStyleSwitched();
  //	}
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

  emit broadcastColorStyleChanged();

  if (!onDragging) emit broadcastColorStyleChangedOnMouseRelease();
}
