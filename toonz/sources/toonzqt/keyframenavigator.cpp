

#include "toonzqt/keyframenavigator.h"
#include "toonzqt/styleselection.h"

#include "toonzqt/gutil.h"
#include "toonz/txsheet.h"
#include "toonz/txshcolumn.h"
#include "toonz/tstageobjectkeyframe.h"
#include "toonz/stageobjectutil.h"

#include "tpixelutils.h"
#include "tfx.h"
#include "tparamcontainer.h"
#include "tspectrumparam.h"
#include "ttonecurveparam.h"

#include <QPainter>
#include <QPixmap>
#include <QIcon>

#ifdef _WIN32
#pragma warning(disable : 4251)
#endif

#include <memory>

#include <QAction>
#include <QMouseEvent>

using namespace std;

//=============================================================================
// KeyframeNavigator
//-----------------------------------------------------------------------------

KeyframeNavigator::KeyframeNavigator(QWidget *parent, TFrameHandle *frameHandle)
    : QToolBar(parent), m_frameHandle(frameHandle) {
  setLayoutDirection(Qt::LeftToRight);
  setIconSize(QSize(15, 23));
  setObjectName("keyFrameNavigator");

  QIcon previewActIcon = createQIconPNG("prevkey");
  previewActIcon.addFile(QString(":Resources/prevkey_disabled.png"), QSize(),
                         QIcon::Disabled);
  m_actPreviewKey = new QAction(previewActIcon, tr("Previous Key"), this);
  QObject::connect(m_actPreviewKey, &QAction::triggered,  //
                   this, &KeyframeNavigator::togglePrevKeyAct);
  addAction(m_actPreviewKey);

  m_actKeyNo = new QAction(createQIconPNG("key_no"), tr("Set Key"), this);
  QObject::connect(m_actKeyNo, &QAction::triggered,  //
                   this, &KeyframeNavigator::toggleKeyAct);
  addAction(m_actKeyNo);

  m_actKeyPartial =
      new QAction(createQIconPNG("key_partial"), tr("Set Key"), this);
  QObject::connect(m_actKeyPartial, &QAction::triggered,  //
                   this, &KeyframeNavigator::toggleKeyAct);
  addAction(m_actKeyPartial);

  m_actKeyTotal = new QAction(createQIconPNG("key_total"), tr("Set Key"), this);
  QObject::connect(m_actKeyTotal, &QAction::triggered,  //
                   this, &KeyframeNavigator::toggleKeyAct);
  addAction(m_actKeyTotal);

  QIcon nextActIcon = createQIconPNG("nextkey");
  nextActIcon.addFile(QString(":Resources/nextkey_disabled.png"), QSize(),
                      QIcon::Disabled);
  m_actNextKey = new QAction(nextActIcon, tr("Next Key"), this);
  QObject::connect(m_actNextKey, &QAction::triggered,  //
                   this, &KeyframeNavigator::toggleNextKeyAct);
  addAction(m_actNextKey);
}

//-----------------------------------------------------------------------------

void KeyframeNavigator::update() {
  // Prev button
  if (hasPrev())
    m_actPreviewKey->setDisabled(false);
  else
    m_actPreviewKey->setDisabled(true);

  bool isFullKey = isFullKeyframe();
  bool isKey     = isKeyframe();

  if (isKey && !isFullKey) {
    m_actKeyNo->setVisible(false);
    m_actKeyTotal->setVisible(false);
    m_actKeyPartial->setVisible(true);
    m_actKeyPartial->setDisabled(false);
  }
  if (isFullKey) {
    m_actKeyNo->setVisible(false);
    m_actKeyPartial->setVisible(false);
    m_actKeyTotal->setVisible(true);
    m_actKeyTotal->setDisabled(false);
  }
  if (!isKey && !isFullKey) {
    m_actKeyPartial->setVisible(false);
    m_actKeyTotal->setVisible(false);
    m_actKeyNo->setVisible(true);
    m_actKeyNo->setDisabled(false);
  }

  // Next button
  if (hasNext())
    m_actNextKey->setDisabled(false);
  else
    m_actNextKey->setDisabled(true);
}

//-----------------------------------------------------------------------------

void KeyframeNavigator::showEvent(QShowEvent *e) {
  update();
  if (!m_frameHandle) return;
  QObject::connect(m_frameHandle, &TFrameHandle::frameSwitched,  //
                   this, &KeyframeNavigator::update);
}

