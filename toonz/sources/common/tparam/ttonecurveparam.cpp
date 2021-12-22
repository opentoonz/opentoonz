

#include "ttonecurveparam.h"
#include "texception.h"
#include "tstream.h"

#include <QList>

//=========================================================

PERSIST_IDENTIFIER(TToneCurveParam, "toneCurveParam")

//---------------------------------------------------------

TToneCurveParam::TToneCurveParam() : TParam() {
  m_toneChannel = RGBA;

  std::vector<TPointD> points;
  // Inserisco dei punti fuori dal range(0-255) perche' mi consentono di gestire
  // i primi punti come speciali.
  points.push_back(TPointD(-40, 0));
  points.push_back(TPointD(-20, 0));
  points.push_back(TPointD(-20, 0));
  points.push_back(TPointD(0, 0));
  points.push_back(TPointD(16, 16));
  points.push_back(TPointD(239, 239));
  points.push_back(TPointD(255, 255));
  points.push_back(TPointD(275, 255));
  points.push_back(TPointD(275, 255));
  points.push_back(TPointD(295, 255));
  m_rgbaParamSet = new TParamSet("redgreenbluealphachannel");
  m_rgbParamSet  = new TParamSet("redgreenbluechannel");
  m_rParamSet    = new TParamSet("redchannel");
  m_gParamSet    = new TParamSet("greenchannel");
  m_bParamSet    = new TParamSet("bluechannel");
  m_aParamSet    = new TParamSet("alphachannel");

  m_isLinear = new TBoolParam(false);

  int i;
  for (i = 0; i < (int)points.size(); i++) {
    m_rgbaParamSet->addParam(new TPointParam(points[i]), "point");
    m_rgbParamSet->addParam(new TPointParam(points[i]), "point");
    m_rParamSet->addParam(new TPointParam(points[i]), "point");
    m_gParamSet->addParam(new TPointParam(points[i]), "point");
    m_bParamSet->addParam(new TPointParam(points[i]), "point");
    m_aParamSet->addParam(new TPointParam(points[i]), "point");
  }
}

//---------------------------------------------------------

static TParamSetP getClonedParamSet(TParamSetP srcParamSet) {
  TParamSetP dstParamSet = new TParamSet(srcParamSet->getName());
  int i;
  for (i = 0; i < srcParamSet->getParamCount(); i++) {
    TParamP param = srcParamSet->getParam(i);
    dstParamSet->addParam(param->clone(), param->getName());
  }
  return dstParamSet;
}

//---------------------------------------------------------

TToneCurveParam::TToneCurveParam(const TToneCurveParam &src)
    : TParam(src.getName()) {
  m_rgbaParamSet = getClonedParamSet(src.getParamSet(RGBA));
  m_rgbParamSet  = getClonedParamSet(src.getParamSet(RGB));
  m_rParamSet    = getClonedParamSet(src.getParamSet(Red));
  m_gParamSet    = getClonedParamSet(src.getParamSet(Green));
  m_bParamSet    = getClonedParamSet(src.getParamSet(Blue));
  m_aParamSet    = getClonedParamSet(src.getParamSet(Alpha));
  m_toneChannel  = src.getCurrentChannel();
  m_isLinear     = src.getIsLinearParam()->clone();
}

//---------------------------------------------------------

void TToneCurveParam::copy(TParam *src) {
  TToneCurveParam *p = dynamic_cast<TToneCurveParam *>(src);
  if (!p) throw TException("invalid source for copy");
  setName(src->getName());
  m_rgbaParamSet->copy(p->getParamSet(RGBA).getPointer());
  m_rgbParamSet->copy(p->getParamSet(RGB).getPointer());
  m_rParamSet->copy(p->getParamSet(Red).getPointer());
  m_gParamSet->copy(p->getParamSet(Green).getPointer());
  m_bParamSet->copy(p->getParamSet(Blue).getPointer());
  m_aParamSet->copy(p->getParamSet(Alpha).getPointer());
  m_isLinear->copy(p->getIsLinearParam().getPointer());
  m_toneChannel = p->getCurrentChannel();
}

//---------------------------------------------------------

