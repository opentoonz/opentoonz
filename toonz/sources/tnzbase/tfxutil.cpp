

#include "tfxutil.h"
#include "tbasefx.h"
//#include "timage_io.h"
#include "trasterimage.h"
#include "tdoubleparam.h"
#include "tparamset.h"
#include "tparamcontainer.h"

//-------------------------------------------------------------------

void TFxUtil::setParam(const TFxP &fx, std::string paramName, double value) {
  TDoubleParamP param = TParamP(fx->getParams()->getParam(paramName));
  assert(param);
  param->setDefaultValue(value);
}

//-------------------------------------------------------------------

void TFxUtil::setParam(const TFxP &fx, std::string paramName, TPixel32 color) {
  TPixelParamP param = TParamP(fx->getParams()->getParam(paramName));
  assert(param);
  param->setDefaultValue(color);
}

//-------------------------------------------------------------------

TFxP TFxUtil::makeColorCard(TPixel32 color) {
  TFxP fx = TFx::create("colorCardFx");
  assert(fx);
  setParam(fx, "color", color);
  return fx;
}

//-------------------------------------------------------------------

TFxP TFxUtil::makeCheckboard() {
  return makeCheckboard(TPixel32(255, 200, 200), TPixel32(180, 190, 190), 50);
}
//-------------------------------------------------------------------

DVAPI TFxP TFxUtil::makeCheckboard(TPixel32 c1, TPixel32 c2, double size) {
  TFxP fx = TFx::create("checkBoardFx");
  assert(fx);
  setParam(fx, "color1", c1);
  setParam(fx, "color2", c2);
  setParam(fx, "size", size);
  return fx;
}

//-------------------------------------------------------------------

TFxP TFxUtil::makeOver(const TFxP &dn, const TFxP &up) {
  if (!dn)
    return up;
  else if (!up)
    return dn;

  assert(up && dn);

  TFxP overFx = TFx::create("overFx");
  if (!overFx) {
    assert(overFx);
    return 0;
  }

  if (!overFx->connect("Source1", up.getPointer()) ||
      !overFx->connect("Source2", dn.getPointer()))
    assert(!"Could not connect ports!");

  return overFx;
}

// ------------------------------------------------------------------ -

TFxP TFxUtil::makeDarken(const TFxP &dn, const TFxP &up) {
  if (!dn)
    return up;
  else if (!up)
    return dn;
  assert(dn);
  assert(up);

  /*-- TODO: FxId名変更となる可能性が高い。DarkenFx実装後に修正のこと。2016/2/3
   * shun_iwasawa --*/
  TFxP darkenFx = TFx::create("STD_inoDarkenFx");
  assert(darkenFx);
  if (!darkenFx) return 0;
  darkenFx->connect("Back", dn.getPointer());
  darkenFx->connect("Fore", up.getPointer());
  return darkenFx;
}

//-------------------------------------------------------------------

TFxP TFxUtil::makeAffine(const TFxP &arg, const TAffine &aff) {
  if (aff == TAffine()) return arg;
  if (!arg) return TFxP();

  NaAffineFx *affFx = new NaAffineFx();
  assert(affFx);
  TFxP fx = affFx;
  affFx->setAffine(aff);
  if (!affFx->connect("source", arg.getPointer()))
    assert(!"Could not connect ports!");
  return fx;
}

//-------------------------------------------------------------------

TFxP TFxUtil::makeBlur(const TFxP &arg, double blurValue) {
  TFxP fx = TFx::create("STD_blurFx");
  assert(fx);
  setParam(fx, "value", blurValue);
  if (!fx->connect("Source", arg.getPointer()))
    assert(!"Could not connect ports!");
  return fx;
}

//-------------------------------------------------------------------

TFxP TFxUtil::makeColumnColorFilter(const TFxP &arg, TPixel32 colorScale) {
  ColumnColorFilterFx *colorFilterfx = new ColumnColorFilterFx();
  assert(colorFilterfx);
  colorFilterfx->setColorFilter(colorScale);
  if (!colorFilterfx->connect("source", arg.getPointer()))
    assert(!"Could not connect ports!");
  return colorFilterfx;
}

//-------------------------------------------------------------------

int TFxUtil::getKeyframeStatus(const TFxP &fx, int frame) {
  bool keyframed = false, notKeyframed = false;
  for (int i = 0; i < fx->getParams()->getParamCount(); i++) {
    TParamP param = fx->getParams()->getParam(i);
    if (param->isAnimatable()) {
      if (param->isKeyframe(frame))
        keyframed = true;
      else
        notKeyframed = true;
    }
  }
  if (keyframed == false)
    return TFxUtil::NO_KEYFRAMES;
  else if (notKeyframed == false)
    return TFxUtil::ALL_KEYFRAMES;
  else
    return TFxUtil::SOME_KEYFRAMES;
}

//-------------------------------------------------------------------

void TFxUtil::deleteKeyframes(const TFxP &fx, int frame) {
  for (int i = 0; i < fx->getParams()->getParamCount(); i++)
    fx->getParams()->getParam(i)->deleteKeyframe(frame);
}

//-------------------------------------------------------------------

void TFxUtil::setKeyframe(const TFxP &dstFx, int dstFrame, const TFxP &srcFx,
                          int srcFrame, bool changedOnly) {
  if (dstFx->getFxType() != srcFx->getFxType()) return;
  assert(dstFx->getParams()->getParamCount() ==
         srcFx->getParams()->getParamCount());
  for (int i = 0; i < dstFx->getParams()->getParamCount(); i++) {
    TParamP dstParam = dstFx->getParams()->getParam(i);
    TParamP srcParam = srcFx->getParams()->getParam(i);
    dstParam->assignKeyframe(dstFrame, srcParam, srcFrame, changedOnly);
  }
}
