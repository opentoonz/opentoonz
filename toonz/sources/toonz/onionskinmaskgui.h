#pragma once

#ifndef ONIONSKINMASKGUI
#define ONIONSKINMASKGUI

#include <QObject>

class OnionSkinMask;
class QMenu;

//=============================================================================
namespace OnioniSkinMaskGUI {
//-----------------------------------------------------------------------------

// Da fare per la filmstrip!!
void addOnionSkinCommand(QMenu *, bool isFilmStrip = false);

void resetShiftTraceFrameOffset();

//=============================================================================
// OnionSkinSwitcher
//-----------------------------------------------------------------------------

class OnionSkinSwitcher final : public QObject {
  Q_OBJECT

public:
  OnionSkinSwitcher() {}

  OnionSkinMask getMask() const;

  void setMask(const OnionSkinMask &mask);

  bool isActive() const;
  bool isWholeScene() const;

public slots:
  void activate();
  void deactivate();
  void setWholeScene();
  void setSingleLevel();
};

//-----------------------------------------------------------------------------
}  // namespace OnioniSkinMaskGUI
//-----------------------------------------------------------------------------

#endif  // ONIONSKINMASKGUI