void TToneCurveParam::addObserver(TParamObserver *observer) {
  m_rgbaParamSet->addObserver(observer);
  m_rgbParamSet->addObserver(observer);
  m_rParamSet->addObserver(observer);
  m_gParamSet->addObserver(observer);
  m_bParamSet->addObserver(observer);
  m_aParamSet->addObserver(observer);
  m_isLinear->addObserver(observer);
}

//---------------------------------------------------------

void TToneCurveParam::removeObserver(TParamObserver *observer) {
  m_rgbaParamSet->removeObserver(observer);
  m_rgbParamSet->removeObserver(observer);
  m_rParamSet->removeObserver(observer);
  m_gParamSet->removeObserver(observer);
  m_bParamSet->removeObserver(observer);
  m_aParamSet->removeObserver(observer);
  m_isLinear->removeObserver(observer);
}

//---------------------------------------------------------

TParamSetP TToneCurveParam::getParamSet(ToneChannel channel) const {
  if (channel == RGBA)
    return m_rgbaParamSet;
  else if (channel == RGB)
    return m_rgbParamSet;
  else if (channel == Red)
    return m_rParamSet;
  else if (channel == Green)
    return m_gParamSet;
  else if (channel == Blue)
    return m_bParamSet;
  else if (channel == Alpha)
    return m_aParamSet;

  return 0;
}

//---------------------------------------------------------

TParamSetP TToneCurveParam::getCurrentParamSet() const {
  return getParamSet(m_toneChannel);
}

//---------------------------------------------------------

void TToneCurveParam::setCurrentChannel(ToneChannel channel) {
  m_toneChannel = channel;
}

//---------------------------------------------------------

QList<TPointD> TToneCurveParam::getValue(double frame) const {
  // compute the handle angle and length
  // in case the handle length is 0 on one side, take the oppositte handle to
  // calculate the angle
  auto handleAngleLength = [](TPointParamP handle, TPointParamP cp,
                              TPointParamP opposite, double f,
                              bool isLeft = true) {
    TPointD vec_h_cp = handle->getValue(f) - cp->getValue(f);
    double angle;
    if (vec_h_cp.x == 0 && vec_h_cp.y == 0) {
      TPointD vec_h_op = handle->getValue(f) - opposite->getValue(f);
      angle            = std::atan2(vec_h_op.y, vec_h_op.x);
    } else
      angle = std::atan2(vec_h_cp.y, vec_h_cp.x);

    // make the angle continuous
    if (isLeft && angle < 0) angle += M_2PI;
    double length =
        std::sqrt(vec_h_cp.x * vec_h_cp.x + vec_h_cp.y * vec_h_cp.y);
    return TPointD(angle, length);
  };
  auto angleLengthToPos = [](TPointD anLen) {
    return TPointD(anLen.y * std::cos(anLen.x), anLen.y * std::sin(anLen.x));
  };

  std::set<double> frames;
  getCurrentParamSet()->getKeyframes(frames);
  std::set<double>::iterator prevIt = frames.lower_bound(frame);
  std::set<double>::iterator nextIt = frames.upper_bound(frame);
  bool isNotInSegment               = getCurrentParamSet()->isKeyframe(frame) ||
                        prevIt == frames.begin() || nextIt == frames.end();
  if (prevIt != frames.begin()) prevIt--;

  int i;
  QList<TPointD> points;
  int pointCount = getCurrentParamSet()->getParamCount();
  for (i = 0; i < pointCount; i++) {
    // control point case or the current frame is not between the keys
    if (i % 3 == 0 || isNotInSegment) {
      TPointParamP pointParam = getCurrentParamSet()->getParam(i);
      points.push_back(pointParam->getValue(frame));
    } else {
      double prevF = (*prevIt);
      double nextF = (*nextIt);
      double ratio = (frame - prevF) / (nextF - prevF);
      if (i % 3 == 2) {  // left handle
        TPointParamP left_Param  = getCurrentParamSet()->getParam(i);
        TPointParamP cp_Param    = getCurrentParamSet()->getParam(i + 1);
        TPointParamP right_Param = (i == pointCount - 2)
                                       ? cp_Param
                                       : TPointParamP(getCurrentParamSet()->getParam(i + 2));

        TPointD prevAnLen =
            handleAngleLength(left_Param, cp_Param, right_Param, prevF);
        TPointD nextAnLen =
            handleAngleLength(left_Param, cp_Param, right_Param, nextF);
        // linear interpolation of angle & length
        TPointD handle =
            angleLengthToPos(prevAnLen * (1.0 - ratio) + nextAnLen * (ratio));
        points.push_back(cp_Param->getValue(frame) + handle);
      } else {  // right handle
        TPointParamP right_Param = getCurrentParamSet()->getParam(i);
        TPointParamP cp_Param    = getCurrentParamSet()->getParam(i - 1);
        TPointParamP left_Param =
            (i == 1) ? cp_Param : TPointParamP(getCurrentParamSet()->getParam(i - 2));

        TPointD prevAnLen =
            handleAngleLength(right_Param, cp_Param, left_Param, prevF, false);
        TPointD nextAnLen =
            handleAngleLength(right_Param, cp_Param, left_Param, nextF, false);
        // linear interpolation of angle & length
        TPointD handle =
            angleLengthToPos(prevAnLen * (1.0 - ratio) + nextAnLen * (ratio));
        points.push_back(cp_Param->getValue(frame) + handle);
      }
    }
  }
  return points;
}

