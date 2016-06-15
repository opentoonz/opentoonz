

//#include "tpointparam.h"
#include "tparamset.h"
#include "tdoubleparam.h"
#include "texception.h"
#include "tstream.h"

//=========================================================

class TPointParamImp {
public:
  TPointParamImp(const TPointD &p)
      : m_x(new TDoubleParam(p.x)), m_y(new TDoubleParam(p.y)) {}
  TPointParamImp(const TPointParamImp &src)
      : m_x(src.m_x->clone()), m_y(src.m_y->clone()) {}
  ~TPointParamImp() {}
  TDoubleParamP m_x, m_y;
};

//---------------------------------------------------------

PERSIST_IDENTIFIER(TPointParam, "pointParam")

TPointParam::TPointParam(const TPointD &p, bool from_plugin)
    : m_data(new TPointParamImp(p)), m_from_plugin(from_plugin) {
  addParam(m_data->m_x, "x");
  addParam(m_data->m_y, "y");
}

//---------------------------------------------------------

TPointParam::TPointParam(const TPointParam &src)
    : TParamSet(src.getName())
    , m_data(new TPointParamImp(*src.m_data))
    , m_from_plugin(src.m_from_plugin) {
  addParam(m_data->m_x, "x");
  addParam(m_data->m_y, "y");
}

//---------------------------------------------------------

TPointParam::~TPointParam() { delete m_data; }

//---------------------------------------------------------

void TPointParam::copy(TParam *src) {
  TPointParam *p = dynamic_cast<TPointParam *>(src);
  if (!p) throw TException("invalid source for copy");
  setName(src->getName());
  m_data->m_x->copy(p->m_data->m_x.getPointer());
  m_data->m_y->copy(p->m_data->m_y.getPointer());
}

//---------------------------------------------------------

TPointD TPointParam::getDefaultValue() const {
  return TPointD(m_data->m_x->getDefaultValue(),
                 m_data->m_y->getDefaultValue());
}

//---------------------------------------------------------

TPointD TPointParam::getValue(double frame) const {
  return TPointD(m_data->m_x->getValue(frame), m_data->m_y->getValue(frame));
}

//---------------------------------------------------------

bool TPointParam::setValue(double frame, const TPointD &p) {
  beginParameterChange();
  m_data->m_x->setValue(frame, p.x);
  m_data->m_y->setValue(frame, p.y);
  endParameterChange();
  return true;
}

//---------------------------------------------------------

void TPointParam::setDefaultValue(const TPointD &p) {
  m_data->m_x->setDefaultValue(p.x);
  m_data->m_y->setDefaultValue(p.y);
}

//---------------------------------------------------------

void TPointParam::loadData(TIStream &is) {
  std::string childName;
  while (is.openChild(childName)) {
    if (childName == "x")
      m_data->m_x->loadData(is);
    else if (childName == "y")
      m_data->m_y->loadData(is);
    else
      throw TException("unknown coord");
    is.closeChild();
  }
}

//---------------------------------------------------------

void TPointParam::saveData(TOStream &os) {
  os.openChild("x");
  m_data->m_x->saveData(os);
  os.closeChild();
  os.openChild("y");
  m_data->m_y->saveData(os);
  os.closeChild();
}

//---------------------------------------------------------

TDoubleParamP &TPointParam::getX() { return m_data->m_x; }

//---------------------------------------------------------

TDoubleParamP &TPointParam::getY() { return m_data->m_y; }