//-----------------------------------------------------------------------------

void KeyframeNavigator::hideEvent(QHideEvent *e) {
  if (!m_frameHandle) return;
  QObject::disconnect(m_frameHandle);
}

//=============================================================================
// ViewerKeyframeNavigator
//-----------------------------------------------------------------------------

TStageObject *ViewerKeyframeNavigator::getStageObject() const {
  if (!m_xsheetHandle || !m_objectHandle) return 0;

  TStageObjectId objectId = m_objectHandle->getObjectId();
  TXsheet *xsh            = m_xsheetHandle->getXsheet();
  // Se e' una colonna sound non posso settare chiavi
  if (objectId.isColumn()) {
    TXshColumn *column = xsh->getColumn(objectId.getIndex());
    if (column && column->getSoundColumn()) return 0;
  }
  return xsh->getStageObject(objectId);
}

//-----------------------------------------------------------------------------

bool ViewerKeyframeNavigator::hasNext() const {
  TStageObject *pegbar = getStageObject();
  if (!pegbar) return false;
  int r0, r1;
  pegbar->getKeyframeRange(r0, r1);
  return r0 <= r1 && getCurrentFrame() < r1;
}

//-------------------------------------------------------------------

bool ViewerKeyframeNavigator::hasPrev() const {
  TStageObject *pegbar = getStageObject();
  if (!pegbar) return false;
  int r0, r1;
  pegbar->getKeyframeRange(r0, r1);
  return r0 <= r1 && getCurrentFrame() > r0;
}

//-------------------------------------------------------------------

bool ViewerKeyframeNavigator::hasKeyframes() const {
  TStageObject *pegbar = getStageObject();
  if (!pegbar) return false;
  int r0, r1;
  pegbar->getKeyframeRange(r0, r1);
  return r0 <= r1;
}

//-------------------------------------------------------------------

bool ViewerKeyframeNavigator::isKeyframe() const {
  TStageObject *pegbar = getStageObject();
  if (!pegbar) return false;
  return pegbar->isKeyframe(getCurrentFrame());
}

//-------------------------------------------------------------------

bool ViewerKeyframeNavigator::isFullKeyframe() const {
  TStageObject *pegbar = getStageObject();
  if (!pegbar) return false;
  return pegbar->isFullKeyframe(getCurrentFrame());
}

//-------------------------------------------------------------------

void ViewerKeyframeNavigator::toggle() {
  TStageObject *pegbar = getStageObject();
  if (!pegbar) return;
  int frame = getCurrentFrame();

  if (pegbar->isFullKeyframe(frame)) {
    TStageObject::Keyframe key = pegbar->getKeyframe(frame);
    pegbar->removeKeyframeWithoutUndo(frame);
    UndoRemoveKeyFrame *undo =
        new UndoRemoveKeyFrame(pegbar->getId(), frame, key, m_xsheetHandle);
    undo->setObjectHandle(m_objectHandle);
    TUndoManager::manager()->add(undo);
  } else {
    UndoSetKeyFrame *undo =
        new UndoSetKeyFrame(pegbar->getId(), frame, m_xsheetHandle);
    pegbar->setKeyframeWithoutUndo(frame);
    undo->setObjectHandle(m_objectHandle);
    TUndoManager::manager()->add(undo);
  }
  m_objectHandle->notifyObjectIdChanged(false);
}

//-------------------------------------------------------------------

void ViewerKeyframeNavigator::goNext() {
  TStageObject *pegbar = getStageObject();
  if (!pegbar) return;
  int frame = getCurrentFrame();
  TStageObject::KeyframeMap keyframes;
  pegbar->getKeyframes(keyframes);
  TStageObject::KeyframeMap::iterator it;
  for (it = keyframes.begin(); it != keyframes.end(); ++it)
    if (it->first > frame) {
      setCurrentFrame(it->first);
      return;
    }
}

//-------------------------------------------------------------------

void ViewerKeyframeNavigator::goPrev() {
  TStageObject *pegbar = getStageObject();
  if (!pegbar) return;
  int frame = getCurrentFrame();
  TStageObject::KeyframeMap keyframes;
  pegbar->getKeyframes(keyframes);
  TStageObject::KeyframeMap::reverse_iterator it;
  for (it = keyframes.rbegin(); it != keyframes.rend(); ++it)
    if (it->first < frame) {
      setCurrentFrame(it->first);
      return;
    }
}