//---------------------------------------------------------

void TToneCurveParam::setValue(double frame, const QList<TPointD> &value,
                               bool undoing) {
  if (value.size() == 0) return;
  int paramCount = getCurrentParamSet()->getParamCount();
  assert(paramCount == value.size());
  int i = 0;
  for (i = 0; i < paramCount; i++) {
    TPointParamP param = getCurrentParamSet()->getParam(i);
    TPointD point      = value.at(i);
    param->setValue(frame, point);
  }
}

//---------------------------------------------------------

bool TToneCurveParam::isLinear() const { return m_isLinear->getValue(); }

//---------------------------------------------------------

void TToneCurveParam::setIsLinear(bool isLinear) {
  m_isLinear->setValue(isLinear);
}

//---------------------------------------------------------

void TToneCurveParam::addValue(double frame, const QList<TPointD> &value,
                               int index) {
  getCurrentParamSet()->insertParam(new TPointParam(value.at(index - 1)),
                                    "point", index - 1);
  getCurrentParamSet()->insertParam(new TPointParam(value.at(index)), "point",
                                    index);
  getCurrentParamSet()->insertParam(new TPointParam(value.at(index + 1)),
                                    "point", index + 1);
}

//---------------------------------------------------------

void TToneCurveParam::removeValue(double frame, int index) {
  getCurrentParamSet()->removeParam(getCurrentParamSet()->getParam(index - 1));
  getCurrentParamSet()->removeParam(getCurrentParamSet()->getParam(index - 1));
  getCurrentParamSet()->removeParam(getCurrentParamSet()->getParam(index - 1));
}

//---------------------------------------------------------

void TToneCurveParam::setDefaultValue(const QList<TPointD> &value) {
  int pointCount = value.size();
  if (pointCount == 0) return;

  int paramCount = getCurrentParamSet()->getParamCount();
  assert(paramCount == pointCount);

  int i;
  for (i = 0; i < pointCount; i++) {
    TPointParamP param = getCurrentParamSet()->getParam(i);
    TPointD paramPoint(param->getValue(0));
    TPointD strokePoint(value.at(i));
    param->setDefaultValue(strokePoint);
  }
  m_isLinear->setDefaultValue(false);
}

//---------------------------------------------------------

std::string TToneCurveParam::getValueAlias(double frame, int precision) {
  return getCurrentParamSet()->getValueAlias(frame, precision) +
         m_isLinear->getValueAlias(frame, precision);
}

//---------------------------------------------------------

bool TToneCurveParam::isKeyframe(double frame) const {
  if (m_rgbaParamSet->isKeyframe(frame) || m_rgbParamSet->isKeyframe(frame) ||
      m_rParamSet->isKeyframe(frame) || m_gParamSet->isKeyframe(frame) ||
      m_bParamSet->isKeyframe(frame) || m_aParamSet->isKeyframe(frame))
    return true;
  return false;
}

//---------------------------------------------------------

void TToneCurveParam::deleteKeyframe(double frame) {
  m_rgbaParamSet->deleteKeyframe(frame);
  m_rgbParamSet->deleteKeyframe(frame);
  m_rParamSet->deleteKeyframe(frame);
  m_gParamSet->deleteKeyframe(frame);
  m_bParamSet->deleteKeyframe(frame);
  m_aParamSet->deleteKeyframe(frame);
}

