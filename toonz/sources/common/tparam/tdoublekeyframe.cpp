

#include "tdoublekeyframe.h"
#include "tstream.h"
#include "tconvert.h"
#include "tunit.h"

TDoubleKeyframe::TDoubleKeyframe(double frame, double value)
    : m_type(Linear)
    , m_frame(frame)
    , m_value(value)
    , m_step(1)
    , m_isKeyframe(false)
    , m_speedIn()
    , m_speedOut()
    , m_linkedHandles(true)
    , m_expressionText("")
    , m_fileParams()
    , m_similarShapeOffset(0)
    , m_unitName("") {}

TDoubleKeyframe::~TDoubleKeyframe() {}

void TDoubleKeyframe::saveData(TOStream &os) const {
  static std::map<TDoubleKeyframe::Type, std::string> typeCodes;
  if (typeCodes.empty()) {
    typeCodes[None]                = "n";
    typeCodes[Constant]            = "C";
    typeCodes[Linear]              = "L";
    typeCodes[Exponential]         = "Exp";
    typeCodes[SpeedInOut]          = "S";
    typeCodes[EaseInOut]           = "E";
    typeCodes[EaseInOutPercentage] = "Ep";
    typeCodes[Expression]          = "Ex";
    typeCodes[File]                = "F";
    typeCodes[SimilarShape]        = "SimShape";
  };
  std::map<std::string, std::string> attr;
  if (!m_linkedHandles) attr["lnk"] = "no";
  if (m_step > 1) attr["step"]      = std::to_string(m_step);
  os.openChild(typeCodes[m_type], attr);
  switch (m_prevType) {
  case Linear:
    os.child("prev") << m_value;
    break;
  case SpeedInOut:
    os.child("prev") << m_value << m_speedIn.x << m_speedIn.y;
    break;
  case EaseInOut:
  case EaseInOutPercentage:
    os.child("prev") << m_value << m_speedIn.x;
    break;
  default:
    break;
  }
  std::string unitName = m_unitName != "" ? m_unitName : "default";
  // Dirty resolution. Because the degree sign is converted to unexpected
  // string...
  if (QString::fromStdWString(L"\u00b0").toStdString() == unitName)
    unitName = "\\u00b0";
  switch (m_type) {
  case Constant:
  case Exponential:
  case Linear:
    os << m_frame << m_value;
    break;
  case SpeedInOut:
    os << m_frame << m_value << m_speedOut.x << m_speedOut.y;
    break;
  case EaseInOut:
  case EaseInOutPercentage:
    os << m_frame << m_value << m_speedOut.x;
    break;
  case Expression:
    os << m_frame << m_expressionText << unitName;
    break;
  case SimilarShape:
    os << m_frame << m_value << m_expressionText << m_similarShapeOffset;
    break;
  case File:
    os << m_frame << m_fileParams.m_path << m_fileParams.m_fieldIndex
       << unitName;
    break;
  default:
    break;
  }
  os.closeChild();
}

void TDoubleKeyframe::loadData(TIStream &is) {
  static std::map<std::string, TDoubleKeyframe::Type> typeCodes;
  if (typeCodes.empty()) {
    typeCodes["n"]        = None;
    typeCodes["C"]        = Constant;
    typeCodes["L"]        = Linear;
    typeCodes["Exp"]      = Exponential;
    typeCodes["S"]        = SpeedInOut;
    typeCodes["E"]        = EaseInOut;
    typeCodes["Ep"]       = EaseInOutPercentage;
    typeCodes["Ex"]       = Expression;
    typeCodes["F"]        = File;
    typeCodes["SimShape"] = SimilarShape;
  };

  std::string tagName;
  if (!is.matchTag(tagName)) return;
  std::map<std::string, TDoubleKeyframe::Type>::iterator it =
      typeCodes.find(tagName);
  if (it == typeCodes.end()) {
    throw TException(tagName + " : unexpected tag");
  }
  m_type = it->second;
  is.getTagParam("step", m_step);
  std::string lnkValue;
  if (is.getTagParam("lnk", lnkValue) && lnkValue == "no")
    m_linkedHandles = false;
  if (is.matchTag(tagName)) {
    if (tagName != "prev") throw TException(tagName + " : unexpected tag");

    is >> m_value;
    if (!is.eos()) {
      is >> m_speedIn.x;
      if (!is.eos()) is >> m_speedIn.y;
    }
    if (!is.matchEndTag()) throw TException(tagName + " : missing endtag");
  }
  double dummy0, dummy1;
  switch (m_type) {
  case Constant:
  case Linear:
  case Exponential:
    is >> m_frame >> m_value;
    break;
  case SpeedInOut:
    is >> m_frame >> m_value >> m_speedOut.x >> m_speedOut.y;
    if (!is.eos())
      is >> dummy0 >>
          dummy1;  // old and wrong format. used only during the 6.0 release
    break;
  case EaseInOut:
  case EaseInOutPercentage:
    is >> m_frame >> m_value >> m_speedOut.x;
    if (!is.eos())
      is >> dummy0;  // old and wrong format. used only during the 6.0 release
    break;
  case Expression:
    is >> m_frame >> m_expressionText >> m_unitName;
    break;
  case SimilarShape:
    is >> m_frame >> m_value >> m_expressionText >> m_similarShapeOffset;
    break;
  case File:
    is >> m_frame >> m_fileParams.m_path >> m_fileParams.m_fieldIndex >>
        m_unitName;
    break;
  default:
    break;
  }
  if (!is.matchEndTag()) throw TException(tagName + " : missing endtag");
  if (m_unitName == "default") m_unitName = "";
  m_isKeyframe                            = true;
}
