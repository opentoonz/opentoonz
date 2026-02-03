

#include "tnotanimatableparam.h"
#include "tstream.h"

PERSIST_IDENTIFIER(TIntParam, "intParam")
PERSIST_IDENTIFIER(TBoolParam, "boolParam")
PERSIST_IDENTIFIER(TFilePathParam, "filePathParam")
PERSIST_IDENTIFIER(TStringParam, "stringParam")
PERSIST_IDENTIFIER(TNADoubleParam, "naDoubleParam")
PERSIST_IDENTIFIER(TFontParam, "fontParam")
// PERSIST_IDENTIFIER(TIntEnumParam, "intEnumParam")

TPersistDeclarationT<TEnumParam> TEnumParam::m_declaration("intEnumParam");

//=========================================================

void TIntParam::loadData(TIStream &is) {
  int def, value;
  is >> def;
  if (is.eos()) {
    def += 1;
    setDefaultValue(def);
    setValue(def, false);
    return;
  }
  setDefaultValue(def);
  is >> value;
  setValue(value, false);
}

//---------------------------------------------------------

void TIntParam::saveData(TOStream &os) {
  os << getDefaultValue();
  os << getValue();
}
//---------------------------------------------------------

bool TIntParam::isWheelEnabled() const { return m_isWheelEnabled; }
//---------------------------------------------------------

void TIntParam::enableWheel(bool on) { m_isWheelEnabled = on; }

//=========================================================

void TBoolParam::loadData(TIStream &is) {
  int def, value;
  is >> def >> value;
  setDefaultValue(def ? true : false);
  setValue(value ? true : false, false);
}

//---------------------------------------------------------

void TBoolParam::saveData(TOStream &os) {
  os << (int)getDefaultValue() << (int)getValue();
}

//=========================================================

void TFilePathParam::loadData(TIStream &is) {
  TFilePath def, value;
  is >> def >> value;
  setDefaultValue(def);
  setValue(value, false);
}

//---------------------------------------------------------

void TFilePathParam::saveData(TOStream &os) {
  os << getDefaultValue();
  os << getValue();
}

//=========================================================
void TStringParam::loadData(TIStream &is) {
  std::wstring def, value;
  is >> def >> value;
  setDefaultValue(def);
  setValue(value, false);
}
void TStringParam::saveData(TOStream &os) {
  os << getDefaultValue();
  os << getValue();
}
//=========================================================
void TNADoubleParam::loadData(TIStream &is) {
  double def, value;
  is >> def >> value;
  setDefaultValue(def);
  setValue(value, false);
}
void TNADoubleParam::saveData(TOStream &os) {
  os << getDefaultValue();
  os << getValue();
}

//=========================================================

void TFontParam::loadData(TIStream &is) {
  std::wstring str;
  is >> str;
  setValue(str, false);
}

void TFontParam::saveData(TOStream &os) { os << getValue(); }

//=========================================================

//=========================================================

namespace {
template <typename T>
class matchesValue {
  T m_v;

public:
  matchesValue(T v) : m_v(v) {}
  bool operator()(const std::pair<T, std::string> &p) { return m_v == p.first; }
};
}

//---------------------------------------------------------

class TEnumParamImp {
public:
  std::vector<std::pair<int, std::string>> m_items;
  void copy(std::unique_ptr<TEnumParamImp> &src) {
    m_items.clear();
    std::back_insert_iterator<std::vector<std::pair<int, std::string>>> bii(
        m_items);
    std::copy(src->m_items.begin(), src->m_items.end(), bii);
  }
  bool checkValue(int v) {
    std::vector<std::pair<int, std::string>>::iterator it =
        std::find_if(m_items.begin(), m_items.end(), matchesValue<int>(v));
    return it != m_items.end();
  }
};

//---------------------------------------------------------

TEnumParam::TEnumParam(const int &v, const std::string &caption)
    : TNotAnimatableParam<int>(v), m_imp(new TEnumParamImp()) {
  addItem(v, caption);
}

//---------------------------------------------------------

void TEnumParam::copy(TParam *src) {
  TNotAnimatableParam<int>::copy(src);
  TEnumParam *p = dynamic_cast<TEnumParam *>(src);
  if (!p) throw TException("invalid source for copy");
  TEnumParam::setName(src->getName());
  m_imp->copy(p->m_imp);
}

//---------------------------------------------------------

TEnumParam::~TEnumParam() {}

//---------------------------------------------------------

TEnumParam::TEnumParam(const TEnumParam &src)
    : TNotAnimatableParam<int>(src), m_imp(new TEnumParamImp(*src.m_imp)) {}

//---------------------------------------------------------

TEnumParam::TEnumParam()
    : TNotAnimatableParam<int>()
    , m_imp(new TEnumParamImp())

{}

//---------------------------------------------------------

void TEnumParam::setValue(int v, bool undoing) {
  bool valid = false;
  std::vector<std::pair<int, std::string>>::iterator it =
      m_imp->m_items.begin();
  for (; it != m_imp->m_items.end(); ++it) {
    if (it->first == v) {
      valid = true;
      break;
    }
  }

  if (!valid) throw TException("out of range parameter value");

  TNotAnimatableParam<int>::setValue(v, undoing);
}

//---------------------------------------------------------

void TEnumParam::setValue(const std::string &caption, bool undoing) {
  bool valid = false;
  int v      = 0;
  std::vector<std::pair<int, std::string>>::iterator it =
      m_imp->m_items.begin();
  for (; it != m_imp->m_items.end(); ++it) {
    if (it->second == caption) {
      v     = it->first;
      valid = true;
      break;
    }
  }

  if (!valid) throw TException("out of range parameter value");

  TNotAnimatableParam<int>::setValue(v, undoing);
}

//---------------------------------------------------------

void TEnumParam::addItem(const int &item, const std::string &caption) {
  m_imp->m_items.push_back(std::make_pair(item, caption));
}

//---------------------------------------------------------

int TEnumParam::getItemCount() const { return m_imp->m_items.size(); }

//---------------------------------------------------------

void TEnumParam::getItem(int i, int &item, std::string &caption) const {
  assert(i >= 0 && i < m_imp->m_items.size());
  item    = m_imp->m_items[i].first;
  caption = m_imp->m_items[i].second;
}

//---------------------------------------------------------

void TEnumParam::loadData(TIStream &is) {
  int value;
  is >> value;

  try {
    setValue(value, false);
  } catch (TException &) {
    TNotAnimatableParam<int>::reset();
  }
}

//---------------------------------------------------------

void TEnumParam::saveData(TOStream &os) {
  os << TNotAnimatableParam<int>::getValue();
}

//---------------------------------------------------------

void TIntParam::setValueRange(int min, int max) {
  assert(min < max);

  minValue = min;
  maxValue = max;
}

bool TIntParam::getValueRange(int &min, int &max) const {
  min = minValue;
  max = maxValue;
  return min < max;
}
