#pragma once

#ifndef KEYFRAMENAVIGATOR_INCLUDED
#define KEYFRAMENAVIGATOR_INCLUDED

#include "tpalette.h"
#include "tfxutil.h"

#include "toonz/tstageobject.h"
#include "toonz/tframehandle.h"
#include "toonz/tobjecthandle.h"
#include "toonz/txsheethandle.h"
#include "toonz/tpalettehandle.h"
#include "toonz/tfxhandle.h"
#include "toonz/tcolumnfx.h"

#include <QToolBar>

#undef DVAPI
#undef DVVAR
#ifdef TOONZQT_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

class TStyleSelection;

//=============================================================================
// KeyframeNavigator
//-----------------------------------------------------------------------------

class DVAPI KeyframeNavigator : public QToolBar {
  Q_OBJECT

  QAction *m_actPreviewKey;
  QAction *m_actKeyNo;
  QAction *m_actKeyPartial;
  QAction *m_actKeyTotal;
  QAction *m_actNextKey;

  TFrameHandle *m_frameHandle;

  QWidget *m_panel;

public:
  KeyframeNavigator(QWidget *parent = 0, TFrameHandle *frameHandle = 0);

  int getCurrentFrame() const {
    if (!m_frameHandle) return -1;
    return m_frameHandle->getFrameIndex();
  }

public slots:
  void setCurrentFrame(int frame) {
    m_frameHandle->setFrameIndex(frame);
    update();
  }

  void setFrameHandle(TFrameHandle *frameHandle) {
    m_frameHandle = frameHandle;
    update();
  }
  void update();

  void onNextKeyframe(QWidget *panel);
  void onPrevKeyframe(QWidget *panel);

protected slots:
  void toggleKeyAct() {
    toggle();
    update();
  }
  void toggleNextKeyAct() {
    goNext();
    update();
  }
  void togglePrevKeyAct() {
    goPrev();
    update();
  }

protected:
  virtual bool hasNext() const        = 0;
  virtual bool hasPrev() const        = 0;
  virtual bool hasKeyframes() const   = 0;
  virtual bool isKeyframe() const     = 0;
  virtual bool isFullKeyframe() const = 0;
  virtual void toggle()               = 0;
  virtual void goNext()               = 0;
  virtual void goPrev()               = 0;

  void showEvent(QShowEvent *) override;
  void hideEvent(QHideEvent *) override;
};

//=============================================================================
// ViewerKeyframeNavigator
//-----------------------------------------------------------------------------

class DVAPI ViewerKeyframeNavigator final : public KeyframeNavigator {
  Q_OBJECT

  TObjectHandle *m_objectHandle;
  TXsheetHandle *m_xsheetHandle;

public:
  ViewerKeyframeNavigator(QWidget *parent, TFrameHandle *frameHandle = 0)
      : KeyframeNavigator(parent, frameHandle)
      , m_objectHandle(0)
      , m_xsheetHandle(0) {
    update();
  }

  TObjectHandle *getObjectHandle() const { return m_objectHandle; }
  TXsheetHandle *getXsheetHandle() const { return m_xsheetHandle; }

public slots:
  void setObjectHandle(TObjectHandle *objectHandle) {
    m_objectHandle = objectHandle;
    update();
  }
  void setXsheetHandle(TXsheetHandle *xsheetHandle) {
    m_xsheetHandle = xsheetHandle;
    update();
  }

protected:
  TStageObject *getStageObject() const;
  bool hasNext() const override;
  bool hasPrev() const override;
  bool hasKeyframes() const override;
  bool isKeyframe() const override;
  bool isFullKeyframe() const override;
  void toggle() override;
  void goNext() override;
  void goPrev() override;

  void showEvent(QShowEvent *) override;
  void hideEvent(QHideEvent *) override;
};

//=============================================================================
// PaletteKeyframeNavigator
//-----------------------------------------------------------------------------

class DVAPI PaletteKeyframeNavigator final : public KeyframeNavigator {
  Q_OBJECT

  TPaletteHandle *m_paletteHandle;
  TStyleSelection *m_selection;

public:
  PaletteKeyframeNavigator(QWidget *parent, TFrameHandle *frameHandle = 0)
      : KeyframeNavigator(parent, frameHandle)
      , m_paletteHandle(0)
      , m_selection(0) {
    update();
  }

  void setSelection(TStyleSelection *selection) { m_selection = selection; }

  TPalette *getPalette() const {
    if (!m_paletteHandle) return 0;
    return m_paletteHandle->getPalette();
  }
  int getStyleIndex() const {
    if (!m_paletteHandle) return 0;
    return m_paletteHandle->getStyleIndex();
  }

  TPaletteHandle *getPaletteHandle() const { return m_paletteHandle; }

public slots:
  void setPaletteHandle(TPaletteHandle *paletteHandle) {
    m_paletteHandle = paletteHandle;
    update();
  }

protected:
  bool hasNext() const override;
  bool hasPrev() const override;
  bool hasKeyframes() const override;
  bool isKeyframe() const override;
  bool isFullKeyframe() const override { return isKeyframe(); }
  void toggle() override;
  void goNext() override;
  void goPrev() override;

  void showEvent(QShowEvent *) override;
  void hideEvent(QHideEvent *) override;
};

//=============================================================================
// FxKeyframeNavigator
//-----------------------------------------------------------------------------

class DVAPI FxKeyframeNavigator final : public KeyframeNavigator {
  Q_OBJECT

  TFxHandle *m_fxHandle;

public:
  FxKeyframeNavigator(QWidget *parent, TFrameHandle *frameHandle = 0)
      : KeyframeNavigator(parent, frameHandle), m_fxHandle(0) {
    update();
  }

  TFx *getFx() const {
    if (!m_fxHandle) return 0;
    TFx *fx = m_fxHandle->getFx();
    if (TZeraryColumnFx *zfx = dynamic_cast<TZeraryColumnFx *>(fx))
      fx = zfx->getZeraryFx();
    return fx;
  }

  TFxHandle *getFxHandle() const { return m_fxHandle; }

public slots:
  void setFxHandle(TFxHandle *fxHandle) {
    m_fxHandle = fxHandle;
    update();
  }

protected:
  bool hasNext() const override;
  bool hasPrev() const override;
  bool hasKeyframes() const override;
  bool isKeyframe() const override;
  bool isFullKeyframe() const override;
  void toggle() override;
  void goNext() override;
  void goPrev() override;

  void showEvent(QShowEvent *) override;
  void hideEvent(QHideEvent *) override;
};

#endif
