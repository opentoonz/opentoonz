#include "tparamset.h"
#include "tgeometry.h"
#include "tdoubleparam.h"
#include "tstream.h"

class TThickPointParamImp final {
  TDoubleParamP m_x, m_y, m_thickness;

public:
  TThickPointParamImp(const TThickPoint &p);
  TThickPointParamImp(const TThickPointParamImp &src);
  ~TThickPointParamImp() {}

  friend class TThickPointParam;
};

TThickPointParamImp::TThickPointParamImp(const TThickPoint &p)
    : m_x(new TDoubleParam(p.x))
    , m_y(new TDoubleParam(p.y))
    , m_thickness(new TDoubleParam(p.thick)) {}

TThickPointParamImp::TThickPointParamImp(const TThickPointParamImp &src)
    : m_x(src.m_x->clone())
    , m_y(src.m_y->clone())
    , m_thickness(src.m_thickness->clone()) {}

//-----------------------------------------------------------------------------

PERSIST_IDENTIFIER(TThickPointParam, "thickPointParam")

TThickPointParam::TThickPointParam(const TThickPoint &p) {
  m_data = new TThickPointParamImp(p);
  addParam(m_data->m_x, "x");
  addParam(m_data->m_y, "y");
  addParam(m_data->m_thickness, "thickness");
}

TThickPointParam::TThickPointParam(const TThickPointParam &src) {
  m_data = new TThickPointParamImp(*src.m_data);
  addParam(m_data->m_x, "x");
  addParam(m_data->m_y, "y");
  addParam(m_data->m_thickness, "thickness");
}

TThickPointParam::~TThickPointParam() { delete m_data; }

void TThickPointParam::copy(TParam *src) {
  TThickPointParam *p = dynamic_cast<TThickPointParam *>(src);
  if (!p) throw TException("invalid source for copy");
  setName(src->getName());
  m_data->m_x->copy(p->m_data->m_x.getPointer());
  m_data->m_y->copy(p->m_data->m_y.getPointer());
  m_data->m_thickness->copy(p->m_data->m_thickness.getPointer());
}

TThickPoint TThickPointParam::getDefaultValue() const {
  return TThickPoint(m_data->m_x->getDefaultValue(),
                     m_data->m_y->getDefaultValue(),
                     m_data->m_thickness->getDefaultValue());
}
TThickPoint TThickPointParam::getValue(double frame) const {
  return TThickPoint(m_data->m_x->getValue(frame), m_data->m_y->getValue(frame),
                     m_data->m_thickness->getValue(frame));
}
void TThickPointParam::setValue(double frame, const TPointD &p) {
  m_data->m_x->setValue(frame, p.x);
  m_data->m_y->setValue(frame, p.y);
}
void TThickPointParam::setValue(double frame, const TThickPoint &p) {
  this->setValue(frame, (const TPointD &)p);
  m_data->m_thickness->setValue(frame, p.thick);
}
void TThickPointParam::setDefaultValue(const TThickPoint &p) {
  m_data->m_x->setDefaultValue(p.x);
  m_data->m_y->setDefaultValue(p.y);
  m_data->m_thickness->setDefaultValue(p.thick);
}

void TThickPointParam::loadData(TIStream &is) {
  std::string childName;
  while (is.openChild(childName)) {
    if (childName == "x")
      m_data->m_x->loadData(is);
    else if (childName == "y")
      m_data->m_y->loadData(is);
    else if (childName == "thickness")
      m_data->m_thickness->loadData(is);
    else
      throw TException("unknown coord");
    is.closeChild();
  }
}
void TThickPointParam::saveData(TOStream &os) {
  os.openChild("x");
  m_data->m_x->saveData(os);
  os.closeChild();

  os.openChild("y");
  m_data->m_y->saveData(os);
  os.closeChild();

  os.openChild("thickness");
  m_data->m_thickness->saveData(os);
  os.closeChild();
}

TDoubleParamP &TThickPointParam::getX() { return m_data->m_x; }
TDoubleParamP &TThickPointParam::getY() { return m_data->m_y; }
TDoubleParamP &TThickPointParam::getThickness() { return m_data->m_thickness; }