//-----------------------------------------------------------------------------

void ViewerKeyframeNavigator::showEvent(QShowEvent *e) {
  if (!m_objectHandle) return;
  QObject::connect(m_objectHandle, &TObjectHandle::objectSwitched,  //
                   this, &ViewerKeyframeNavigator::update);
  QObject::connect(m_objectHandle, &TObjectHandle::objectChanged,  //
                   this, &ViewerKeyframeNavigator::update);
  KeyframeNavigator::showEvent(e);
}

//-----------------------------------------------------------------------------

void ViewerKeyframeNavigator::hideEvent(QHideEvent *e) {
  if (!m_objectHandle) return;
  QObject::disconnect(m_objectHandle);
  KeyframeNavigator::hideEvent(e);
}

//=============================================================================
namespace {
//-----------------------------------------------------------------------------

class UndoPaletteSetKeyFrame final : public TUndo {
  int m_frame;
  int m_styleId;
  TPaletteHandle *m_paletteHandle;

public:
  UndoPaletteSetKeyFrame(int styleId, int frame, TPaletteHandle *paletteHandle)
      : m_frame(frame), m_styleId(styleId), m_paletteHandle(paletteHandle) {}

  void undo() const override { setKeyFrame(); }
  void redo() const override { setKeyFrame(); }
  int getSize() const override { return sizeof(*this); }

protected:
  void setKeyFrame() const {
    TPalette *palette = m_paletteHandle->getPalette();
    if (palette->isKeyframe(m_styleId, m_frame))
      palette->clearKeyframe(m_styleId, m_frame);
    else
      palette->setKeyframe(m_styleId, m_frame);
    m_paletteHandle->notifyPaletteChanged();
  }
};
//-----------------------------------------------------------------------------
}  // namespace
//=============================================================================

//=============================================================================
// PaletteKeyframeNavigator
//-----------------------------------------------------------------------------

bool PaletteKeyframeNavigator::hasNext() const {
  TPalette *palette = getPalette();
  if (!palette) return false;
  int styleId = getStyleIndex();
  int frame   = getCurrentFrame();
  int n       = palette->getKeyframeCount(styleId);
  for (int i = n - 1; i >= 0; i--) {
    int f = palette->getKeyframe(styleId, i);
    if (f > frame)
      return true;
    else if (f <= frame)
      return false;
  }
  return false;
}

//-----------------------------------------------------------------------------

bool PaletteKeyframeNavigator::hasPrev() const {
  TPalette *palette = getPalette();
  if (!palette) return false;
  int styleId = getStyleIndex();
  int frame   = getCurrentFrame();
  int n       = palette->getKeyframeCount(styleId);
  for (int i = 0; i < n; i++) {
    int f = palette->getKeyframe(styleId, i);
    if (f < frame)
      return true;
    else if (f >= frame)
      return false;
  }
  return false;
}

//-----------------------------------------------------------------------------

bool PaletteKeyframeNavigator::hasKeyframes() const {
  TPalette *palette = getPalette();
  if (!palette) return false;
  return palette->getKeyframeCount(getStyleIndex()) > 0;
}

//-----------------------------------------------------------------------------

bool PaletteKeyframeNavigator::isKeyframe() const {
  TPalette *palette = getPalette();
  if (!palette) return false;
  int frame = getCurrentFrame();
  return palette->isKeyframe(getStyleIndex(), frame);
}

//-----------------------------------------------------------------------------

void PaletteKeyframeNavigator::toggle() {
  TPalette *palette = getPalette();
  if (!palette) return;

  int styleId = getStyleIndex(), frame = getCurrentFrame();

  std::unique_ptr<UndoPaletteSetKeyFrame> undo(
      new UndoPaletteSetKeyFrame(styleId, frame, m_paletteHandle));
  undo->redo();

  TUndoManager::manager()->add(undo.release());
}

//-----------------------------------------------------------------------------

void PaletteKeyframeNavigator::goNext() {
  TPalette *palette = getPalette();
  if (!palette) return;
  int styleId = getStyleIndex();
  int frame   = getCurrentFrame();
  int n       = palette->getKeyframeCount(styleId);
  for (int i = 0; i < n; i++) {
    int f = palette->getKeyframe(styleId, i);
    if (f > frame) {
      setCurrentFrame(f);
      break;
    }
  }
}

//-----------------------------------------------------------------------------

