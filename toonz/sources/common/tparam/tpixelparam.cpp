

//#include "tpixelparam.h"
#include "tparamset.h"
#include "tdoubleparam.h"
#include "texception.h"
#include "tpixelutils.h"
#include "tstream.h"

class TPixelParamImp {
public:
  TPixelParamImp(const TPixel32 &p)
      : m_r(new TDoubleParam(p.r / 255.0))
      , m_g(new TDoubleParam(p.g / 255.0))
      , m_b(new TDoubleParam(p.b / 255.0))
      , m_m(new TDoubleParam(p.m / 255.0))
      , m_isMatteEnabled(true) {}
  TPixelParamImp(const TPixelParamImp &src)
      : m_r(src.m_r->clone())
      , m_g(src.m_g->clone())
      , m_b(src.m_b->clone())
      , m_m(src.m_m->clone())
      , m_isMatteEnabled(src.m_isMatteEnabled) {}
  ~TPixelParamImp() {}
  TDoubleParamP m_r, m_g, m_b, m_m;
  bool m_isMatteEnabled;
};

PERSIST_IDENTIFIER(TPixelParam, "pixelParam")

//---------------------------------------------------------

TPixelParam::TPixelParam(const TPixel32 &p) : m_data(new TPixelParamImp(p)) {
  addParam(m_data->m_r, "Red");
  addParam(m_data->m_g, "Green");
  addParam(m_data->m_b, "Blue");
  addParam(m_data->m_m, "Alpha");
  std::string measureName("colorChannel");
  m_data->m_r->setMeasureName(measureName);
  m_data->m_g->setMeasureName(measureName);
  m_data->m_b->setMeasureName(measureName);
  m_data->m_m->setMeasureName(measureName);
}

//---------------------------------------------------------

TPixelParam::TPixelParam(const TPixelParam &src)
    : TParamSet(src.getName()), m_data(new TPixelParamImp(*src.m_data)) {
  addParam(m_data->m_r, "Red");
  addParam(m_data->m_g, "Green");
  addParam(m_data->m_b, "Blue");
  addParam(m_data->m_m, "Alpha");
  std::string measureName("colorChannel");
  m_data->m_r->setMeasureName(measureName);
  m_data->m_g->setMeasureName(measureName);
  m_data->m_b->setMeasureName(measureName);
  m_data->m_m->setMeasureName(measureName);
}

//---------------------------------------------------------

void TPixelParam::copy(TParam *src) {
  TPixelParam *p = dynamic_cast<TPixelParam *>(src);
  if (!p) throw TException("invalid source for copy");
  setName(src->getName());
  m_data->m_r->copy(p->m_data->m_r.getPointer());
  m_data->m_g->copy(p->m_data->m_g.getPointer());
  m_data->m_b->copy(p->m_data->m_b.getPointer());
  m_data->m_m->copy(p->m_data->m_m.getPointer());
  std::string measureName("colorChannel");

  m_data->m_r->setMeasureName(measureName);
  m_data->m_g->setMeasureName(measureName);
  m_data->m_b->setMeasureName(measureName);
  m_data->m_m->setMeasureName(measureName);
}

//---------------------------------------------------------

TPixelParam::~TPixelParam() { delete m_data; }

//---------------------------------------------------------

TPixel32 TPixelParam::getDefaultValue() const {
  TPixelD pixd(m_data->m_r->getDefaultValue(), m_data->m_g->getDefaultValue(),
               m_data->m_b->getDefaultValue(), m_data->m_m->getDefaultValue());
  return toPixel32(pixd);
}

//---------------------------------------------------------

void TPixelParam::setDefaultValue(const TPixel32 &p) {
  TPixelD pixd = toPixelD(p);
  m_data->m_r->setDefaultValue(pixd.r);
  m_data->m_g->setDefaultValue(pixd.g);
  m_data->m_b->setDefaultValue(pixd.b);
  m_data->m_m->setDefaultValue(pixd.m);
}

//---------------------------------------------------------

TPixelD TPixelParam::getValueD(double frame) const {
  return TPixelD(m_data->m_r->getValue(frame), m_data->m_g->getValue(frame),
                 m_data->m_b->getValue(frame), m_data->m_m->getValue(frame));
}

//---------------------------------------------------------

TPixel32 TPixelParam::getValue(double frame) const {
  return toPixel32(getValueD(frame));
}

//---------------------------------------------------------

TPixel64 TPixelParam::getValue64(double frame) const {
  return toPixel64(getValueD(frame));
}

//---------------------------------------------------------

TPixel32 TPixelParam::getPremultipliedValue(double frame) const {
  return premultiply(getValue(frame));
}

//---------------------------------------------------------

bool TPixelParam::setValueD(double frame, const TPixelD &p) {
  beginParameterChange();
  m_data->m_r->setValue(frame, p.r);
  m_data->m_g->setValue(frame, p.g);
  m_data->m_b->setValue(frame, p.b);
  m_data->m_m->setValue(frame, p.m);
  endParameterChange();
  return true;
}

//---------------------------------------------------------

bool TPixelParam::setValue(double frame, const TPixel32 &pix) {
  return setValueD(frame, toPixelD(pix));
}

//---------------------------------------------------------

bool TPixelParam::setValue64(double frame, const TPixel64 &pix) {
  return setValueD(frame, toPixelD(pix));
}

//---------------------------------------------------------

void TPixelParam::loadData(TIStream &is) {
  std::string childName;
  while (is.openChild(childName)) {
    if (childName == "red")
      m_data->m_r->loadData(is);
    else if (childName == "green")
      m_data->m_g->loadData(is);
    else if (childName == "blue")
      m_data->m_b->loadData(is);
    else if (childName == "matte")
      m_data->m_m->loadData(is);
    else
      throw TException("unknown channel name: " + childName);
    is.closeChild();
  }
}

//---------------------------------------------------------

void TPixelParam::saveData(TOStream &os) {
  os.openChild("red");
  m_data->m_r->saveData(os);
  os.closeChild();
  os.openChild("green");
  m_data->m_g->saveData(os);
  os.closeChild();
  os.openChild("blue");
  m_data->m_b->saveData(os);
  os.closeChild();
  os.openChild("matte");
  m_data->m_m->saveData(os);
  os.closeChild();
}
//---------------------------------------------------------

TDoubleParamP &TPixelParam::getRed() { return m_data->m_r; }

//---------------------------------------------------------

TDoubleParamP &TPixelParam::getGreen() { return m_data->m_g; }

//---------------------------------------------------------

TDoubleParamP &TPixelParam::getBlue() { return m_data->m_b; }

//---------------------------------------------------------

TDoubleParamP &TPixelParam::getMatte() { return m_data->m_m; }

//---------------------------------------------------------

void TPixelParam::enableMatte(bool on) {
  m_data->m_isMatteEnabled     = on;
  if (on == false) m_data->m_m = new TDoubleParam(255.0);
}
//---------------------------------------------------------

bool TPixelParam::isMatteEnabled() const { return m_data->m_isMatteEnabled; }
