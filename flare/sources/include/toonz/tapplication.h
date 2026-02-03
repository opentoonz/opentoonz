#pragma once

#ifndef TAPPLICATION_H
#define TAPPLICATION_H

//====================================================

//    Forward declarations

class TFrameHandle;
class TXshLevelHandle;
class TXsheetHandle;
class TObjectHandle;
class TColumnHandle;
class TSceneHandle;
class ToolHandle;
class TSelectionHandle;
class TOnionSkinMaskHandle;
class TPaletteHandle;
class TFxHandle;
class PaletteController;
class TColorStyle;

//====================================================

/*!
  TApplication defines the base interface for a handle class
  (typically implemented as a singleton) to global Toonz handles.
*/

class TApplication {
public:
  TApplication() {}
  virtual ~TApplication() {}

  virtual TFrameHandle *getCurrentFrame() const             = 0;
  virtual TXshLevelHandle *getCurrentLevel() const          = 0;
  virtual TXsheetHandle *getCurrentXsheet() const           = 0;
  virtual TObjectHandle *getCurrentObject() const           = 0;
  virtual TColumnHandle *getCurrentColumn() const           = 0;
  virtual TSceneHandle *getCurrentScene() const             = 0;
  virtual ToolHandle *getCurrentTool() const                = 0;
  virtual TSelectionHandle *getCurrentSelection() const     = 0;
  virtual TOnionSkinMaskHandle *getCurrentOnionSkin() const = 0;
  virtual TPaletteHandle *getCurrentPalette() const         = 0;
  virtual TFxHandle *getCurrentFx() const                   = 0;
  virtual PaletteController *getPaletteController() const   = 0;

  // Current Palette (PaletteController) methods
  virtual TColorStyle *getCurrentLevelStyle() const = 0;
  virtual int getCurrentLevelStyleIndex() const     = 0;
  virtual void setCurrentLevelStyleIndex(int index,
                                         bool forceUpdate = false) = 0;
};

#endif  // TAPPLICATION_H