void PaletteKeyframeNavigator::goPrev() {
  TPalette *palette = getPalette();
  if (!palette) return;
  int styleId = getStyleIndex();
  int frame   = getCurrentFrame();
  int n       = palette->getKeyframeCount(styleId);
  for (int i = n - 1; i >= 0; i--) {
    int f = palette->getKeyframe(styleId, i);
    if (f < frame) {
      setCurrentFrame(f);
      break;
    }
  }
}

//-----------------------------------------------------------------------------

void PaletteKeyframeNavigator::showEvent(QShowEvent *e) {
  if (!m_paletteHandle) return;
  QObject::connect(m_paletteHandle, &TPaletteHandle::paletteSwitched,  //
                   this, &PaletteKeyframeNavigator::update);
  QObject::connect(m_paletteHandle, &TPaletteHandle::paletteChanged,  //
                   this, &PaletteKeyframeNavigator::update);
  QObject::connect(m_paletteHandle, &TPaletteHandle::colorStyleSwitched,  //
                   this, &PaletteKeyframeNavigator::update);
  KeyframeNavigator::showEvent(e);
}

//-----------------------------------------------------------------------------

void PaletteKeyframeNavigator::hideEvent(QHideEvent *e) {
  if (!m_paletteHandle) return;
  QObject::disconnect(m_paletteHandle);
  KeyframeNavigator::hideEvent(e);
}

//=============================================================================
namespace {
//-----------------------------------------------------------------------------

//! Se non c'e' un keyframe successivo ritorna frame
int getNextKeyframe(TFxP fx, int frame) {
  if (!fx) return frame;
  int targetFrame = frame;
  for (int i = 0; i < fx->getParams()->getParamCount(); i++) {
    TParamP param = fx->getParams()->getParam(i);
    int j         = param->getNextKeyframe(frame);
    if (j < 0) continue;
    int f = (int)param->keyframeIndexToFrame(j);
    if (targetFrame == frame || f < targetFrame) targetFrame = f;
  }
  return targetFrame;
}

//-----------------------------------------------------------------------------

//! Se non c'e' un keyframe precedente ritorna frame
int getPrevKeyframe(TFxP fx, int frame) {
  if (!fx) return frame;
  int targetFrame = frame;
  for (int i = 0; i < fx->getParams()->getParamCount(); i++) {
    TParamP param = fx->getParams()->getParam(i);
    int j         = param->getPrevKeyframe(frame);
    if (j < 0) continue;
    int f = (int)param->keyframeIndexToFrame(j);
    if (targetFrame == frame || f > targetFrame) targetFrame = f;
  }
  return targetFrame;
}

//-----------------------------------------------------------------------------

void setKeyframe(TFxP fx, int frame, bool on) {
  if (fx)
    for (int i = 0; i < fx->getParams()->getParamCount(); i++) {
      TParamP param = fx->getParams()->getParam(i);
      if (TDoubleParamP dp = param) {
        if (on)
          dp->setValue(frame, dp->getValue(frame));
        else
          dp->deleteKeyframe(frame);
      }
    }
}

//-----------------------------------------------------------------------------
}  // anonymous namespace
//=============================================================================

//=============================================================================
// FxKeyframeNavigator
//-----------------------------------------------------------------------------

bool FxKeyframeNavigator::hasNext() const {
  TFx *fx = getFx();
  if (!fx)
    return false;
  else
    return getNextKeyframe(fx, getCurrentFrame()) > getCurrentFrame();
}

//-----------------------------------------------------------------------------

bool FxKeyframeNavigator::hasPrev() const {
  TFx *fx = getFx();
  if (!fx)
    return false;
  else
    return getPrevKeyframe(fx, getCurrentFrame()) < getCurrentFrame();
}

//-----------------------------------------------------------------------------

bool FxKeyframeNavigator::hasKeyframes() const {
  TFx *fx = getFx();
  if (!fx) return false;
  for (int i = 0; i < fx->getParams()->getParamCount(); i++) {
    TParamP param = fx->getParams()->getParam(i);
    if (param->hasKeyframes()) return true;
  }
  return false;
}

//-----------------------------------------------------------------------------

bool FxKeyframeNavigator::isKeyframe() const {
  TFx *fx = getFx();
  if (!fx) return false;
  for (int i = 0; i < fx->getParams()->getParamCount(); i++) {
    TParamP param = fx->getParams()->getParam(i);
    if (param->isKeyframe(getCurrentFrame())) return true;
  }
  return false;
}