//---------------------------------------------------------

void TToneCurveParam::clearKeyframes() {
  m_rgbaParamSet->clearKeyframes();
  m_rgbParamSet->clearKeyframes();
  m_rParamSet->clearKeyframes();
  m_gParamSet->clearKeyframes();
  m_bParamSet->clearKeyframes();
  m_aParamSet->clearKeyframes();
}

//---------------------------------------------------------

void TToneCurveParam::assignKeyframe(double frame,
                                     const TSmartPointerT<TParam> &src,
                                     double srcFrame, bool changedOnly) {
  m_rgbaParamSet->assignKeyframe(frame, src, srcFrame, changedOnly);
  m_rgbParamSet->assignKeyframe(frame, src, srcFrame, changedOnly);
  m_rParamSet->assignKeyframe(frame, src, srcFrame, changedOnly);
  m_gParamSet->assignKeyframe(frame, src, srcFrame, changedOnly);
  m_bParamSet->assignKeyframe(frame, src, srcFrame, changedOnly);
  m_aParamSet->assignKeyframe(frame, src, srcFrame, changedOnly);
}

//---------------------------------------------------------

void TToneCurveParam::getKeyframes(std::set<double> &frames) const {
  m_rgbaParamSet->getKeyframes(frames);
  m_rgbParamSet->getKeyframes(frames);
  m_rParamSet->getKeyframes(frames);
  m_gParamSet->getKeyframes(frames);
  m_bParamSet->getKeyframes(frames);
  m_aParamSet->getKeyframes(frames);
}

//---------------------------------------------------------

bool TToneCurveParam::hasKeyframes() const {
  if (m_rgbaParamSet->hasKeyframes() || m_rgbParamSet->hasKeyframes() ||
      m_rParamSet->hasKeyframes() || m_gParamSet->hasKeyframes() ||
      m_bParamSet->hasKeyframes() || m_aParamSet->hasKeyframes())
    return true;
  return false;
}

//---------------------------------------------------------

int TToneCurveParam::getNextKeyframe(double frame) const {
  std::set<double> frames;
  getKeyframes(frames);
  std::set<double>::iterator it = frames.upper_bound(frame);
  if (it == frames.end())
    return -1;
  else
    return std::distance(frames.begin(), it);
}

//---------------------------------------------------------

int TToneCurveParam::getPrevKeyframe(double frame) const {
  std::set<double> frames;
  getKeyframes(frames);
  std::set<double>::iterator it = frames.lower_bound(frame);
  if (it == frames.begin())
    return -1;
  else {
    --it;
    return std::distance(frames.begin(), it);
  }
}

//---------------------------------------------------------

double TToneCurveParam::keyframeIndexToFrame(int index) const {
  std::set<double> frames;
  getKeyframes(frames);
  assert(0 <= index && index < (int)frames.size());
  std::set<double>::const_iterator it = frames.begin();
  std::advance(it, index);
  return *it;
}

//---------------------------------------------------------

void TToneCurveParam::loadData(TIStream &is) {
  std::string tagName;
  is.openChild(tagName);
  assert(tagName == "tonecurve");
  m_rgbaParamSet->removeAllParam();
  m_rgbaParamSet->loadData(is);
  m_rgbParamSet->removeAllParam();
  m_rgbParamSet->loadData(is);
  m_rParamSet->removeAllParam();
  m_rParamSet->loadData(is);
  m_gParamSet->removeAllParam();
  m_gParamSet->loadData(is);
  m_bParamSet->removeAllParam();
  m_bParamSet->loadData(is);
  m_aParamSet->removeAllParam();
  m_aParamSet->loadData(is);
  is.openChild(tagName);
  m_isLinear->loadData(is);
  is.closeChild();
  is.closeChild();
}

//---------------------------------------------------------

void TToneCurveParam::saveData(TOStream &os) {
  os.openChild("tonecurve");
  m_rgbaParamSet->saveData(os);
  m_rgbParamSet->saveData(os);
  m_rParamSet->saveData(os);
  m_gParamSet->saveData(os);
  m_bParamSet->saveData(os);
  m_aParamSet->saveData(os);
  os.openChild("isLineaer");
  m_isLinear->saveData(os);
  os.closeChild();
  os.closeChild();
}