//-----------------------------------------------------------------------------

bool FxKeyframeNavigator::isFullKeyframe() const {
  TFx *fx = getFx();
  if (!fx) return false;
  int keyFrameCount        = 0;
  int animatableParamCount = 0;
  for (int i = 0; i < fx->getParams()->getParamCount(); i++) {
    TParamP param = fx->getParams()->getParam(i);
    if (param->isAnimatable()) {
      animatableParamCount++;
      if (param->isKeyframe(getCurrentFrame())) keyFrameCount++;
    }
  }
  return animatableParamCount > 0 && keyFrameCount == animatableParamCount;
}

//-----------------------------------------------------------------------------

void FxKeyframeNavigator::toggle() {
  TFx *fx = getFx();
  if (!fx) return;
  int i, paramCount = fx->getParams()->getParamCount();

  // determino in quale caso ci troviamo:
  // il frame corrente non e' keyframe di nessun parametro (isKeyframe=false),
  // di qualche parametro o di tutti i parametri (isFullKeyframe=true)
  bool isFullKeyframe = true;
  bool isKeyframe     = false;
  int frame           = getCurrentFrame();
  for (i = 0; i < paramCount; i++) {
    TParamP param = fx->getParams()->getParam(i);
    if (!param->isAnimatable()) continue;
    if (param->isKeyframe(frame))
      isKeyframe = true;
    else
      isFullKeyframe = false;
  }
  if (!isKeyframe) isFullKeyframe = false;

  // modifico lo stato: nokeyframe->full, full->no, partial->full
  bool on = !isKeyframe || isKeyframe && !isFullKeyframe;
  for (i = 0; i < fx->getParams()->getParamCount();
       i++) {  // TODO. spostare questo codice in TParam
    TParamP param = fx->getParams()->getParam(i);
    if (TDoubleParamP dp = param) {
      if (on)
        dp->setValue(frame, dp->getValue(frame));
      else
        dp->deleteKeyframe(frame);
    } else if (TRangeParamP rp = param) {
      if (on)
        rp->setValue(frame, rp->getValue(frame));
      else
        rp->deleteKeyframe(frame);
    } else if (TPointParamP pp = param) {
      if (on)
        pp->setValue(frame, pp->getValue(frame));
      else
        pp->deleteKeyframe(frame);
    } else if (TPixelParamP pip = param) {
      if (on)
        pip->setValue(frame, pip->getValue(frame));
      else
        pip->deleteKeyframe(frame);
    } else if (TSpectrumParamP sp = param) {
      if (on)
        sp->setValue(frame, sp->getValue(frame), false);
      else
        sp->deleteKeyframe(frame);
    } else if (TToneCurveParamP tcp = param) {
      if (on)
        tcp->setValue(frame, tcp->getValue(frame), false);
      else
        tcp->deleteKeyframe(frame);
    }
  }
  m_fxHandle->notifyFxChanged();
}

//-----------------------------------------------------------------------------

void FxKeyframeNavigator::goNext() {
  TFx *fx = getFx();
  if (!fx) return;
  int frame = getNextKeyframe(fx, getCurrentFrame());
  if (frame > getCurrentFrame()) {
    setCurrentFrame(frame);
    //		m_fxHandle->notifyFxChanged();
  }
}

//-----------------------------------------------------------------------------

void FxKeyframeNavigator::goPrev() {
  TFx *fx = getFx();
  if (!fx) return;
  int frame = getPrevKeyframe(fx, getCurrentFrame());
  if (frame < getCurrentFrame()) {
    setCurrentFrame(frame);
  }
}

//-----------------------------------------------------------------------------

void FxKeyframeNavigator::showEvent(QShowEvent *e) {
  if (!m_fxHandle) return;
  QObject::connect(m_fxHandle, &TFxHandle::fxSwitched,  //
                   this, &FxKeyframeNavigator::update);
  QObject::connect(m_fxHandle, &TFxHandle::fxChanged,  //
                   this, &FxKeyframeNavigator::update);
  KeyframeNavigator::showEvent(e);
}

//-----------------------------------------------------------------------------

void FxKeyframeNavigator::hideEvent(QHideEvent *e) {
  if (!m_fxHandle) return;
  QObject::disconnect(m_fxHandle);
  KeyframeNavigator::hideEvent(e);
}
